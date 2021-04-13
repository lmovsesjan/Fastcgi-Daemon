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

#ifndef _FASTCGI_DETAILS_LOADER_H_
#define _FASTCGI_DETAILS_LOADER_H_

#include <boost/utility.hpp>

#include <string>
#include <vector>

#include <fastcgi2/component_factory.h>

namespace fastcgi
{

class Config;
class ComponentFactory;

class Loader : private boost::noncopyable
{
public:
	Loader();
	virtual ~Loader();

	virtual void init(const Config *config);
	virtual ComponentFactory *findComponentFactory(const std::string &type) const;
	
protected:
	virtual void load(const char *name, const char *path);
	void checkLoad(const char *err);

private:
	std::vector<void*> handles_;
	FactoryMap factories_;
};

} // namespace fastcgi

#endif // _FASTCGI_DETAILS_LOADER_H_
