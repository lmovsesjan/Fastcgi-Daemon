#include "settings.h"

#include <iostream>
#include <vector>
#include <stdexcept>
#include <algorithm>

#include "fastcgi2/config.h"
#include "fastcgi2/component.h"
#include "fastcgi2/component_factory.h"

#include "details/loader.h"
#include "details/componentset.h"
#include "details/component_context.h"
#include "details/globals.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace fastcgi
{

ComponentSet::ComponentContainer::ComponentContainer()
    : component(NULL), context(NULL), isLoadingStarted(false)
{}

ComponentSet::ComponentSet() : globals_(NULL) {
}

ComponentSet::~ComponentSet() {
    sendOnUnloadToComponents();
    for (ComponentMap::reverse_iterator i = components_.rbegin(); i != components_.rend(); ++i) {
        delete i->second.component;
        delete i->second.context;
    }
}

void
ComponentSet::init(const Globals *globals) {
    globals_ = globals;
    std::vector<std::string> v;
    std::string key("/fastcgi/components/component");
    globals->config()->subKeys(key, v);
    for (std::vector<std::string>::iterator i = v.begin(), end = v.end(); i != end; ++i) {
        const std::string name = globals->config()->asString(*i + "/@name");
        const std::string type = globals->config()->asString(*i + "/@type");
        add(name, type, *i);
    }
    sendOnLoadToComponents();
}

Component*
ComponentSet::find(const std::string &name) const {
    ComponentSet *nonConstThis = const_cast<ComponentSet*>(this);
    ComponentMap::iterator i = nonConstThis->components_.find(name);
    if (i != components_.end()) {
        ComponentContainer &cont = i->second;
        if (!isComponentLoaded(name)) {
            nonConstThis->sendOnLoad(name, cont);
        }
        return cont.component;
    }
    return NULL;
}

void
ComponentSet::add(const std::string &name, const std::string &type,
        const std::string &componentXPath) {
    ComponentFactory *factory = globals_->loader()->findComponentFactory(type);
    if (!factory) {
        throw std::runtime_error("Cannot find component factory for type: " + type);
    }
    ComponentContainer c;
    std::unique_ptr<ComponentContext> ctx(new ComponentContextImpl(globals_, componentXPath));
    try {
        c.component = factory->createComponent(ctx.get());
    }
    catch (const std::exception &e) {
        throw std::runtime_error(e.what());
    }
    catch (...) {
        throw std::runtime_error("Unknown exception caught");
    }
    c.context = ctx.release();
    components_.insert(make_pair(name, c));
}

void
ComponentSet::sendOnLoadToComponents() {
    for (ComponentMap::iterator i = components_.begin(); i != components_.end(); ++i) {
        sendOnLoad(i->first, i->second);
    }
}

void
ComponentSet::sendOnLoad(const std::string &componentName, ComponentContainer &cont) {
    if (!isComponentLoaded(componentName)) {
        if (cont.isLoadingStarted) {
            throw std::runtime_error("Cyclic component dependence found");
        }
        cont.isLoadingStarted = true;
        try {
            cont.component->onLoad();
        }
        catch (const std::exception &e) {
            throw std::runtime_error(e.what());
        }
        catch (...) {
            throw std::runtime_error("Unknown exception caught");
        }
        loadingStack_.push_back(componentName);
    }
}

void
ComponentSet::sendOnUnloadToComponents() {
    for (std::vector<std::string>::reverse_iterator i = loadingStack_.rbegin(); i != loadingStack_.rend(); ++i) {
        ComponentContainer cont = components_.find(*i)->second;
        try {
            cont.component->onUnload();
        }
        catch (const std::exception &e) {
            std::cerr << e.what() << std::endl;
        }
        catch (...) {
            std::cerr << "Unknown exception caught" << std::endl;
        }
    }
}

bool
ComponentSet::isComponentLoaded(const std::string &componentName) const {
    return std::find(loadingStack_.begin(), loadingStack_.end(), componentName) != loadingStack_.end();
}

} // namespace fastcgi
