#ifndef _FASTCGI_FASTCGI_SERVER_H_
#define _FASTCGI_FASTCGI_SERVER_H_

#include <map>
#include <string>
#include <vector>
#include <memory>

#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>

#include "details/server.h"

namespace fastcgi
{

class Config;
class Request;
class Logger;
class Loader;
class Endpoint;
class ComponentSet;
class HandlerSet;
class RequestsThreadPool;

class ServerStopper {
public:
	ServerStopper() : stopped_(false) {}

	void stopped(bool flag) {
		boost::mutex::scoped_lock lock(mutex_);
		stopped_ = flag;
	}

	bool stopped() const {
		boost::mutex::scoped_lock lock(mutex_);
		return stopped_;
	}
private:
	mutable boost::mutex mutex_;
	bool stopped_;
};

class ActiveThreadCounter {
public:
	ActiveThreadCounter() : count_(0)
	{}
	void increment() {
		boost::mutex::scoped_lock lock(mutex_);
		++count_;
	}
	void decrement() {
		boost::mutex::scoped_lock lock(mutex_);
		--count_;
	}

	int count() const {
		boost::mutex mutex_;
		return count_;
	}

private:
	boost::mutex mutex_;
	int count_;
};

class FCGIServer : public Server {
protected:
	enum Status {NOT_INITED, LOADING, RUNNING};

public:
	FCGIServer(boost::shared_ptr<Globals> globals);
	virtual ~FCGIServer();

	static void writePid(const Config& config);
	void start();
	void stop();
	void join();
	
private:
	virtual const Globals* globals() const;
	virtual Logger* logger() const;
	virtual void handleRequest(RequestTask task);
	void handle(Endpoint *endpoint);
	void monitor();

	std::string getServerInfo() const;

	void initMonitorThread();
	void initRequestCache();
	void initTimeStatistics();
    void initFastCGISubsystem();
	void initPools();
    void createWorkThreads();

	void stopInternal();

	void stopThreadFunction();

	Status status() const;

private:
	boost::shared_ptr<Globals> globals_;
	boost::shared_ptr<ServerStopper> stopper_;

	typedef char ThreadHolder;
	boost::shared_ptr<ThreadHolder> active_thread_holder_;

	std::vector<boost::shared_ptr<Endpoint> > endpoints_;
	int monitorSocket_;
	
	RequestCache *request_cache_;
	ResponseTimeStatistics *time_statistics_;
	
	mutable boost::mutex statusInfoMutex_;
	Status status_;

	std::auto_ptr<boost::thread> monitorThread_;
	std::auto_ptr<boost::thread> stopThread_;
	int stopPipes_[2];

	bool logTimes_;
	boost::thread_group globalPool_;
};

} // namespace fastcgi

#endif // _FASTCGI_FASTCGI_SERVER_H_
