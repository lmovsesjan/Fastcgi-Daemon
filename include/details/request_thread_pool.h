// Fastcgi Daemon - framework for design highload FastCGI applications on C++
// Copyright (C) 2011 Ilya Golubtsov <golubtsov@yandex-team.ru>

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef _FASTCGI_DETAILS_REQUEST_THREAD_POOL_H_
#define _FASTCGI_DETAILS_REQUEST_THREAD_POOL_H_

#include <fastcgi2/request.h>
#include <fastcgi2/request_io_stream.h>

#include "details/response_time_statistics.h"
#include "details/thread_pool.h"

namespace fastcgi {

class Handler;
class Logger;

struct RequestTask {
	std::shared_ptr<Request> request;
	std::vector<Handler*> handlers;
	std::shared_ptr<RequestIOStream> request_stream;
	std::uint64_t start;
};

class RequestsThreadPool : public ThreadPool<RequestTask> {
public:
	RequestsThreadPool(const unsigned threadsNumber, const unsigned queueLength, fastcgi::Logger *logger);
	RequestsThreadPool(const unsigned threadsNumber, const unsigned queueLength, std::uint64_t delay,
		fastcgi::Logger *logger);
	virtual ~RequestsThreadPool();
	virtual void handleTask(RequestTask task);
	std::uint64_t delay() const;
private:
	fastcgi::Logger *logger_;
	std::uint64_t delay_;
};

} // namespace fastcgi

#endif // _FASTCGI_DETAILS_REQUEST_THREAD_POOL_H_
