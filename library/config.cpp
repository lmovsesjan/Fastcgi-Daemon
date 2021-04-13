#include "settings.h"

#include <fstream>
#include <sstream>
#include <iterator>
#include <stdexcept>
#include <boost/lexical_cast.hpp>

#include <libxml/xpath.h>
#include <libxml/parser.h>
#include <libxml/xinclude.h>

#include "fastcgi2/config.h"

#include "details/config.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace fastcgi
{

Config::Config()
{}

Config::~Config() {
}

std::unique_ptr<Config>
Config::create(const char *file) {
	return std::unique_ptr<Config>(new XmlConfig(file));
}

std::unique_ptr<Config>
Config::create(int &argc, char *argv[], HelpFunc func) {
	for (int i = 1; i < argc; ++i) {
		if (strncmp(argv[i], "--config", sizeof("--config") - 1) == 0) {
			const char *pos = strchr(argv[i], '=');
			if (NULL != pos) {
				std::unique_ptr<Config> conf(new XmlConfig(pos + 1));
				std::swap(argv[i], argv[argc - 1]);
				--argc;
				return conf;
			}
		}
	}
	std::stringstream stream;
	if (NULL != func) {
		func(stream);
	}
	else {
		stream << "usage: fastcgi-daemon --config=<config file>";
	}
	throw std::logic_error(stream.str());
}

const std::string&
Config::filename() const {
	return filename_;
}

void
Config::setFilename(const std::string &name) {
	filename_ = name;
}

XmlConfig::XmlConfig(const char *file) :
	doc_(NULL), regex_("\\$\\{([A-Za-z][A-Za-z0-9\\-]*)\\}")
{
	try {
		std::ifstream f(file);
		if (!f) {
			throw std::runtime_error(std::string("can not open ").append(file));
		}

		setFilename(file);

		doc_ = XmlDocHelper(xmlParseFile(file));
		XmlUtils::throwUnless(NULL != doc_.get());
		if (NULL == xmlDocGetRootElement(doc_.get())) {
			throw std::logic_error("got empty config");
		}
		XmlUtils::throwUnless(xmlXIncludeProcess(doc_.get()) >= 0);
		findVariables(doc_);
	}
	catch (const std::ios::failure &e) {
		throw std::runtime_error(std::string("can not read ").append(file));
	}
}

XmlConfig::~XmlConfig() {
}

int
XmlConfig::asInt(const std::string &key) const {
	return boost::lexical_cast<int>(asString(key));
}

int
XmlConfig::asInt(const std::string &key, int defval) const {
	try {
		return asInt(key);
	}
	catch (const std::exception &e) {
		return defval;
	}
}

std::string
XmlConfig::asString(const std::string &key) const {

	std::string res;
	
	XmlXPathContextHelper xctx(xmlXPathNewContext(doc_.get()));
	XmlUtils::throwUnless(NULL != xctx.get());

	XmlXPathObjectHelper object(xmlXPathEvalExpression((const xmlChar*) key.c_str(), xctx.get()));
	XmlUtils::throwUnless(NULL != object.get());
		
	if (NULL != object->nodesetval && 0 != object->nodesetval->nodeNr) {
			
		xmlNodeSetPtr ns = object->nodesetval;
		XmlUtils::throwUnless(NULL != ns->nodeTab[0]);
		const char *val = XmlUtils::value(ns->nodeTab[0]);
		if (NULL != val) {
			res.assign(val);
		}
	}
	else {
		std::stringstream stream;
		stream << "nonexistent config param: " << key;
		throw std::runtime_error(stream.str());
	}
	resolveVariables(res);
	return res;
}

std::string
XmlConfig::asString(const std::string &key, const std::string &defval) const {
	try {
		return asString(key);
	}
	catch (const std::exception &e) {
		return defval;
	}
}

void
XmlConfig::subKeys(const std::string &key, std::vector<std::string> &v) const {
	
	std::string tmp;
	
	XmlXPathContextHelper xctx(xmlXPathNewContext(doc_.get()));
	XmlUtils::throwUnless(NULL != xctx.get());

	XmlXPathObjectHelper object(xmlXPathEvalExpression((const xmlChar*) key.c_str(), xctx.get()));
	XmlUtils::throwUnless(NULL != object.get());
		
	if (NULL != object->nodesetval && 0 != object->nodesetval->nodeNr) {
			
		xmlNodeSetPtr ns = object->nodesetval;
		v.reserve(ns->nodeNr);
			
		for (int i = 0; i < ns->nodeNr; ++i) {
			tmp.clear();
			XmlUtils::throwUnless(NULL != ns->nodeTab[i]);
			std::stringstream stream;
			stream << key << "[" << (i + 1) << "]";
			v.push_back(stream.str());
		}
	}
}

void
XmlConfig::findVariables(const XmlDocHelper &doc) {
	
	XmlXPathContextHelper xctx(xmlXPathNewContext(doc.get()));
	XmlUtils::throwUnless(NULL != xctx.get());
	
	XmlXPathObjectHelper object(xmlXPathEvalExpression((const xmlChar*) "/fastcgi/variables/variable", xctx.get()));
	XmlUtils::throwUnless(NULL != object.get());
	
	if (NULL != object->nodesetval && 0 != object->nodesetval->nodeNr) {
		xmlNodeSetPtr ns = object->nodesetval;
		for (int i = 0; i < ns->nodeNr; ++i) {
			
			xmlNodePtr node = ns->nodeTab[i];
			XmlUtils::throwUnless(NULL != node);
			
			const char *val = XmlUtils::value(node);
			const char *name = XmlUtils::attrValue(node, "name");
			
			if (NULL == val || NULL == name) {
				throw std::logic_error("bad variable definition");
			}
			vars_.insert(std::pair<std::string, std::string>(name, val));
		}
	}
}

void
XmlConfig::resolveVariables(std::string &val) const {
	boost::smatch res;
	while (boost::regex_search(val, res, regex_)) {
		if (2 == res.size()) {
			std::string key(res[1].first, res[1].second);
			val.replace(res.position(static_cast<boost::smatch::size_type>(0)), res.length(0), findVariable(key));
		}
	}
}

const std::string&
XmlConfig::findVariable(const std::string &key) const {
	std::map<std::string, std::string>::const_iterator i = vars_.find(key);
	if (vars_.end() != i) {
		return i->second;
	}
	else {
		throw std::runtime_error(std::string("nonexistent variable ").append(key));
	}
}

} // namespace fastcgi
