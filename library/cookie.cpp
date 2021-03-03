#include "settings.h"

#include <sstream>
#include <exception>
#include <limits>

#include "fastcgi2/util.h"
#include "fastcgi2/cookie.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace fastcgi
{

static const time_t MAX_EXPIRES_TIME = std::numeric_limits<std::int32_t>::max();

class Cookie::CookieData {
public:
    CookieData(const std::string &name, const std::string &value);
    ~CookieData();
    const std::string& name() const;
    const std::string& value() const;
    bool secure() const;
    void secure(bool value);
    bool httpOnly() const;
    void httpOnly(bool value);
    time_t expires() const;
    void expires(time_t value);
    const std::string& path() const;
    void path(const std::string &value);
    const std::string& domain() const;
    void domain(const std::string &value);
    bool permanent() const;
    void permanent(bool value);
    void urlEncode(bool value);
    std::string toString() const;
private:
    bool secure_;
    bool http_only_;
    bool encode_;
    time_t expires_;
    std::string name_, value_, path_, domain_;
};


Cookie::CookieData::CookieData(const std::string &name, const std::string &value) :
    secure_(false), http_only_(false), encode_(false), expires_(0),
    name_(name), value_(value), path_("/")
{}

Cookie::CookieData::~CookieData() {
}

const std::string&
Cookie::CookieData::name() const {
    return name_;
}

const std::string&
Cookie::CookieData::value() const {
    return value_;
}

bool
Cookie::CookieData::secure() const {
    return secure_;
}

void
Cookie::CookieData::secure(bool value) {
    secure_ = value;
}

bool
Cookie::CookieData::httpOnly() const {
    return http_only_;
}

void
Cookie::CookieData::httpOnly(bool value) {
    http_only_ = value;
}

time_t
Cookie::CookieData::expires() const {
    return expires_;
}

void
Cookie::CookieData::expires(time_t value) {
    expires_ = value;
}

const std::string&
Cookie::CookieData::path() const {
    return path_;
}

void
Cookie::CookieData::path(const std::string &value) {
    path_ = value;
}

const std::string&
Cookie::CookieData::domain() const {
    return domain_;
}

void
Cookie::CookieData::domain(const std::string &value) {
    domain_ = value;
}

bool
Cookie::CookieData::permanent() const {
    return expires_ == MAX_EXPIRES_TIME;
}

void
Cookie::CookieData::permanent(bool value) {
    expires_ = value ? MAX_EXPIRES_TIME : 0;
}

void
Cookie::CookieData::urlEncode(bool value) {
    encode_ = value;
}

std::string
Cookie::CookieData::toString() const {
    std::stringstream stream;
    stream << name_;
    stream << '=' << (encode_ ? StringUtils::urlencode(value_) : value_);
    if (!domain_.empty()) {
        stream << "; domain=" << domain_;
    }
    if (!path_.empty()) {
        stream << "; path=" << path_;
    }
    if (expires_) {
        stream <<  "; expires=" << HttpDateUtils::format(expires_);
    }
    if (secure_) {
        stream << "; secure";
    }
    if (http_only_) {
        stream << "; HttpOnly";
    }
    return stream.str();
}

Cookie::Cookie(const std::string &name, const std::string &value) :
        data_(new CookieData(name, value))
{}

Cookie::Cookie(const Cookie &cookie) : data_(new CookieData(*cookie.data_))
{}

Cookie::~Cookie() {
}

const std::string&
Cookie::name() const {
    return data_->name();
}

const std::string&
Cookie::value() const {
    return data_->value();
}

bool
Cookie::secure() const {
    return data_->secure();
}

void
Cookie::secure(bool value) {
    data_->secure(value);
}

bool
Cookie::httpOnly() const {
    return data_->httpOnly();
}

void
Cookie::httpOnly(bool value) {
    data_->httpOnly(value);
}

time_t
Cookie::expires() const {
    return data_->expires();
}

void
Cookie::expires(time_t value) {
    data_->expires(value);
}

const std::string&
Cookie::path() const {
    return data_->path();
}

void
Cookie::path(const std::string &value) {
    data_->path(value);
}

const std::string&
Cookie::domain() const {
    return data_->domain();
}

void
Cookie::domain(const std::string &value) {
    data_->domain(value);
}

bool
Cookie::permanent() const {
    return data_->permanent();
}

void
Cookie::permanent(bool value) {
    data_->permanent(value);
}

std::string
Cookie::toString() const {
    return data_->toString();
}

void
Cookie::urlEncode(bool value) {
    data_->urlEncode(value);
}

bool
Cookie::operator < (const Cookie &cookie) const {
    return name() < cookie.name();
}

Cookie&
Cookie::operator=(const Cookie &cookie) {
    if (this != &cookie) {
        data_.reset(new CookieData(*cookie.data_));
    }
    return *this;
}

} // namespace fastcgi
