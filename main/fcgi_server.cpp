#include "settings.h"

#include <cerrno>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

#include <boost/ref.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/checked_delete.hpp>
#include <boost/current_function.hpp>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <sys/time.h>

#include "endpoint.h"
#include "fcgi_request.h"
#include "fcgi_server.h"

#include "fastcgi2/util.h"
#include "fastcgi2/config.h"
#include "fastcgi2/except.h"
#include "fastcgi2/logger.h"
#include "fastcgi2/stream.h"
#include "fastcgi2/request.h"
#include "fastcgi2/handler.h"
#include "fastcgi2/component.h"
#include "fastcgi2/request_io_stream.h"

#include "details/componentset.h"
#include "details/globals.h"
#include "details/handler_context.h"
#include "details/handlerset.h"
#include "details/loader.h"
#include "details/request_cache.h"
#include "details/request_thread_pool.h"
#include "details/thread_pool.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace fastcgi
{

FCGIServer::FCGIServer(boost::shared_ptr<Globals> globals) :
	globals_(globals), stopper_(new ServerStopper()), active_thread_holder_(new char(0)),
	monitorSocket_(-1), request_cache_(NULL), time_statistics_(NULL), status_(NOT_INITED)
{}

FCGIServer::~FCGIServer() {
	close(stopPipes_[0]);
	close(stopPipes_[1]);

	if (-1 != monitorSocket_) {
		close(monitorSocket_);
	}
}

const Globals*
FCGIServer::globals() const {
	return globals_.get();
}

Logger*
FCGIServer::logger() const {
	return globals_->logger();
}

FCGIServer::Status
FCGIServer::status() const {
	boost::mutex::scoped_lock l(statusInfoMutex_);
	return status_;
}

void
FCGIServer::start() {
	if (status() != NOT_INITED) {
		throw std::runtime_error("Server is already started");
	}

	status_ = LOADING;

	logTimes_ = globals_->config()->asInt("/fastcgi/daemon/log-times", 0);

	initMonitorThread();

	initRequestCache();
	initTimeStatistics();
	initFastCGISubsystem();

	createWorkThreads();

	{
		boost::mutex::scoped_lock l(statusInfoMutex_);
		status_ = RUNNING;
	}

	if (-1 == pipe(stopPipes_)) {
		throw std::runtime_error("Cannot create stop signal pipes");
	}
	stopThread_.reset(new boost::thread(boost::bind(&FCGIServer::stopThreadFunction, this)));
}

void
FCGIServer::stopThreadFunction() {
	while (true) {
		char c;
		if (1 == read(stopPipes_[0], &c, 1) && 's' == c) {
			break;
		}
	}
	stopInternal();
}

void
FCGIServer::stop() {
	write(stopPipes_[1], "s", 1);
}

void
FCGIServer::join() {
	if (NOT_INITED == status()) {
		throw std::runtime_error("Server is not started yet");
	}
	globals_->joinThreadPools();

	while (!active_thread_holder_.unique()) {
		usleep(10000);
	}
}

void
FCGIServer::stopInternal() {
	Status stat = status();
	if (stat == NOT_INITED) {
		throw std::runtime_error("Cannot stop server because it is not started yet");
	}
	else if (stat == LOADING) {
		throw std::runtime_error("Cannot stop until loading finish");
	}
	else if (stopper_->stopped()) {
		throw std::runtime_error("Server is already stopping");
	}

	stopper_->stopped(true);

	FCGX_ShutdownPending();
	globals_->stopThreadPools();
}

void
FCGIServer::initMonitorThread() {
	monitorSocket_ = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == monitorSocket_) {
		throw std::runtime_error("Cannot create monitor socket");
	}

	int one = 1;
	if (-1 == setsockopt (monitorSocket_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one))) {
		throw std::runtime_error("Cannot reuse monitor port: " + boost::lexical_cast<std::string>(errno));
	}

	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(globals_->config()->asInt("/fastcgi/daemon/monitor_port"));
	addr.sin_addr.s_addr = INADDR_ANY;
	memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));

	if (-1 == bind(monitorSocket_, (const sockaddr*)&addr, sizeof(sockaddr_in))) {
		throw std::runtime_error("Cannot bind monitor port address");
	}

	if (1 == listen(monitorSocket_, SOMAXCONN)) {
		throw std::runtime_error("Cannot listen monitor port");
	}

	monitorThread_.reset(new boost::thread(boost::bind(&FCGIServer::monitor, this)));
}

