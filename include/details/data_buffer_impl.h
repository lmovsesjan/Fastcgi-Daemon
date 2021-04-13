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

#ifndef _FASTCGI_DETAILS_DATA_BUFFER_IMPL_H_
#define _FASTCGI_DETAILS_DATA_BUFFER_IMPL_H_

#include <string>
#include <cstdint>

namespace fastcgi
{

class DataBufferImpl {
public:
	virtual ~DataBufferImpl() {};
	virtual std::uint64_t read(std::uint64_t pos, char *data, std::uint64_t len) = 0;
	virtual std::uint64_t write(std::uint64_t pos, const char *data, std::uint64_t len) = 0;
	virtual char at(std::uint64_t pos) = 0;
	virtual std::uint64_t find(std::uint64_t begin, std::uint64_t end, const char* buf, std::uint64_t len) = 0;
	virtual std::pair<std::uint64_t, std::uint64_t> trim(std::uint64_t begin, std::uint64_t end) const = 0;
	virtual std::pair<char*, std::uint64_t> chunk(std::uint64_t pos) const = 0;
	virtual std::pair<std::uint64_t, std::uint64_t> segment(std::uint64_t pos) const = 0;
	virtual std::uint64_t size() const = 0;
	virtual void resize(std::uint64_t size) = 0;
	virtual const std::string& filename() const = 0;
	virtual DataBufferImpl* getCopy() const = 0;
};

} // namespace fastcgi

#endif // _FASTCGI_DETAILS_DATA_BUFFER_IMPL_H_
