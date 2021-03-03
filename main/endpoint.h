#ifndef _FASTCGI_FASTCGI_ENDPOINT_H_
#define _FASTCGI_FASTCGI_ENDPOINT_H_

#include <mutex>

namespace fastcgi
{

class Endpoint {
public:
	class ScopedBusyCounter {
	public:
		ScopedBusyCounter(Endpoint &endpoint);
		~ScopedBusyCounter();
		void increment();
		void decrement();
	private:
		bool incremented_;
		Endpoint &endpoint_;
	};

public:
	Endpoint(const std::string &path, const std::string &port, unsigned short threads);
	virtual ~Endpoint();

	int socket() const;

	unsigned short threads() const;

	std::string toString() const;
	unsigned short getBusyCounter() const;

	void openSocket(const int backlog);

	void incrementBusyCounter();
	void decrementBusyCounter();

private:
	int socket_;
	int busy_count_;
	unsigned short threads_;
	mutable std::mutex mutex_;
	std::string socket_path_, socket_port_;
};

} // namespace fastcgi

#endif // _FASTCGI_FASTCGI_ENDPOINT_H_
