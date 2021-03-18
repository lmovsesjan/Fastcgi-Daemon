#include "settings.h"

#include <stdexcept>
#include <syslog.h>

#include "fastcgi2/logger.h"
#include "fastcgi2/config.h"
#include "fastcgi2/request.h"
#include "fastcgi2/stream.h"
#include "fastcgi2/component_factory.h"

#include "syslog-logger.h"
#include <stdio.h>

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif
#include <sstream>
#include <iostream>


namespace fastcgi
{
	
SyslogLogger::SyslogLogger(ComponentContext *context) : Component(context) {
    const Config *config = context->getConfig();
	const std::string componentXPath = context->getComponentXPath();
/*
	std::string globalLogIdent = config->asString("/fastcgi/daemon/global-log-ident");
	const int facility = config->asInt(componentXPath + "/facility", 0);
	if (facility < 0 || facility > 6) {
		throw std::runtime_error("SyslogLogger: facility must be in range 0..6");
	}
	openlog(globalLogIdent.c_str(), 0, facility);
*/
	ident_ = config->asString(componentXPath + "/ident");
					    
	setLevel(stringToLevel(config->asString(componentXPath + "/level")));

	requestSpecificIdent_ = (config->asString(componentXPath + "/request-specific-ident", "off") == "on");
}

SyslogLogger::~SyslogLogger() {
}

void SyslogLogger::onLoad() {
}

void SyslogLogger::onUnload() {
}

void SyslogLogger::handleRequest(Request *request, HandlerContext *handlerContext) {
    request->setContentType("text/plain");
    const std::string &action = request->getArg("action");
    if ("setlevel" == action) {
        const std::string &l = request->getArg("level");
        setLevel(stringToLevel(l));
        RequestStream(request) << "level " << l << "successfully set" << std::endl;
    }
    else if ("rollover" == action) {
        rollOver();
        RequestStream(request) << "rollover successful" << std::endl;
    }
    else {
        RequestStream(request) << "bad action" << std::endl;
    }
}

void SyslogLogger::log(const Level level, const char *format, va_list args) {
	if (level >= getLevel()) {
		std::string format_ident;

		if (requestSpecificIdent_ && threadIdent_.get()) {
			format_ident = ident_ + ":" + *threadIdent_ + ": %s";
		} else {
			format_ident = ident_ + ": %s";
		}

		std::string formatted(1024*10, '\0');

		vsnprintf(const_cast<char *>(formatted.data()), formatted.size(), format, args);
	
		std::string tmp;
		std::stringstream ss(formatted);

		while (ss.good()) {
			std::getline(ss, tmp, '\n');
			syslog(toSyslogPriority(level), format_ident.c_str(), tmp.c_str());
		}
                
		//vsyslog(toSyslogPriority(level), signedFormat.c_str(), args);
	}
}

void SyslogLogger::setLevelInternal(const Level level) {
	priority_ = toSyslogPriority(level);
}

void SyslogLogger::rollOver() {
}

void SyslogLogger::setRequestId(const std::string &id) {
	threadIdent_.reset(new std::string(id));
}

std::string SyslogLogger::getRequestId() {
	if (requestSpecificIdent_ && threadIdent_.get()) {
		return *threadIdent_;
	}
	return std::string();
}

int
SyslogLogger::toSyslogPriority(const Level level) {
    switch (level) {
        case INFO:
            return LOG_INFO;
        case DEBUG:
            return LOG_DEBUG;
        case ERROR:
            return LOG_ERR;
        case EMERGENCY:
            return LOG_EMERG;
        default:
            throw std::logic_error("SyslogLogger: unknown log level");
    }
}

} //namespace fastcgi

FCGIDAEMON_REGISTER_FACTORIES_BEGIN()
FCGIDAEMON_ADD_DEFAULT_FACTORY("logger", fastcgi::SyslogLogger)
FCGIDAEMON_REGISTER_FACTORIES_END()
