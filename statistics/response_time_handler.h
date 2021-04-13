#ifndef _FASTCGI_STATISTICS_RESPONSE_TIME_HANDLER_H_
#define _FASTCGI_STATISTICS_RESPONSE_TIME_HANDLER_H_

#include <string>
#include <mutex>
#include <cstdint>

#include "fastcgi2/component.h"
#include "fastcgi2/handler.h"

#include "details/response_time_statistics.h"

namespace fastcgi
{

class CounterData {
public:
	CounterData();
	void add(std::uint64_t time);
	std::uint64_t min() const;
	std::uint64_t max() const;
	std::uint64_t avg() const;
	std::uint64_t hits() const;

private:
	std::uint64_t min_;
	std::uint64_t max_;
	std::uint64_t total_;
	std::uint64_t hits_;
};

class ResponseTimeHandler : virtual public Handler, virtual public Component,
	virtual public ResponseTimeStatistics {
public:
	ResponseTimeHandler(ComponentContext *context);
    virtual ~ResponseTimeHandler();

    virtual void onLoad();
    virtual void onUnload();

    virtual void handleRequest(Request *req, HandlerContext *handlerContext);
	virtual void add(const std::string &handler, unsigned short status, std::uint64_t time);

private:
	std::mutex mutex_;
	typedef std::map<unsigned short, std::shared_ptr<CounterData> > CounterMapType;
	std::map<std::string, CounterMapType> data_;
};

} // namespace fastcgi

#endif // _FASTCGI_STATISTICS_RESPONSE_TIME_HANDLER_H_
