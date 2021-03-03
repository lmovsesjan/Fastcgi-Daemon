#include "settings.h"

#include <boost/lexical_cast.hpp>

#include "details/handlerset.h"
#include "details/componentset.h"
#include "details/request_filter.h"

#include "fastcgi2/config.h"
#include "fastcgi2/component.h"
#include "fastcgi2/handler.h"
#include "fastcgi2/request.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace fastcgi
{

HandlerSet::HandlerSet() {
}

HandlerSet::~HandlerSet() {
}

void
HandlerSet::init(const Config *config, const ComponentSet *componentSet) {
    std::vector<std::string> v;

    config->subKeys("/fastcgi/handlers/handler", v);
    for (std::vector<std::string>::const_iterator k = v.begin(), end = v.end(); k != end; ++k) {
        HandlerDescription handlerDesc;
        handlerDesc.poolName = config->asString(*k + "/@pool");
        handlerDesc.id = config->asString(*k + "/@id", "");

        std::string url_filter = config->asString(*k + "/@url", "");
        if (!url_filter.empty()) {
              handlerDesc.filters.push_back(std::make_pair(
                  "url", std::shared_ptr<RequestFilter>(new UrlFilter(url_filter))));
        }

        std::string host_filter = config->asString(*k + "/@host", "");
        if (!host_filter.empty()) {
              handlerDesc.filters.push_back(std::make_pair(
                  "host", std::shared_ptr<RequestFilter>(new HostFilter(host_filter))));
        }

        std::string port_filter = config->asString(*k + "/@port", "");
        if (!port_filter.empty()) {
              handlerDesc.filters.push_back(std::make_pair(
                  "port", std::shared_ptr<RequestFilter>(new PortFilter(port_filter))));
        }

        std::string address_filter = config->asString(*k + "/@address", "");
        if (!address_filter.empty()) {
              handlerDesc.filters.push_back(std::make_pair(
                  "address", std::shared_ptr<RequestFilter>(new AddressFilter(address_filter))));
        }

        std::string referer_filter = config->asString(*k + "/@referer", "");
        if (!referer_filter.empty()) {
              handlerDesc.filters.push_back(std::make_pair(
                  "referer", std::shared_ptr<RequestFilter>(new RefererFilter(referer_filter))));
        }

        std::vector<std::string> q;
        config->subKeys(*k + "/param", q);
        for (std::vector<std::string>::const_iterator it = q.begin(); it != q.end(); ++it) {
            std::string name = config->asString(*it + "/@name", "");
            if (name.empty()) {
                continue;
            }
            std::string value = config->asString(*it, "");
            if (value.empty()) {
                continue;
            }
            handlerDesc.filters.push_back(std::make_pair(
                "param", std::shared_ptr<RequestFilter>(new ParamFilter(name, value))));
        }

        std::vector<std::string> components;
        config->subKeys(*k + "/component", components);
        for (std::vector<std::string>::const_iterator c = components.begin(); c != components.end(); ++c) {
            const std::string componentName = config->asString(*c + "/@name");
            Component *handlerComponent = componentSet->find(componentName);
            if (!handlerComponent) {
                throw std::runtime_error("Cannot find component: " + componentName);
            }

            Handler *handler = dynamic_cast<Handler*>(handlerComponent);
            if (!handler) {
                throw std::runtime_error("Component " + componentName + " does not implement interface Handler");
            }

            handlerDesc.handlers.push_back(handler);
        }
        handlers_.push_back(handlerDesc);
    }
}

const HandlerSet::HandlerDescription*
HandlerSet::findURIHandler(const Request *request) const {
    for (HandlerArray::const_iterator i = handlers_.begin(); i != handlers_.end(); ++i) {
        bool matched = true;
        for (HandlerDescription::FilterArray::const_iterator f = i->filters.begin();
             f != i->filters.end();
             ++f) {
            if (f->first == "url" && !f->second->check(request)) {
                matched = false;
                break;
            }
            else if (f->first == "host" && !f->second->check(request)) {
                matched = false;
                break;
            }
            else if (f->first == "address" && !f->second->check(request)) {
                matched = false;
                break;
            }
            else if (f->first == "port" && !f->second->check(request)) {
                matched = false;
                break;
            }
            else if (f->first == "referer" && !f->second->check(request)) {
                matched = false;
                break;
            }
            else if (f->first == "param" && !f->second->check(request)) {
                matched = false;
                break;
            }
        }

        if (matched) {
            return &(*i);
        }
    }
    return NULL;
}

void
HandlerSet::findPoolHandlers(const std::string &poolName, std::set<Handler*> &handlers) const {
    handlers.clear();
    for (HandlerArray::const_iterator it = handlers_.begin();
         it != handlers_.end();
         ++it) {
        if (it->poolName == poolName) {
            handlers.insert(it->handlers.begin(), it->handlers.end());
        }
    }
}

std::set<std::string>
HandlerSet::getPoolsNeeded() const {
    std::set<std::string> pools;
    for (HandlerArray::const_iterator i = handlers_.begin(); i != handlers_.end(); ++i) {
        pools.insert(i->poolName);
    }
    return pools;
}

} // namespace fastcgi

