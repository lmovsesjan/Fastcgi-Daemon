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

#ifndef _FASTCGI_COMPONENT_FACTORY_H_
#define _FASTCGI_COMPONENT_FACTORY_H_

#include <string>
#include <map>

#include <boost/utility.hpp>

namespace fastcgi
{

class Component;
class ComponentFactory;
class ComponentContext;
	
typedef std::map<std::string, ComponentFactory*> FactoryMap;

class ComponentFactory : private boost::noncopyable
{
public:
	ComponentFactory();
	virtual ~ComponentFactory();
	virtual Component* createComponent(ComponentContext *context) = 0;
}; 

template<typename T>
class DefaultComponentFactory : public ComponentFactory {
public:
	virtual Component* createComponent(ComponentContext *context) {
		return new T(context);
	}
	
	virtual ~DefaultComponentFactory() {
	}
};

} // namespace fastcgi

typedef fastcgi::FactoryMap* (*FastcgiGetFactoryMapFunction)();

#if __GNUC__ >= 4
#	define FCGIDAEMON_DSO_GLOBALLY_VISIBLE \
		__attribute__ ((visibility ("default")))
#else
#	define FCGIDAEMON_DSO_GLOBALLY_VISIBLE
#endif

#define FCGIDAEMON_REGISTER_FACTORIES_BEGIN() \
	extern "C" FCGIDAEMON_DSO_GLOBALLY_VISIBLE \
	const fastcgi::FactoryMap* getFactoryMap() { \
		static fastcgi::FactoryMap m;
			        
#define FCGIDAEMON_ADD_DEFAULT_FACTORY(name, Type) \
		m.insert(std::make_pair((name), new fastcgi::DefaultComponentFactory<Type>));

#define FCGIDAEMON_ADD_FACTORY(name, factory) \
		m.insert(std::make_pair((name), (factory)));

#define FCGIDAEMON_REGISTER_FACTORIES_END() \
	    return &m; \
	}

#endif //_FASTCGI_COMPONENT_FACTORY_H_

