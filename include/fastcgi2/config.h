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

#ifndef _FASTCGI_CONFIG_H_
#define _FASTCGI_CONFIG_H_

#include <map>
#include <string>
#include <vector>
#include <memory>
#include <iosfwd>

#include <boost/utility.hpp>

namespace fastcgi
{

typedef std::ostream& (*HelpFunc)(std::ostream &stream);

class Config : private boost::noncopyable {
public:
	Config();
	virtual ~Config();
	
	virtual int asInt(const std::string &value) const = 0;
	virtual int asInt(const std::string &value, int defval) const = 0;
	
	virtual std::string asString(const std::string &value) const = 0;
	virtual std::string asString(const std::string &value, const std::string &defval) const = 0;
	
	virtual void subKeys(const std::string &value, std::vector<std::string> &v) const = 0;
	
	static std::unique_ptr<Config> create(const char *file);
	static std::unique_ptr<Config> create(int &argc, char *argv[], HelpFunc func = NULL);

	const std::string& filename() const;

protected:
	void setFilename(const std::string &name);

private:
	Config(const Config &);
	Config& operator = (const Config &);

private:
	std::string filename_;
};

} // namespace fastcgi

#endif // _FASTCGI_CONFIG_H_
