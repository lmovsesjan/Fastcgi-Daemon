#include "settings.h"

#include <sys/stat.h>
#include <cassert>
#include <cerrno>
#include <sstream>

#include <fcgiapp.h>

#include "endpoint.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace fastcgi
{

Endpoint::ScopedBusyCounter::ScopedBusyCounter(Endpoint &endpoint) :
	incremented_(false), endpoint_(endpoint) {
	increment();
}

Endpoint::ScopedBusyCounter::~ScopedBusyCounter() {
	decrement();
}

void
Endpoint::ScopedBusyCounter::increment() {
	if (!incremented_) {
		endpoint_.incrementBusyCounter();
		incremented_ = true;
	}
}

void
Endpoint::ScopedBusyCounter::decrement() {
	if (incremented_) {
		endpoint_.decrementBusyCounter();
		incremented_ = false;
	}
}

Endpoint::Endpoint(const std::string &path, const std::string &port, unsigned short threads) :
	socket_(-1), busy_count_(0), threads_(threads), socket_path_(path), socket_port_(port)
{
	if (socket_path_.empty() && socket_port_.empty()) {
		throw std::runtime_error("Both /socket and /port param for endpoint is empty");
	}
}

Endpoint::~Endpoint() {
}

int
Endpoint::socket() const {
	std::lock_guard<std::mutex> sl(mutex_);
	return socket_;
}

unsigned short
Endpoint::threads() const {
	return threads_;
}

std::string
Endpoint::toString() const {
	return socket_path_.empty() ? (std::string(":") + socket_port_) : socket_path_;
}

unsigned short
Endpoint::getBusyCounter() const {
	std::lock_guard<std::mutex> sl(mutex_);
	return busy_count_;
}

void
Endpoint::openSocket(const int backlog) {
	std::lock_guard<std::mutex> sl(mutex_);
	socket_ = FCGX_OpenSocket(toString().c_str(), backlog);
	if (-1 == socket_) {
		std::stringstream stream;
		stream << "can not open fastcgi socket: " << toString() << "[" << errno << "]";
		throw std::runtime_error(stream.str());
	}
	if (!socket_path_.empty()) {
		chmod(socket_path_.c_str(), 0666);
	}
}

void
Endpoint::incrementBusyCounter() {
	std::lock_guard<std::mutex> sl(mutex_);
	busy_count_ += 1;
}

void
Endpoint::decrementBusyCounter() {
	std::lock_guard<std::mutex> sl(mutex_);
	assert(busy_count_ > 0);
	busy_count_ -= 1;
}

} // namespace fastcgi
