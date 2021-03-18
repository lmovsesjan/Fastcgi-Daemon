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

#ifndef _FASTCGI_LOGGER_H_
#define _FASTCGI_LOGGER_H_

#include <cstdarg>
#include <string>
#include <boost/utility.hpp>

namespace fastcgi
{

class Logger : private boost::noncopyable
{
public:
	enum Level {
		DEBUG, INFO, ERROR, EMERGENCY
	};
	
public:
	Logger();
	virtual ~Logger();

	void exiting(const char *function);
	void entering(const char *function);

	Level getLevel() const;
	void setLevel(const Level level);

	static Level stringToLevel(const std::string &);
	static std::string levelToString(const Level);

	virtual void info(const char *format, ...);
	virtual void debug(const char *format, ...);
	virtual void error(const char *format, ...);
	virtual void emerg(const char *format, ...);

	virtual void log(const Level level, const char *format, va_list args) = 0;

protected:
	virtual void setLevelInternal(const Level level);
	virtual void rollOver();

private:
	Level level_;
};

class LoggerRequestId
{
public:
	LoggerRequestId();
	virtual ~LoggerRequestId();

	virtual void setRequestId(const std::string &id) = 0;
	virtual std::string getRequestId() = 0;
};


class BulkLogger : public Logger {
public:
	BulkLogger();

protected:
	virtual void log(const Level level, const char *format, va_list args);
};

} // namespace fastcgi

#endif // _FASTCGI_LOGGER_H_
