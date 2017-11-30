#include "settings.h"

#include <set>
#include <cctype>
#include <cassert>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <stdexcept>
#include <boost/bind.hpp>

#include "fastcgi2/logger.h"

#include "details/range.h"
#include "details/parser.h"
#include "details/requestimpl.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace fastcgi
{

const Range Parser::RETURN_N_RANGE = Range::fromChars("\n");
const Range Parser::RETURN_RN_RANGE = Range::fromChars("\r\n");

const Range Parser::EMPTY_LINE_NN_RANGE = Range::fromChars("\n\n");
const Range Parser::EMPTY_LINE_RNRN_RANGE = Range::fromChars("\r\n\r\n");

const Range Parser::NAME_RANGE = Range::fromChars("name");
const Range Parser::FILENAME_RANGE = Range::fromChars("filename");
const Range Parser::HEADER_RANGE = Range::fromChars("HTTP_");
const Range Parser::COOKIE_RANGE = Range::fromChars("HTTP_COOKIE");
const Range Parser::CONTENT_TYPE_RANGE = Range::fromChars("CONTENT_TYPE");

const char*
Parser::statusToString(short status) {
	
	switch (status) {
		case 200: return "OK";
		case 201: return "Created";
		case 202: return "Accepted";
		case 203: return "Partial Information";
		case 204: return "No Response";

		case 301: return "Moved";
		case 302: return "Moved Temporarily";
		case 303: return "Method";
		case 304: return "Not Modified";

		case 400: return "Bad request";
		case 401: return "Unauthorized";
		case 402: return "PaymentRequired";
		case 403: return "Forbidden";
		case 404: return "Not found";
		case 405: return "Method not allowed";

		case 500: return "Internal Error";
		case 501: return "Not implemented";
		case 502: return "Bad Gateway";
		case 503: return "Service Unavailable";
	}
	return "Unknown status";
}

std::string
Parser::getBoundary(const Range &range) {
	
	Range head, tail;
	range.split(';', head, tail);
	
	tail = tail.trim();
	if (strncasecmp("boundary", tail.begin(), sizeof("boundary") - 1) == 0) {
		Range key, value;
		tail.split('=', key, value);
		Range boundary = value.trim();
		Range quote = Range::fromChars("\"");
		if (!boundary.empty() && boundary.startsWith(quote)) {
			if (boundary.size() < 2 || !boundary.endsWith(quote)) {
				throw std::runtime_error("incomplete boundary quotation");
			}
			boundary = boundary.trimn(1, 1);
		}
		return std::string("--").append(boundary.begin(), boundary.end());
	}
	return StringUtils::EMPTY_STRING;
}

void
Parser::addCookie(RequestImpl *req, const Range &range) {
	Range tmp = range.trim(), head, tail;
	tmp.split('=', head, tail);
	if (!head.empty()) {
		req->cookies_[StringUtils::urldecode(head)] = StringUtils::urldecode(tail);
	}
}

void
Parser::addHeader(RequestImpl *req, const Range &key, const Range &value) {
	req->headers_[normalizeInputHeaderName(key)] = value.toString();
}

void
Parser::parse(RequestImpl *req, char *env[], Logger* logger) {
	for (int i = 0; NULL != env[i]; ++i) {
		logger->debug("env[%d] = %s", i, env[i]);
		Range key, value;
		Range::fromChars(env[i]).split('=', key, value);
		if (COOKIE_RANGE == key) {
			parseCookies(req, value);
			addHeader(req, key.trimn(HEADER_RANGE.size(), 0), value.trim());
		}
		else if (CONTENT_TYPE_RANGE == key) {
			addHeader(req, key, value.trim());
		}
		else if (key.startsWith(HEADER_RANGE)) {
			addHeader(req, key.trimn(HEADER_RANGE.size(), 0), value.trim());
		}
		else {
			req->vars_[key.toString()] = value.toString();
		}
	}
}

void
Parser::parseCookies(RequestImpl *req, const Range &range) {
	Range tmp = range.trim(), delim = Range::fromChars("; ");
	while (!tmp.empty()) {
		Range head, tail;
		tmp.split(delim, head, tail);
		addCookie(req, head);
		tmp = tail.trim();
	}
}

static const std::string EMPTY_LINE_RNRN_STRING = "\r\n\r\n";
static const std::string EMPTY_LINE_NN_STRING = "\n\n";
static const std::string RETURN_RN_STRING = "\r\n";
static const std::string RETURN_N_STRING = "\n";
static const std::string ONE_SPACE_STRING = " ";
static const std::string ONE_TAB_STRING = "\t";
static const std::string MINUS_PREFIX_STRING = "--";
static const std::string NAME_STRING = "name";
static const std::string FILENAME_STRING = "filename";
static const std::string CONTENT_TYPE_STRING = "CONTENT_TYPE";
static const std::string CONTENT_TYPE_HEADER = "Content-Type";

void
Parser::parseLine(DataBuffer line, DataBuffer &name, DataBuffer &filename, DataBuffer &type) {
	while (!line.empty()) {
		DataBuffer head, tail, key, value;
		line.split(';', head, tail);
		head.split('=', key, value);
		if (NAME_STRING.size() == key.size() && key.startsWith(NAME_STRING)) {
			name = value.trimn(1, 1);
		}
		else if (FILENAME_STRING.size() == key.size() && key.startsWith(FILENAME_STRING)) {
			filename = value.trimn(1, 1);
		}
		else if (CONTENT_TYPE_STRING.size() == key.size() && key.startsWithCI(CONTENT_TYPE_STRING)) {
			type = value.trim();
		}
		else {
			head.split(':', key, value);

			if (CONTENT_TYPE_HEADER.size() == key.size() && key.startsWith(CONTENT_TYPE_HEADER)) {
				type = value.trim();
			}
		}
		line = tail.trim();
	}
}

void
Parser::parsePart(RequestImpl *req, DataBuffer part) {
	DataBuffer headers, content;
	DataBuffer name, filename, type;
	if (!part.split(EMPTY_LINE_RNRN_STRING, headers, content)) {
		part.split(EMPTY_LINE_NN_STRING, headers, content);
	}
	while (!headers.empty()) {
		DataBuffer line;
		DataBuffer tail = headers;
		bool lineFound = false;
		while (!tail.empty()) {
			DataBuffer subline, subtail;
			if (!tail.split(RETURN_RN_STRING, subline, subtail)) {
				tail.split('\n', subline, subtail);
			}
			if (subline.startsWith(ONE_SPACE_STRING) || subline.startsWith(ONE_TAB_STRING)) {
				line = DataBuffer(part, line.beginIndex(), subline.endIndex());
			}
			else {
				if (lineFound) {
					break;
				}
				line = subline;
			}
			tail = subtail;
			lineFound = true;
		}
		parseLine(line, name, filename, type);
		headers = tail;
	}

	if (name.empty()) {
		return;
	} 

	std::string name_str;
	name.toString(name_str);

	if (!filename.empty()) {
		req->files_.insert(std::make_pair(name_str, File(filename, type, content)));
	}
	else {
		std::string arg;
		content.toString(arg);
		req->args_.push_back(std::make_pair(name_str, arg));
	}
}

void
Parser::parseMultipart(RequestImpl *req, DataBuffer data, const std::string &boundary) {
	DataBuffer head, tail;
	while (!data.empty()) {
		if (data.split(boundary, head, tail) && !head.empty()) {
			if (head.endsWith(RETURN_RN_STRING)) {
				head = head.trimn(0, 2);
			}
			else if (head.endsWith(RETURN_N_STRING)) {
				head = head.trimn(0, 1);
			}
			else {
				throw std::runtime_error("malformed multipart message");
			}
			if (head.startsWith(RETURN_RN_STRING)) {
				head = head.trimn(2, 0);
			}
			else if (head.startsWith(RETURN_N_STRING)) {
				head = head.trimn(1, 0);
			}
			else {
				throw std::runtime_error("malformed multipart message");
			}
		}
		if (!head.empty()) {
			parsePart(req, head);
		}
		if (tail.startsWith(MINUS_PREFIX_STRING)) {
			break;
		}
		data = tail;
	}
}

std::string
Parser::normalizeInputHeaderName(const Range &range) {
	
	std::string res;
	res.reserve(range.size());
	
	Range tmp = range;
	while (!tmp.empty()) {
		Range head, tail;
		bool splitted = tmp.split('_', head, tail);
		res.append(head.begin(), head.end());
		if (splitted) {
			res.append(1, '-');
		}
		tmp = tail;
	}
	return res;
}

std::string
Parser::normalizeOutputHeaderName(const std::string &name) {
	
	std::string res;
	Range range = Range::fromString(name).trim();
	res.reserve(range.size());
	
	while (!range.empty()) {
		
		Range head, tail;
		range.split('-', head, tail);
		
		if (!head.empty()) {
			res.append(1, static_cast<char>(toupper(head[0])));
		}
		if (head.size() > 1) {
			res.append(head.begin() + 1, head.end());
		}
		range = tail.trim();
		if (!range.empty()) {
			res.append(1, '-');
		}
	}
	return res;
}

} // namespace fastcgi
