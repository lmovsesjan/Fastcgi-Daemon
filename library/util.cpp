#include "settings.h"

#include <cstdlib>
#include <stdexcept>

#include "fastcgi2/util.h"
#include "fastcgi2/logger.h"
#include "details/range.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace fastcgi
{

const std::string StringUtils::EMPTY_STRING;

StringUtils::StringUtils() 
{
}

StringUtils::~StringUtils() {
}

std::string
StringUtils::urlencode(const Range &range) {
	
	std::string result;
	result.reserve(3 * range.size());
	
	for (const char* i = range.begin(), *end = range.end(); i != end; ++i) {
		char symbol = (*i);
		if (isalnum(symbol)) {
			result.append(1, symbol);
			continue;
		}
		switch (symbol) {
			case '-': case '_': case '.': case '!': case '~': 
			case '*': case '(': case ')': case '\'': 
				result.append(1, symbol);
				break;
			default:
				result.append(1, '%');
				char bytes[3] = { 0, 0, 0 };
				bytes[0] = (symbol & 0xF0) / 16 ;
				bytes[0] += (bytes[0] > 9) ? 'A' - 10 : '0';
				bytes[1] = symbol & 0x0F;
				bytes[1] += (bytes[1] > 9) ? 'A' - 10 : '0';
				result.append(bytes, sizeof(bytes) - 1);
				break;
		}
	}
	return result;
}

void
StringUtils::urldecode(const Range &range, std::string &result) {
	for (const char *i = range.begin(), *end = range.end(); i != end; ++i) {
		switch (*i) {
			case '+':
				result.append(1, ' ');
				break;
			case '%':
				if (std::distance(i, end) > 2) {
					int digit;
					char f = *(i + 1), s = *(i + 2);
					digit = (f >= 'A' ? ((f & 0xDF) - 'A') + 10 : (f - '0')) * 16;
					digit += (s >= 'A') ? ((s & 0xDF) - 'A') + 10 : (s - '0');
					result.append(1, static_cast<char>(digit));
					i += 2;
				}
				else {
					result.append(1, '%');
				}
				break;
			default:
				result.append(1, (*i));
				break;
		}
	}
}

std::string
StringUtils::urldecode(const Range &range) {
	std::string result;
	result.reserve(range.size());
	urldecode(range, result);
	return result;
}

std::string
StringUtils::urldecode(DataBuffer data) {
	std::string result;
	result.reserve(data.size());
	for (DataBuffer::SegmentIterator it = data.begin(); it != data.end(); ++it) {
		std::pair<char*, std::uint64_t> chunk = *it;
		urldecode(Range(chunk.first, chunk.first + chunk.second), result);
	}
	return result;
}

void
StringUtils::parse(const Range &range, std::vector<NamedValue> &v) {
	
	Range tmp = range;
	while (!tmp.empty()) {
		Range key, value, head, tail, normalize;
		
		tmp.split('&', head, tail);
		head.split('=', key, value);
		if (!key.empty()) {
			try {
				v.push_back(std::pair<std::string, std::string>(urldecode(key), urldecode(value)));
			}
			catch (const std::exception &e) {
				throw;
				// do we really want to swallow this exception here ???
				// log()->error("%s, caught exception: %s", e.what());
			}
		}
		tmp = tail;
	}
}

void
StringUtils::parse(DataBuffer data, std::vector<NamedValue> &v) {

	DataBuffer tmp = data;
	while (!tmp.empty()) {
		DataBuffer key, value, head, tail;
		tmp.split('&', head, tail);
		head.split('=', key, value);
		if (!key.empty()) {
			try {
				v.push_back(std::pair<std::string, std::string>(urldecode(key), urldecode(value)));
			}
			catch (const std::exception &e) {
				throw;
			}
		}
		tmp = tail;
	}
}

void
StringUtils::parse(const std::string &str, std::vector<NamedValue> &v) {
	std::string m(str);
	size_t pos = 0;
        while ((pos = m.find("%26")) != std::string::npos){
            m.replace(pos, 3, "&");
        }

        const std::string s(m);

	parse(Range::fromString(s), v);
}

std::string
StringUtils::urlencode(const std::string &str) {
	return urlencode(Range::fromString(str));
}
	
std::string
StringUtils::urldecode(const std::string &str) {
	return urldecode(Range::fromString(str));
}

HttpDateUtils::HttpDateUtils() {
}

HttpDateUtils::~HttpDateUtils() {
}

std::string
HttpDateUtils::format(time_t value) {

	struct tm ts;
	memset(&ts, 0, sizeof(struct tm));

	if (NULL != gmtime_r(&value, &ts)) {
		char buf[255];
		int res = strftime(buf, sizeof(buf), "%a, %d %b %Y %T GMT", &ts);
		if (0 != res) {
			return std::string(buf, buf + res);
		}
	}
	throw std::runtime_error("failed to format date");
}

time_t
HttpDateUtils::parse(const char *value) {
	
	struct tm ts;
	memset(&ts, 0, sizeof(struct tm));
	
	const char *formats[] = { "%a, %d %b %Y %T GMT", "%A, %d-%b-%y %T GMT", "%a %b %d %T %Y" };
	for (unsigned int i = 0; i < sizeof(formats)/sizeof(const char*); ++i) {
		if (NULL != strptime(value, formats[i], &ts)) {
			return mktime(&ts) - timezone;
		}
	}
	return static_cast<time_t>(0);
}

} // namespace fastcgi
