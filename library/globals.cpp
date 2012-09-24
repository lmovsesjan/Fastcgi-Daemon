#include "settings.h"

#include "fastcgi2/component.h"
#include "fastcgi2/config.h"
#include "fastcgi2/handler.h"
#include "fastcgi2/logger.h"

#include "details/componentset.h"
#include "details/globals.h"
#include "details/handlerset.h"
#include "details/loader.h"
#include "details/request_thread_pool.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace fastcgi
{

Globals::Globals(const Config *config) : config_(config), loader_(new Loader()),
	handlerSet_(new HandlerSet()), componentSet_(new ComponentSet()), logger_(NULL)
{
	loader_->init(config);
	componentSet_->init(this);
	handlerSet_->init(config, componentSet_.get());

	initLogger();
	initPools();
	startThreadPools();
}

Globals::~Globals() {
}

ComponentSet*
Globals::components() const {
	return componentSet_.get();
}

HandlerSet*
Globals::handlers() const {
	return handlerSet_.get();
}

const Globals::ThreadPoolMap&
Globals::pools() const {
	return pools_;
}

Loader*
Globals::loader() const {
	return loader_.get();
}

Logger*
Globals::logger() const {
	return logger_;
}

const Config*
Globals::config() const {
	return config_;
}

static void
startUpFunc(const std::set<Handler*> &handlers) {
	for (std::set<Handler*>::const_iterator it = handlers.begin();
		 it != handlers.end();
		 ++it) {
		(*it)->onThreadStart();
	}
}

void
Globals::startThreadPools() {
	for (ThreadPoolMap::iterator it = pools_.begin(); it != pools_.end(); ++it) {
		std::set<Handler*> handlers;
		handlerSet_->findPoolHandlers(it->first, handlers);
		it->second->start(boost::bind(&startUpFunc, handlers));
	}
}

void
Globals::stopThreadPools() {
	for (ThreadPoolMap::iterator it = pools_.begin(); it != pools_.end(); ++it) {
		it->second->stop();
	}
}

void
Globals::joinThreadPools() {
	for (ThreadPoolMap::iterator i = pools_.begin(); i != pools_.end(); ++i) {
		i->second->join();
	}
}

void
Globals::initPools() {
	std::set<std::string> poolsNeeded = handlerSet_->getPoolsNeeded();

	std::vector<std::string> poolSubkeys;
	config_->subKeys("/fastcgi/pools/pool", poolSubkeys);
    unsigned maxTasksInProcessCounter = 0;
    for (std::vector<std::string>::const_iterator p = poolSubkeys.begin(); p != poolSubkeys.end(); ++p) {
        const std::string poolName = config_->asString(*p + "/@name");
        const int threadsNumber = config_->asInt(*p + "/@threads");
        const int queueLength = config_->asInt(*p + "/@queue");
        const int delay = config_->asInt(*p + "/@max-delay", 0);

		maxTasksInProcessCounter += (threadsNumber + queueLength);
		if (maxTasksInProcessCounter > 65535) {
			throw std::runtime_error("The sum of all threads and queue attributes must be not more than 65535");
		}

		if (pools_.find(poolName) != pools_.end()) {
            throw std::runtime_error(poolName + ": pool names must be unique");
        }

		if (poolsNeeded.find(poolName) == poolsNeeded.end()) {
			continue;
		}

		pools_.insert(make_pair(poolName, boost::shared_ptr<RequestsThreadPool>(delay ?
				new RequestsThreadPool(threadsNumber, queueLength, delay, logger_) :
				new RequestsThreadPool(threadsNumber, queueLength, logger_))));
    }

    for (std::set<std::string>::const_iterator i = poolsNeeded.begin(); i != poolsNeeded.end(); ++i) {
        if (pools_.find(*i) == pools_.end()) {
            throw std::runtime_error("cannot find pool " + *i);
        }
    }
}

void
Globals::initLogger() {
	const std::string loggerComponentName = config_->asString(
		"/fastcgi/daemon[count(logger)=1]/logger/@component");
	Component *loggerComponent = componentSet_->find(loggerComponentName);
	if (!loggerComponent) {
		throw std::runtime_error("Daemon logger does not exist");
	}
	logger_ = dynamic_cast<Logger*>(loggerComponent);
	if (!logger_) {
		throw std::runtime_error("Component " + loggerComponentName + " does not implement Logger interface");
	}
}

} // namespace fastcgi
