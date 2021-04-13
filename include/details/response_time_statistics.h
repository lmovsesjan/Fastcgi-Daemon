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

#ifndef _FASTCGI_DETAILS_RESPONSE_TIME_STATISTICS_H_
#define _FASTCGI_DETAILS_RESPONSE_TIME_STATISTICS_H_

#include <string>
#include <cstdint>

namespace fastcgi
{

class ResponseTimeStatistics {
public:
	ResponseTimeStatistics();
	virtual ~ResponseTimeStatistics();

	virtual void add(const std::string &handler, unsigned short status, std::uint64_t time) = 0;
};

} // namespace fastcgi

#endif // _FASTCGI_DETAILS_RESPONSE_TIME_STATISTICS_H_
