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

#ifndef _FASTCGI_DETAILS_RANGE_H_
#define _FASTCGI_DETAILS_RANGE_H_

#include <cctype>
#include <string>
#include <vector>
#include <cstddef>
#include <cassert>
#include <algorithm>
#include <string.h>

namespace fastcgi
{

class Range
{
public:
	typedef std::size_t size_type;

	Range() : begin_(NULL), end_(NULL) {
	}

	Range(const char* begin, const char* end) : begin_(begin), end_(end) { 
		assert(begin <= end);
	}

	const char* end() const { 
		return end_;
	}

	const char* begin() const { 
		return begin_;
	}

	bool empty() const { 
		return begin_ == end_;
	}

	Range::size_type size() const { 
		return end_ - begin_; 
	}

	char operator[] (Range::size_type i) const { 
		assert(i < size()); 
		return begin_[i];
	}

	Range trim() const {
		const char* begin = begin_;
		const char* end = end_;
		while (begin != end && isspace(*begin)) {
			++begin;
		}
		while (begin != end && isspace(*(end - 1))) {
			--end;
		}
		return Range(begin, end);
	}

	Range trimn(int b, int e) const {
		const char* begin = begin_;
		const char* end = end_;
		while (begin != end && b--) {
			++begin;
		}
		while (begin != end && e--) {
			--end;
		}
		return Range(begin, end);
	}

	const char* find(const Range& substr) const { 
		if (substr.size() == 1) {
			return find(*substr.begin());
		}
		else {
			return std::search(begin(), end(), substr.begin(), substr.end());
		}
	}

	const char* find(char ch) const {
		return std::find(begin(), end(), ch);
	}

	bool split(Range const& delim, Range& first, Range& second) const {
		return doSplit(find(delim), delim.size(), first, second);
	}

	bool split(const char delim, Range& first, Range& second) const {
		return doSplit(find(delim), 1, first, second);
	}
	
	bool endsWith(const Range &range) {
		if (range.size() <= size()) {
			return memcmp(end_ - range.size(), range.begin(), range.size()) == 0;
		}
		return false;
	}
	
	bool startsWith(const Range &range) const {
		if (range.size() <= size()) {
			return memcmp(begin_, range.begin_, range.size()) == 0;
		}
		return false;
	}

	bool operator < (const Range &range) const {
		return std::lexicographical_compare(begin_, end_, range.begin_, range.end_);
	}

	bool operator > (const Range &range) const {
		return range.operator < (*this);
	}

	bool operator == (const Range &range) const {
		if (size() == range.size()) {
			return memcmp(begin_, range.begin_, size()) == 0;
		}
		return false;
	}

	bool operator != (const Range &range) const {
		return !operator == (range);
	}

	static Range fromChars(const char* str) {
		return Range(str, str + strlen(str));
	}

	static Range fromString(const std::string& str) {
		return Range(str.c_str(), str.c_str() + str.size());
	}

	static Range fromVector(const std::vector<char>& vec) {
		return Range(&vec[0], &vec[0] + vec.size());
	}

	std::string toString() const { 
		return (NULL == begin()) ? std::string() : std::string(begin_, end_);
	}
	
private:
	
	bool doSplit(const char* e, size_type size, Range& first, Range& second) const {
		const char *begin = begin_;
		const char *end = end_;
		first = Range(begin, e);
		if (e != end) {
			second = Range(e + size, end);
			return true;
		} 
		else {
			second = Range(end, end);
			return false;
		}
	}
	
private:
	const char *begin_, *end_;
};

} // namespace fastcgi

#endif // _FASTCGI_DETAILS_RANGE_H_