void
FCGIServer::initRequestCache() {
	const std::string cacheComponentName = globals_->config()->asString(
		"/fastcgi/daemon[count(request-cache)=1]/request-cache/@component",
		StringUtils::EMPTY_STRING);
	Component *cacheComponent = globals()->components()->find(cacheComponentName);
	if (!cacheComponent) {
		return;
	}
	request_cache_ = dynamic_cast<RequestCache*>(cacheComponent);
	if (!request_cache_) {
		throw std::runtime_error("Component " + cacheComponentName + " does not implement RequestCache interface");
	}
}

void
FCGIServer::initTimeStatistics() {
	const std::string componentName = globals_->config()->asString(
		"/fastcgi/daemon[count(statistics)=1]/statistics/@component",
		StringUtils::EMPTY_STRING);
	Component *statisticsComponent = globals()->components()->find(componentName);
	if (!statisticsComponent) {
		return;
	}
	time_statistics_ = dynamic_cast<ResponseTimeStatistics*>(statisticsComponent);
	if (!time_statistics_) {
		throw std::runtime_error("Component " + componentName +
			" does not implement ResponseTimeStatistics interface");
	}
}

void
FCGIServer::createWorkThreads() {
	for (std::vector<boost::shared_ptr<Endpoint> >::iterator i = endpoints_.begin();
		 i != endpoints_.end();
		 ++i) {
		boost::function<void()> f = boost::bind(&FCGIServer::handle, this, i->get());
		for (unsigned short t = 0; t < (*i)->threads(); ++t) {
			globalPool_.create_thread(f);
		}
	}
}

void
FCGIServer::initFastCGISubsystem() {
	if (0 != FCGX_Init()) {
		throw std::runtime_error("Cannot init fastcgi library");
	}

	std::vector<std::string> v;
	globals_->config()->subKeys("/fastcgi/daemon/endpoint", v);
	for (std::vector<std::string>::iterator i = v.begin(), end = v.end(); i != end; ++i) {
		boost::shared_ptr<Endpoint> endpoint(new Endpoint(
			globals_->config()->asString(*i + "/socket", ""),
			globals_->config()->asString(*i + "/port", ""),
			boost::lexical_cast<unsigned>(globals_->config()->asString(*i + "/threads"))));
		const int backlog = globals_->config()->asInt(*i + "/backlog", SOMAXCONN);
		endpoint->openSocket(backlog);	
		endpoints_.push_back(endpoint);
	}

	if (endpoints_.empty()) {
		throw std::runtime_error("You must specify at least one endpoint");
	}
}

void
FCGIServer::handle(Endpoint *endpoint) {
	boost::shared_ptr<ServerStopper> stopper = stopper_;
	Logger* logger = globals_->logger();
	while (true) {
		try {
			boost::shared_ptr<ThreadHolder> holder = active_thread_holder_;
			if (stopper_->stopped()) {
				return;
			}

			Endpoint::ScopedBusyCounter busyCounter(*endpoint);
			RequestTask task;
			task.request = boost::shared_ptr<Request>(new Request(logger, request_cache_));
			task.request_stream = boost::shared_ptr<RequestIOStream>(
				new FastcgiRequest(task.request, endpoint, logger, time_statistics_, logTimes_));

			FastcgiRequest *request = dynamic_cast<FastcgiRequest*>(task.request_stream.get());

			busyCounter.decrement();
			holder.reset();
			int status = request->accept();
			if (stopper_->stopped()) {
				return;
			}
			holder = active_thread_holder_;
			if (status < 0) {
				throw std::runtime_error("failed to accept fastcgi request: " +
					boost::lexical_cast<std::string>(status));
			}
			busyCounter.increment();

			try {
				request->attach();
			}
			catch (const std::exception &e) {
				logger->error("caught exception while attach request: %s", e.what());
				task.request->sendError(400);
				continue;
			}

			try {
				handleRequest(task);
			}
			catch (const std::exception &e) {
				task.request->sendError(500);
			}
		}
		catch (const std::exception &e) {
			logger->error("caught exception while handling request: %s", e.what());
		}
		catch (...) {
			logger->error("caught unknown exception while handling request");
		}
	}
}

