#include "settings.h"

#include <cstring>

#include <boost/lexical_cast.hpp>

#include "endpoint.h"
#include "fcgi_request.h"

#include "fastcgi2/logger.h"
#include "fastcgi2/request.h"

#include "details/response_time_statistics.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace fastcgi
{

static const std::string DAEMON_STRING = "fastcgi-daemon";

FastcgiRequest::FastcgiRequest(std::shared_ptr<Request> request, Endpoint *endpoint,
        Logger *logger, ResponseTimeStatistics *statistics, const bool logTimes) :
    request_(request), logger_(logger), endpoint_(endpoint),
    statistics_(statistics), logTimes_(logTimes), handler_(NULL)
{
    if (0 != FCGX_InitRequest(&fcgiRequest_, endpoint_->socket(), 0)) {
        throw std::runtime_error("can not init fastcgi request");
    }
}

FastcgiRequest::~FastcgiRequest() {
    std::uint64_t microsec = 0;
    if (logTimes_ || statistics_) {
        gettimeofday(&finish_time_, NULL);

        microsec = (finish_time_.tv_sec - accept_time_.tv_sec) *
            1000000 + (finish_time_.tv_usec - accept_time_.tv_usec);
    }

    if (logTimes_) {
        double res = static_cast<double>(microsec) / 1000000.0;
        logger_->info("handling %s taken %08f seconds", url_.c_str(), res);
    }

    if (statistics_) {
        try {
            statistics_->add(handler_ ? handler_->id : DAEMON_STRING, request_->status(), microsec);
        }
        catch (const std::exception &e) {
            logger_->error("Exception caught while update statistics: %s", e.what());
        }
        catch (...) {
            logger_->error("Unknown exception caught while update statistics");
        }
    }

    FCGX_Finish_r(&fcgiRequest_);
}

void
FastcgiRequest::attach() {

    char **envp = fcgiRequest_.envp;
    for (std::size_t i = 0; envp[i]; ++i) {
        if (0 == strncasecmp(envp[i], "REQUEST_URI=", sizeof("REQUEST_URI=") - 1)) {
            url_.assign(envp[i] + sizeof("REQUEST_URI=") - 1);
        }
        if (0 == strncasecmp(envp[i], "REQUEST_ID=", sizeof("REQUEST_ID=") - 1)) {
            request_id_.assign(envp[i] + sizeof("REQUEST_ID=") - 1);
        }
    }

    LoggerRequestId *logger_req_id = dynamic_cast<LoggerRequestId*>(logger_);
    if (logger_req_id) {
		logger_req_id->setRequestId(request_id_);
    }

    request_->attach(this, fcgiRequest_.envp);
}

int
FastcgiRequest::accept() {
    int status = FCGX_Accept_r(&fcgiRequest_);
    if (status >= 0 && (logTimes_|| statistics_)) {
        gettimeofday(&accept_time_, NULL);
    }
    return status;
}

int
FastcgiRequest::read(char *buf, int size) {
    return FCGX_GetStr(buf, size, fcgiRequest_.in);
}

static void
generateRequestInfo(const Request *request, std::stringstream &str) {
    str << "Url: " << request->getUrl();
    if ("POST" != request->getRequestMethod() && "PUT" != request->getRequestMethod()) {
        return;
    }
    std::string result;
    result.reserve(2048);
    std::vector<std::string> names;
    request->argNames(names);
    for (std::vector<std::string>::iterator it = names.begin();
         it != names.end();
         ++it) {
        std::vector<std::string> args;
        request->getArg(*it, args);
        for (std::vector<std::string>::iterator it2 = args.begin();
             it2 != args.end();
             ++it2) {
            if (!result.empty()) {
                result.push_back('&');
            }
            result.append(*it);
            result.push_back('=');
            if (result.size() + it2->size() > 1024) {
                str << ". Args: " << result << "...";
                return;
            }
            result.append(*it2);
        }
    }
    str << ". Args: " << result;
}

int
FastcgiRequest::write(const char *buf, int size) {
    int num = FCGX_PutStr(buf, size, fcgiRequest_.out);
    if (-1 == num) {
        std::stringstream str;
        int error = FCGX_GetError(fcgiRequest_.out);
        if (error > 0) {
            char buffer[256];
            str << "Cannot write data to fastcgi socket: " <<
                strerror_r(error, buffer, sizeof(buffer)) << ". ";
        }
        else {
            str << "FastCGI error. ";
        }
        generateRequestInfo(request_.get(), str);
        throw std::runtime_error(str.str());
    }
    return num;
}

void
FastcgiRequest::write(std::streambuf *buf) {
    std::vector<char> outv(4096);
    fcgi_streambuf outbuf(fcgiRequest_.out, &outv[0], outv.size());
    std::ostream os(&outbuf);
    os << buf;
}

void
FastcgiRequest::setHandlerDesc(const HandlerSet::HandlerDescription *handler) {
    handler_ = handler;
}

void
FastcgiRequest::flush() {
    int num = FCGX_FFlush(fcgiRequest_.out);
    if (-1 == num) {
        std::stringstream str;
        int error = FCGX_GetError(fcgiRequest_.out);
        if (error > 0) {
            char buffer[256];
            str << "Cannot flush data to fastcgi socket: " <<
                strerror_r(error, buffer, sizeof(buffer)) << ". ";
        }
        else {
            str << "FastCGI error. ";
        }
        generateRequestInfo(request_.get(), str);
        throw std::runtime_error(str.str());
    }
}

} // namespace fastcgi
