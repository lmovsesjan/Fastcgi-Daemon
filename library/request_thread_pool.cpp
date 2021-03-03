#include "settings.h"

#include "details/request_thread_pool.h"

#include <sys/time.h>

#include <fastcgi2/except.h>
#include <fastcgi2/handler.h>
#include <fastcgi2/logger.h>

#include <fastcgi2/stream.h>

#include "details/handler_context.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace fastcgi
{

RequestsThreadPool::RequestsThreadPool(
    const unsigned threadsNumber, const unsigned queueLength, fastcgi::Logger *logger) :
        ThreadPool<RequestTask>(threadsNumber, queueLength), logger_(logger), delay_(0)
{}

RequestsThreadPool::RequestsThreadPool(
    const unsigned threadsNumber, const unsigned queueLength, std::uint64_t delay, fastcgi::Logger *logger) :
        ThreadPool<RequestTask>(threadsNumber, queueLength), logger_(logger), delay_(delay)
{}

RequestsThreadPool::~RequestsThreadPool()
{}

std::uint64_t
RequestsThreadPool::delay() const {
	return delay_;
}

void
RequestsThreadPool::handleTask(RequestTask task) {
    try {
    	if (delay_) {
    		struct timeval t;
    		gettimeofday(&t, 0);
    		std::uint64_t now = (t.tv_sec * 1000) + (t.tv_usec / 1000);
    		if (now - task.start > delay_) {
				logger_->error("thread pool task is timed out");
				task.request->sendError(503);
				return;
    		}
    	}
        try {
            LoggerRequestId *logger_req_id = dynamic_cast<LoggerRequestId*>(logger_);
            if (logger_req_id) {
                logger_req_id->setRequestId(task.request->getRequestId());
            }

            std::unique_ptr<HandlerContext> context(new HandlerContextImpl);
            for (std::vector<Handler*>::iterator i = task.handlers.begin();
                 i != task.handlers.end();
                 ++i) {
                if (task.request->isProcessed()) {
                    break;
                }
                (*i)->handleRequest(task.request.get(), context.get());
            }

            task.request->sendHeaders();
        }
        catch (const HttpException &e) {
            bool headersAlreadySent = false;
            try {
                task.request->setStatus(500);
            }
            catch (...) { // this means that headers already send and we cannot change status/headers and so on
                headersAlreadySent = true;
            }
            if (headersAlreadySent) {
                throw;
            }
            else {
                task.request->sendError(e.status());
            }
        }
        catch (const std::exception &e) {
            bool headersAlreadySent = false;
            try {
                task.request->setStatus(500);
            }
            catch (...) { // this means that headers already send and we cannot change status/headers and so on
                headersAlreadySent = true;
            }
            if (headersAlreadySent) {
                throw;
            }
            else {
                task.request->sendError(500);
                std::string err(e.what());
                task.request->write(err.c_str(), err.size());
            }
        }
        catch (...) {
            bool headersAlreadySent = false;
            try {
                task.request->setStatus(500);
            }
            catch (...) { // this means that headers already send and we cannot change status/headers and so on
                headersAlreadySent = true;
            }
            if (headersAlreadySent) {
                throw;
            }
            else {
                fastcgi::RequestStream stream(task.request.get());
                stream << "fastcgi-daemon: got unknown exception from handler";
                task.request->sendError(500);
            }
        }
    }
    catch (const std::exception &e) {
        logger_->error("%s", e.what());
        throw;
    }
    catch (...) {
        logger_->error("RequestsThreadPool::handleTask: got unknown exception, it should't happen");
        throw;
    }
}

} // namespace fastcgi
