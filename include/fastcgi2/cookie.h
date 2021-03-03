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

#ifndef _FASTCGI_COOKIE_H_
#define _FASTCGI_COOKIE_H_

#include <ctime>
#include <memory>
#include <string>

namespace fastcgi
{

class Cookie {
public:
	Cookie(const std::string &name, const std::string &value);
	Cookie(const Cookie &cookie);
	virtual ~Cookie();
	Cookie& operator=(const Cookie &cookie);
	bool operator < (const Cookie &cookie) const;
	
	const std::string& name() const;
	const std::string& value() const;
	
	bool secure() const;
	void secure(bool value);
	
	time_t expires() const;
	void expires(time_t value);

	void permanent(bool value);
	bool permanent() const;

	void httpOnly(bool value);
	bool httpOnly() const;

	const std::string& path() const;
	void path(const std::string &value);
	
	const std::string& domain() const;
	void domain(const std::string &value);

	std::string toString() const;

	void urlEncode(bool value);

private:
    class CookieData;
    std::unique_ptr<CookieData> data_;
};

} // namespace fastcgi

#endif // _FASTCGI_COOKIE_H_
