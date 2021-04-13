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

#ifndef _FASTCGI_DETAILS_FUNCTORS_H_
#define _FASTCGI_DETAILS_FUNCTORS_H_

#include <cctype>
#include <string>
#include <cstring>
#include <algorithm>
#include <functional>
#include <cstdint>

#include "settings.h"
#include "details/range.h"

namespace fastcgi
{

struct CharCILess : public std::binary_function<char, char, bool>
{
	bool operator () (char c, char target) const {
		return tolower(c) < tolower(target);
	}
};

struct RangeCILess : public std::binary_function<const Range&, const Range&, bool>
{
	bool operator() (const Range &range, const Range &target) const {
		return std::lexicographical_compare(range.begin(), range.end(), target.begin(), target.end(), CharCILess());
	}
};

#if defined(HAVE_GNUCXX_HASHMAP) || defined(HAVE_EXT_HASH_MAP) || defined(HAVE_STLPORT_HASHMAP)

struct StringCIHash : public std::unary_function<const std::string&, std::uint32_t>
{
	std::uint32_t operator () (const std::string &str) const {
		std::uint32_t value = 0;
		for (std::string::const_iterator i = str.begin(), end = str.end(); i != end; ++i) {
			value += 5 * tolower(*i);
		}
		return value;
	}
};

struct StringCIEqual : public std::binary_function<const std::string&, const std::string&, bool>
{
	bool operator () (const std::string& str, const std::string& target) const {
		if (str.size() == target.size()) {
			return (strncasecmp(str.c_str(), target.c_str(), str.size()) == 0);
		}
		return false;
	}
};

#else

struct StringCILess : public std::binary_function<const std::string&, const std::string&, bool>
{
	bool operator () (const std::string& str, const std::string& target) const {
		return std::lexicagraphical_compare(str.begin(), str.end(), target.begin(), target.end(), CharCILess());
	}
};

#endif

} // namespace fastcgi

#endif // _FASTCGI_DETAILS_FUNCTORS_H_
