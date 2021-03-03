#include "settings.h"

#include <vector>
#include <stdexcept>
#include <algorithm>
#include <map>
#include <string.h>
#include <functional>

#include <dlfcn.h>

#include "details/loader.h"
#include "fastcgi2/config.h"
#include "fastcgi2/component_factory.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace fastcgi
{

Loader::Loader()
{
}

Loader::~Loader() {
	std::for_each(handles_.rbegin(), handles_.rend(), std::bind(&dlclose, std::placeholders::_1));
}

void
Loader::init(const Config *config) {	
	std::vector<std::string> v;
	std::string key("/fastcgi/modules/module");
	
	config->subKeys(key, v);
	for (std::vector<std::string>::iterator i = v.begin(), end = v.end(); i != end; ++i) {
		const std::string name = config->asString(*i + "/@name");
		const std::string path = config->asString(*i + "/@path");		
		load(name.c_str(), path.c_str());
	}
}

ComponentFactory*
Loader::findComponentFactory(const std::string &type) const {
	FactoryMap::const_iterator i = factories_.find(type);
	if (i != factories_.end()) {
		return i->second;
	}
	return NULL;
}

void
Loader::load(const char *name, const char *path) {

	void *handle = NULL;
	try {
		handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
		checkLoad(dlerror());
		
		void *libraryEntry = dlsym(handle, "getFactoryMap");
		checkLoad(dlerror());
		FastcgiGetFactoryMapFunction getFactoryMap = NULL;
		memcpy(&getFactoryMap, &libraryEntry, sizeof(FastcgiGetFactoryMapFunction));

		FactoryMap *factoryMap = getFactoryMap();
		
		for (FactoryMap::const_iterator i = factoryMap->begin(); i != factoryMap->end(); ++i) {
			const std::string fullFactoryName = std::string(name) + ":" + i->first;
			factories_.insert(make_pair(fullFactoryName, i->second));
		}
		
		handles_.push_back(handle);
		
	} catch (const std::exception &e) {
		if (NULL != handle) {
			dlclose(handle);
		}
		throw;
	}
}

void
Loader::checkLoad(const char *err) {
	if (NULL != err) {
		throw std::runtime_error(err);
	}
}

} // namespace fastcgi