void
FCGIServer::handleRequest(RequestTask task) {
	logger()->debug("handling request %s", task.request->getScriptName().c_str());
	FastcgiRequest *request = dynamic_cast<FastcgiRequest*>(task.request_stream.get());
	const HandlerSet::HandlerDescription* handler = getHandler(task);
	request->setHandlerDesc(handler);
	handleRequestInternal(handler, task);
}

void
FCGIServer::monitor() {
	while (true) {
		if (stopper_->stopped()) {
			return;
		}
		int s = -1;
		try {
			s = accept(monitorSocket_, NULL, NULL);
			if (stopper_->stopped()) {
				return;
			}
			if (-1 == s) {
				throw std::runtime_error("Cannot accept connection on monitor port");
			}

			char buf[80];
			int rlen = read(s, buf, sizeof(buf));
			if (rlen <= 0) {
				close(s);
				continue;
			}

			const char c = buf[0];
			if ('i' == c || 'I' == c) {
				std::string info = getServerInfo();
				write(s, info.c_str(), info.size());
			} else if ('s' == c || 'S' == c) { 
				stop();
			}

			close(s);
		} 
		catch (const std::exception &e) {
			if (-1 != s) {
				close(s);
			}
			Logger *logger = globals_->logger();
			if (logger) {
				if (RUNNING == status()) {
					logger->error("%s, errno = %i", e.what(), errno);
					continue;
				}
				if (stopper_->stopped()) {
					continue;	
				}
			}
			std::cerr << e.what() << ", errno = " << errno << std::endl;
		}
	}
}

void
FCGIServer::writePid(const Config& config) {
	const std::string& file = config.asString("/fastcgi/daemon/pidfile");
	try {
		std::ofstream f(file.c_str());
		f.exceptions(std::ios::badbit);
		f << static_cast<int>(getpid());
	}
	catch (std::ios::failure &e) {
		std::cerr << "can not open file " << file << std::endl;
		throw;
	}
}

std::string
FCGIServer::getServerInfo() const {

	std::string info;
	Status stat = status();

	info += "<fastcgi-daemon>\n";

	info += "<status>";
	switch (stat) {
		case LOADING:
			info += "loading";
			break;
		case RUNNING:
			info += "running";
			break;
		case NOT_INITED:
			info += "not inited";
			break;
		default:
			info += stopper_->stopped() ? "stopping" : "unknown";
			break;
	};
	info += "</status>\n";

	if (RUNNING == stat) {

		info += "<pools>\n";

		std::stringstream s;
		s << "<endpoint_pools>\n";
		for (std::vector<boost::shared_ptr<Endpoint> >::const_iterator i = endpoints_.begin();
			 i != endpoints_.end();
			 ++i) {
			s << "<endpoint"
				<< " socket=\"" << (*i)->toString() << "\""
				<< " threads=\"" << (*i)->threads() << "\"" 
				<< " busy=\"" << (*i)->getBusyCounter() << "\""
				<< "/>\n";
		}
		s << "</endpoint_pools>\n";

		const Globals::ThreadPoolMap& pools = globals_->pools();
		for (Globals::ThreadPoolMap::const_iterator i = pools.begin(); i != pools.end(); ++i) {
			const RequestsThreadPool *pool = i->second.get();
			ThreadPoolInfo info = pool->getInfo();
			uint64_t goodTasks = info.goodTasksCounter;
			uint64_t badTasks = info.badTasksCounter;
			s << "<pool name=\"" << i->first << "\""
				<< " threads=\"" << info.threadsNumber << "\""
				<< " busy=\"" << info.busyThreadsCounter << "\""
				<< " queue=\"" << info.queueLength << "\""
				<< " current_queue=\"" << info.currentQueue << "\""
				<< " all_tasks=\"" << (goodTasks + badTasks)  << "\""
				<< " exception_tasks=\"" << badTasks << "\""
				<< "/>\n";
		}

		s << "</pools>\n";

		info += s.str();
	}
	
	info += "</fastcgi-daemon>\n";

	return info;
}

} // namespace fastcgi
