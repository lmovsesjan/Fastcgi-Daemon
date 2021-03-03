#include "settings.h"

#include <sys/time.h>

#include <boost/lexical_cast.hpp>

#include "fastcgi2/component_factory.h"
#include "fastcgi2/request.h"

#include "response_time_handler.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace fastcgi
{

CounterData::CounterData() : min_(std::numeric_limits<std::uint64_t>::max()),
	max_(0), total_(0), hits_(0)
{}

void
CounterData::add(std::uint64_t time) {
	total_ += time;
	++hits_;
	min_ = std::min(min_, time);
	max_ = std::max(max_, time);
}

std::uint64_t
CounterData::min() const {
	return min_;
}

std::uint64_t
CounterData::max() const {
	return max_;
}

std::uint64_t
CounterData::avg() const {
	return hits_ ? total_/hits_ : 0;
}

std::uint64_t
CounterData::hits() const {
	return hits_;
}

ResponseTimeHandler::ResponseTimeHandler(ComponentContext *context) : Component(context)
{}

ResponseTimeHandler::~ResponseTimeHandler()
{}

void
ResponseTimeHandler::onLoad() {
}

void
ResponseTimeHandler::onUnload() {
}

void
ResponseTimeHandler::handleRequest(Request *req, HandlerContext *handlerContext) {
	(void)handlerContext;
	std::stringstream str;
	str.precision(3);
	str << std::showpoint << std::fixed;
	str << "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
	str << "<response-time>";
	{
		std::lock_guard<std::mutex> lock(mutex_);
		for (std::map<std::string, CounterMapType>::iterator iter = data_.begin();
			 iter != data_.end();
			 ++iter) {
			str << "<handler id=\"" << iter->first << "\">";
			for (CounterMapType::iterator it = iter->second.begin();
				 it != iter->second.end();
				 ++it) {
				str << "<data";
				str << " status=\"" << it->first << "\"";
				str << " avg=\"" << 0.001*it->second->avg() << "\"";
				str << " min=\"" << 0.001*it->second->min() << "\"";
				str << " max=\"" << 0.001*it->second->max() << "\"";
				str << " hits=\"" << it->second->hits() << "\"";
				str << "/>";
			}
			str << "</handler>";
		}
	}
    str << "</response-time>";
	req->setStatus(200);
	req->write(str.rdbuf());
}

void
ResponseTimeHandler::add(const std::string &handler, unsigned short status, std::uint64_t time) {
	std::lock_guard<std::mutex> lock(mutex_);
	CounterMapType& handle = data_[handler];
	CounterMapType::iterator it = handle.find(status);
	if (handle.end() == it) {
		std::shared_ptr<CounterData> counter(new CounterData);
		counter->add(time);
		handle.insert(std::make_pair(status, counter));
	}
	else {
		it->second->add(time);
	}
}

} //namespace fastcgi

FCGIDAEMON_REGISTER_FACTORIES_BEGIN()
FCGIDAEMON_ADD_DEFAULT_FACTORY("statistics", fastcgi::ResponseTimeHandler)
FCGIDAEMON_REGISTER_FACTORIES_END()
