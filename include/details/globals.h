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

#ifndef _FASTCGI_DETAILS_GLOBALS_H_
#define _FASTCGI_DETAILS_GLOBALS_H_

#include <map>
#include <string>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>

namespace fastcgi
{

class ComponentSet;
class Config;
class HandlerSet;
class Loader;
class Logger;
class RequestsThreadPool;

class Globals : private boost::noncopyable {
public:
	Globals(const Config *config);
	virtual ~Globals();

	const Config* config() const;

	typedef std::map<std::string, boost::shared_ptr<RequestsThreadPool> > ThreadPoolMap;

	ComponentSet* components() const;
	HandlerSet* handlers() const;
	const ThreadPoolMap& pools() const;
	Loader* loader() const;
	Logger* logger() const;

	void stopThreadPools();
	void joinThreadPools();

private:
	void initPools();
	void initLogger();
	void startThreadPools();

private:
	ThreadPoolMap pools_;
	const Config* config_;
	std::auto_ptr<Loader> loader_;
	std::auto_ptr<HandlerSet> handlerSet_;
	std::auto_ptr<ComponentSet> componentSet_;
	Logger* logger_;
};

} // namespace fastcgi

#endif // _FASTCGI_DETAILS_GLOBALS_H_
