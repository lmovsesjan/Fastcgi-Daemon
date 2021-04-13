#include "settings.h"

#include <pthread.h>

#include <cctype>
#include <iterator>
#include <algorithm>
#include <stdexcept>
#include <boost/lexical_cast.hpp>
#include <boost/current_function.hpp>

#include "fastcgi2/logger.h"
#include "fastcgi2/request_io_stream.h"

#include "details/parser.h"
#include "details/request_cache.h"
#include "details/range.h"
#include "details/requestimpl.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace fastcgi
{

static const std::string HEAD("HEAD");
static const std::string HOST_KEY("host");
static const std::string CONTENT_TYPE_KEY("content-type");
static const std::string CONTENT_LENGTH_KEY("content-length");

static const std::string HTTPS_KEY("HTTPS");
static const std::string SERVER_ADDR_KEY("SERVER_ADDR");
static const std::string SERVER_PORT_KEY("SERVER_PORT");

static const std::string PATH_INFO_KEY("PATH_INFO");
static const std::string PATH_TRANSLATED_KEY("PATH_TRANSLATED");

static const std::string SCRIPT_NAME_KEY("SCRIPT_NAME");
static const std::string SCRIPT_FILENAME_KEY("SCRIPT_FILENAME");
static const std::string DOCUMENT_ROOT_KEY("DOCUMENT_ROOT");

static const std::string REMOTE_USER_KEY("REMOTE_USER");
static const std::string REMOTE_ADDR_KEY("REMOTE_ADDR");

static const std::string QUERY_STRING_KEY("QUERY_STRING");
static const std::string REQUEST_METHOD_KEY("REQUEST_METHOD");
static const std::string REQUEST_ID_KEY("REQUEST_ID");

File::File(DataBuffer filename, DataBuffer type, DataBuffer content) :
	data_(content)
{
	if (!type.empty()) {
		type.toString(type_);
	}

	if (!filename.empty()) {
		filename.toString(name_);
	}
	else {
		throw std::runtime_error("uploaded file without name"); 
	}
}

File::File(const std::string &filename, const std::string &type, DataBuffer content) :
	name_(filename), type_(type), data_(content)
{
	if (name_.empty()) {
		throw std::runtime_error("uploaded file without name");
	}
}

const std::string&
File::type() const {
	return type_;
}

const std::string&
File::remoteName() const {
	return name_;
}

DataBuffer
File::data() const {
	return data_;
}

RequestImpl::RequestImpl(Logger *logger, RequestCache *cache) :
	processed_(false), delay_(0), logger_(logger), cache_(cache)
{
	reset();
}

RequestImpl::~RequestImpl() {
}

unsigned short
RequestImpl::getServerPort() const {
	const std::string &res = Parser::get(vars_, SERVER_PORT_KEY);
	return (!res.empty()) ? boost::lexical_cast<unsigned short>(res) : 80;
}

const std::string&
RequestImpl::getHost() const {
	return Parser::get(headers_, HOST_KEY);
}

const std::string&
RequestImpl::getServerAddr() const {
	return Parser::get(vars_, SERVER_ADDR_KEY);
}

const std::string&
RequestImpl::getPathInfo() const {
	return Parser::get(vars_, PATH_INFO_KEY);
}

const std::string&
RequestImpl::getPathTranslated() const {
		return Parser::get(vars_, PATH_TRANSLATED_KEY);
}

const std::string&
RequestImpl::getScriptName() const {
	return Parser::get(vars_, SCRIPT_NAME_KEY);
}

const std::string&
RequestImpl::getScriptFilename() const {
	return Parser::get(vars_, SCRIPT_FILENAME_KEY);
}

const std::string&
RequestImpl::getDocumentRoot() const {
	return Parser::get(vars_, DOCUMENT_ROOT_KEY);
}

const std::string&
RequestImpl::getRemoteUser() const {
	return Parser::get(vars_, REMOTE_USER_KEY);
}

const std::string&
RequestImpl::getRemoteAddr() const {
	return Parser::get(vars_, REMOTE_ADDR_KEY);
}

const std::string&
RequestImpl::getQueryString() const {
	return Parser::get(vars_, QUERY_STRING_KEY);
}

const std::string&
RequestImpl::getRequestMethod() const {
	return Parser::get(vars_, REQUEST_METHOD_KEY);
}

const std::string&
RequestImpl::getRequestId() const {
	return Parser::get(vars_, REQUEST_ID_KEY);
}

std::streamsize
RequestImpl::getContentLength() const {
	const std::string& header = Parser::get(headers_, CONTENT_LENGTH_KEY);
	if (header.empty()) {
		return 0;
	}
	try {
		return boost::lexical_cast<std::streamsize>(header);
	}
	catch (boost::bad_lexical_cast&) {
	}
	return 0;
}

const std::string&
RequestImpl::getContentType() const {
	return Parser::get(headers_, CONTENT_TYPE_KEY);
}

unsigned int
RequestImpl::countArgs() const {
	return args_.size();
}

bool
RequestImpl::hasArg(const std::string &name) const {
	for (std::vector<StringUtils::NamedValue>::const_iterator i = args_.begin(), end = args_.end(); i != end; ++i) {
		if (i->first == name) {
			return true;
		}
	}
	return false;
}

const std::string&
RequestImpl::getArg(const std::string &name) const {
	for (std::vector<StringUtils::NamedValue>::const_iterator i = args_.begin(), end = args_.end(); i != end; ++i) {
		if (i->first == name) {
			return i->second;
		}
	}
	return StringUtils::EMPTY_STRING;
}

void
RequestImpl::getArg(const std::string &name, std::vector<std::string> &v) const {
	
	std::vector<std::string> tmp;
	tmp.reserve(args_.size());
	for (std::vector<StringUtils::NamedValue>::const_iterator i = args_.begin(), end = args_.end(); i != end; ++i) {
		if (i->first == name) {
			tmp.push_back(i->second);
		}
	}
	v.swap(tmp);
}

void
RequestImpl::argNames(std::vector<std::string> &v) const {
	std::set<std::string> names;
	for (std::vector<StringUtils::NamedValue>::const_iterator i = args_.begin(), end = args_.end(); i != end; ++i) {
		names.insert(i->first);
	}
	std::vector<std::string> tmp;
	tmp.reserve(names.size());
	std::copy(names.begin(), names.end(), std::back_inserter(tmp));
	v.swap(tmp);
}

unsigned int
RequestImpl::countHeaders() const {
	return headers_.size();
}

bool
RequestImpl::hasHeader(const std::string &name) const {
	return Parser::has(headers_, name);
}

const std::string&
RequestImpl::getHeader(const std::string &name) const {
	return Parser::get(headers_, name);
}

void
RequestImpl::headerNames(std::vector<std::string> &v) const {
	Parser::keys(headers_, v);
}

unsigned int
RequestImpl::countCookie() const {
	return cookies_.size();
}

bool
RequestImpl::hasCookie(const std::string &name) const {
	return Parser::has(cookies_, name);
}

const std::string&
RequestImpl::getCookie(const std::string &name) const {
	return Parser::get(cookies_, name);
}

void
RequestImpl::cookieNames(std::vector<std::string> &v) const {
	Parser::keys(cookies_, v);
}

bool
RequestImpl::hasFile(const std::string &name) const {
	return files_.find(name) != files_.end();
}

void
RequestImpl::remoteFiles(std::vector<std::string> &v) const {
	std::vector<std::string> tmp;
	tmp.reserve(files_.size());
	for (std::map<std::string, File>::const_iterator i = files_.begin(), end = files_.end(); i != end; ++i) {
		tmp.push_back(i->first);
	}
	v.swap(tmp);
}

const std::string&
RequestImpl::remoteFileName(const std::string &name) const {
	std::map<std::string, File>::const_iterator i = files_.find(name);
	if (files_.end() != i) {
		return i->second.remoteName();
	}
	return StringUtils::EMPTY_STRING;
}

const std::string&
RequestImpl::remoteFileType(const std::string &name) const {
	std::map<std::string, File>::const_iterator i = files_.find(name);
	if (files_.end() != i) {
		return i->second.type();
	}
	return StringUtils::EMPTY_STRING;
}

DataBuffer
RequestImpl::remoteFile(const std::string &name) const {
	std::map<std::string, File>::const_iterator i = files_.find(name);
	if (files_.end() != i) {
		return i->second.data();
	}
	return DataBuffer(body_, 0, 0);
}

bool
RequestImpl::isSecure() const {
	const std::string &val = Parser::get(vars_, HTTPS_KEY);
	return !val.empty() && ("on" == val);
}

DataBuffer
RequestImpl::requestBody() const {
	return body_;
}

void
RequestImpl::setCookie(const Cookie &cookie) {
	if (!headers_sent_) {
		std::pair<std::set<Cookie>::iterator, bool> p = out_cookies_.insert(cookie);
		if (!p.second) {
			Cookie& target = const_cast<Cookie&>(*p.first);
			target = cookie;
		}
	}
	else {
		throw std::runtime_error("Error in RequestImpl::setCookie: headers already sent: cookie - '" + cookie.name() + "=" + cookie.value() + "'");
	}
}

void
RequestImpl::setStatus(unsigned short status) {
	if (!headers_sent_) {
		status_ = status;
	}
	else {
		throw std::runtime_error("Error in RequestImpl::setStatus headers already sent: status - '" + boost::lexical_cast<std::string>(status) + "'");
	}
}

void
RequestImpl::sendError(unsigned short status) {
	if (!headers_sent_) {
		out_cookies_.clear();
		out_headers_.clear();
	}
	else {
		throw std::runtime_error("Error in RequestImpl::setError headers already sent: status - '" + boost::lexical_cast<std::string>(status) + "'");
	}
	status_ = status;
	out_headers_.insert(std::pair<std::string, std::string>("Content-type", "text/html"));
	sendHeadersInternal();
	if (stream_) {
		stream_->write("<html><body><h1>", sizeof("<html><body><h1>") - 1);
		std::string status_str = boost::lexical_cast<std::string>(status);
		stream_->write(status_str.c_str(), status_str.size());
		stream_->write(" ", 1);
		const char* stat = Parser::statusToString(status);
		stream_->write(stat, strlen(stat));
		stream_->write("</h1></body></html>", sizeof("</h1></body></html>") - 1);
	}
}

void
RequestImpl::setHeader(const std::string &name, const std::string &value) {
	if (!headers_sent_) {
		out_headers_[Parser::normalizeOutputHeaderName(name)] = value;
	}
	else {
		throw std::runtime_error("Error in RequestImpl::setCookie: headers already sent: header - '" + name + ": " + value + "'");
	}
}

void
RequestImpl::write(std::streambuf *buf) {
	sendHeaders();
	if (stream_ && HEAD != getRequestMethod()) {
		stream_->write(buf);
	}
}

std::streamsize
RequestImpl::write(const char *buf, std::streamsize size) {
	sendHeaders();
	if (stream_ && HEAD != getRequestMethod()) {
		stream_->write(buf, size);
	}
	return size;
}

std::string
RequestImpl::outputHeader(const std::string &name) const {
	return Parser::get(out_headers_, name);
}

void
RequestImpl::reset() {
	
	status_ = 200;
	stream_ = NULL;
	headers_sent_ = false;

	args_.clear();
	vars_.clear();
	
	files_.clear();
	cookies_.clear();
	headers_.clear();
	out_cookies_.clear();
	out_headers_.clear();
}

void
RequestImpl::sendHeaders() {
	sendHeadersInternal();
}

void
RequestImpl::attach(RequestIOStream *stream, char *env[]) {
	if (NULL == stream) {
		throw std::runtime_error("Stream is NULL");
	}
	if (!env) {
		throw std::runtime_error("ENV is NULL");
	}
	stream_ = stream;
	Parser::parse(this, env, logger_);
	const std::string& query = getQueryString();
	if ("POST" != getRequestMethod() && "PUT" != getRequestMethod()) {
		StringUtils::parse(query, args_);
		return;
	}

	DataBuffer post_buffer;
	std::uint64_t size = getContentLength();
	if (cache_ && size >= cache_->minPostSize()) {
		post_buffer = cache_->create();
		std::uint64_t shift = serializeEnv(post_buffer, size + sizeof(std::uint64_t));
		shift = serializeInt(post_buffer, shift, size);
		body_ = DataBuffer(post_buffer, post_buffer.beginIndex() + shift, post_buffer.endIndex());
	}
	else {
		body_ = DataBuffer::create(StringUtils::EMPTY_STRING.c_str(), 0);
		body_.resize(size);
	}
	std::uint64_t rsz = 0;
	for (DataBuffer::SegmentIterator it = body_.begin(), end;
		 it != end;
		 ++it) {
		rsz += stream_->read(it->first, it->second);
	}
	if (rsz != size) {
		throw std::runtime_error("failed to read request entity");
	}

	if (!query.empty()) {
		StringUtils::parse(query, args_);
	}

	const std::string &type = getContentType();
	if (0 == strncasecmp("multipart/form-data", type.c_str(), sizeof("multipart/form-data") - 1)) {
		std::string boundary = Parser::getBoundary(Range::fromString(type));
		if (!boundary.empty()) {
		    Parser::parseMultipart(this, body_, boundary);
		}
	}
	else {
		if (0 != strncasecmp("text/plain", type.c_str(), sizeof("text/plain") - 1) &&
			0 != strncasecmp("application/octet-stream", type.c_str(), sizeof("application/octet-stream") - 1) &&
			!disablePostParams())
		{
			StringUtils::parse(body_, args_);
		}
	}

	if (!post_buffer.isNil()) {
		std::uint64_t pos = post_buffer.size();
		post_buffer.resize(pos + 2 * sizeof(std::uint64_t) +
			filesSerializedSize() + argsSerializedSize());
		pos = serializeFiles(post_buffer, pos);
		pos = serializeArgs(post_buffer, pos);
	}
}

bool
RequestImpl::disablePostParams() const {
	const std::string& disable_params = Parser::get(vars_, "DISABLE_POST_PARAMS");
	if (disable_params.empty()) {
		return false;
	}
	return 0 == strcasecmp("yes", disable_params.c_str()) ||
		   0 == strcasecmp("true", disable_params.c_str()) ||
		   0 == strcasecmp("1", disable_params.c_str());
}

void
RequestImpl::sendHeadersInternal() {
	if (!headers_sent_) {
		std::stringstream stream;
		stream << status_ << " " << Parser::statusToString(status_);
		out_headers_["Status"] = stream.str();
		if (stream_) {
			for (HeaderMap::iterator i = out_headers_.begin(), end = out_headers_.end(); i != end; ++i) {
				stream_->write(i->first.c_str(), i->first.size());
				stream_->write(": ", 2);
				stream_->write(i->second.c_str(), i->second.size());
				stream_->write("\r\n", 2);
			}
			for (std::set<Cookie>::const_iterator i = out_cookies_.begin(), end = out_cookies_.end(); i != end; ++i) {
				stream_->write("Set-Cookie: ", sizeof("Set-Cookie: ") - 1);
				std::string cookie = i->toString();
				stream_->write(cookie.c_str(), cookie.size());
				stream_->write("\r\n", 2);
			}
			stream_->write("\r\n", 2);
		}
		headers_sent_ = true;
	}
}

bool RequestImpl::isProcessed() const {
	return processed_;
}

void
RequestImpl::markAsProcessed () {
	processed_ = true;
}

void
RequestImpl::tryAgain(time_t delay) {
	delay_ = delay;
}

std::uint64_t
RequestImpl::serializeEnv(DataBuffer &buffer, std::uint64_t add_size) {
	std::uint64_t header_size = 0;
	for (HeaderMap::iterator it = headers_.begin(), end = headers_.end();
		 it != end;
		 ++it) {
		header_size += it->first.size();
		header_size += it->second.size();
		header_size += 2*sizeof(std::uint64_t);
	}
	std::uint64_t cookie_size = 0;
	for (VarMap::iterator it = cookies_.begin(), end = cookies_.end();
		 it != end;
		 ++it) {
		cookie_size += it->first.size();
		cookie_size += it->second.size();
		cookie_size += 2*sizeof(std::uint64_t);
	}
	std::uint64_t var_size = 0;
	for (VarMap::iterator it = vars_.begin(), end = vars_.end();
		 it != end;
		 ++it) {
		var_size += it->first.size();
		var_size += it->second.size();
		var_size += 2*sizeof(std::uint64_t);
	}
	buffer.resize(add_size + header_size + cookie_size +
		var_size + 3*sizeof(std::uint64_t));

	std::uint64_t pos = 0;
	pos = serializeInt(buffer, pos, header_size);
	for (HeaderMap::iterator it = headers_.begin(), end = headers_.end();
		 it != end;
		 ++it) {
		pos = serializeString(buffer, pos, it->first);
		pos = serializeString(buffer, pos, it->second);
	}
	pos = serializeInt(buffer, pos, cookie_size);
	for (VarMap::iterator it = cookies_.begin(), end = cookies_.end();
		 it != end;
		 ++it) {
		pos = serializeString(buffer, pos, it->first);
		pos = serializeString(buffer, pos, it->second);
	}
	pos = serializeInt(buffer, pos, var_size);
	for (VarMap::iterator it = vars_.begin(), end = vars_.end();
		 it != end;
		 ++it) {
		pos = serializeString(buffer, pos, it->first);
		pos = serializeString(buffer, pos, it->second);
	}

	return pos;
}

std::uint64_t
RequestImpl::serializeInt(DataBuffer &buffer, std::uint64_t pos,
	std::uint64_t val) {
	pos += buffer.write(pos, (char*)&val, sizeof(val));
	return pos;
}

std::uint64_t
RequestImpl::serializeString(DataBuffer &buffer, std::uint64_t pos,
	const std::string &val) {
	std::uint64_t size = val.size();
	pos = serializeInt(buffer, pos, size);
	pos += buffer.write(pos, val.c_str(), size);
	return pos;
}

std::uint64_t
RequestImpl::serializeBuffer(DataBuffer &buffer, std::uint64_t pos,
	const DataBuffer &src) {
	pos = serializeInt(buffer, pos, src.size());
	for (DataBuffer::SegmentIterator it = src.begin(), end;
		 it != end;
		 ++it) {
		std::pair<char*, std::uint64_t> chunk = *it;
		pos += buffer.write(pos, chunk.first, chunk.second);
	}
	return pos;
}

std::uint64_t
RequestImpl::filesSerializedSize() {
	std::uint64_t file_size = 0;
	for (std::map<std::string, File>::iterator it = files_.begin(), end = files_.end();
		 it != end;
		 ++it) {
		file_size += sizeof(std::uint64_t);
		file_size += it->first.size();
		file_size += sizeof(std::uint64_t);
		file_size += it->second.remoteName().size();
		file_size += sizeof(std::uint64_t);
		file_size += it->second.type().size();
		file_size += 2 * sizeof(std::uint64_t);
	}
	return file_size;
}

std::uint64_t
RequestImpl::argsSerializedSize() {
	std::uint64_t arg_size = 0;
	for (std::vector<StringUtils::NamedValue>::iterator it = args_.begin(),
			end = args_.end();
		 it != end;
		 ++it) {
		arg_size += it->first.size();
		arg_size += it->second.size();
		arg_size += 2 * sizeof(std::uint64_t);
	}
	return arg_size;
}

void
RequestImpl::serialize(DataBuffer &buffer) {
	std::uint64_t add_size = 0;
	add_size += sizeof(std::uint64_t);
	add_size += body_.size();
	add_size += sizeof(std::uint64_t);
	std::uint64_t file_size = filesSerializedSize();
	add_size += file_size;
	add_size += sizeof(std::uint64_t);
	std::uint64_t arg_size = argsSerializedSize();
	add_size += arg_size;
	std::uint64_t pos = serializeEnv(buffer, add_size);
	pos = serializeBuffer(buffer, pos, body_);
	pos = serializeFiles(buffer, pos);
	pos = serializeArgs(buffer, pos);
}

std::uint64_t
RequestImpl::serializeFiles(DataBuffer &buffer, std::uint64_t pos) {
	std::uint64_t file_size = filesSerializedSize();
	pos = serializeInt(buffer, pos, file_size);
	for (std::map<std::string, File>::iterator it = files_.begin(), end = files_.end();
		 it != end;
		 ++it) {
		pos = serializeString(buffer, pos, it->first);
		pos = serializeString(buffer, pos, it->second.remoteName());
		pos = serializeString(buffer, pos, it->second.type());
		DataBuffer file = it->second.data();
		pos = serializeInt(buffer, pos, file.beginIndex() - body_.beginIndex());
		pos = serializeInt(buffer, pos, file.endIndex() - file.beginIndex());
	}
	return pos;
}

std::uint64_t
RequestImpl::serializeArgs(DataBuffer &buffer, std::uint64_t pos) {
	std::uint64_t arg_size = argsSerializedSize();
	pos = serializeInt(buffer, pos, arg_size);
	for (std::vector<StringUtils::NamedValue>::iterator it = args_.begin(),
			end = args_.end();
		 it != end;
		 ++it) {
		pos = serializeString(buffer, pos, it->first);
		pos = serializeString(buffer, pos, it->second);
	}
	return pos;
}

std::uint64_t
RequestImpl::parseInt(DataBuffer buffer, std::uint64_t pos,
	std::uint64_t &val) {
	pos += buffer.read(pos, (char*)&val, sizeof(val));
	return pos;
}

std::uint64_t
RequestImpl::parseString(DataBuffer buffer, std::uint64_t pos, std::string &val) {
	std::uint64_t len = 0;
	pos = parseInt(buffer, pos, len);
	val.resize(len);
	pos += buffer.read(pos, (char*)&(val[0]), len);
	return pos;
}

std::uint64_t
RequestImpl::parseHeaders(DataBuffer buffer, std::uint64_t pos) {
    std::uint64_t field_size = 0;
    pos = parseInt(buffer, pos, field_size);
    std::uint64_t pos_end = pos + field_size;
    if (pos_end > buffer.size()) {
    	throw std::runtime_error("Cannot parse request headers");
    }
    while (pos < pos_end) {
		std::string name, value;
		pos = parseString(buffer, pos, name);
		pos = parseString(buffer, pos, value);
		headers_.insert(std::make_pair(name, value));
    }
    return pos;
}

std::uint64_t
RequestImpl::parseCookies(DataBuffer buffer, std::uint64_t pos) {
    std::uint64_t field_size = 0;
    pos = parseInt(buffer, pos, field_size);
    std::uint64_t pos_end = pos + field_size;
    if (pos_end > buffer.size()) {
    	throw std::runtime_error("Cannot parse request cookies");
    }
    while (pos < pos_end) {
		std::string name, value;
		pos = parseString(buffer, pos, name);
		pos = parseString(buffer, pos, value);
		cookies_.insert(std::make_pair(name, value));
    }
    return pos;
}

std::uint64_t
RequestImpl::parseVars(DataBuffer buffer, std::uint64_t pos) {
	std::uint64_t field_size = 0;
	pos = parseInt(buffer, pos, field_size);
	std::uint64_t pos_end = pos + field_size;
	if (pos_end > buffer.size()) {
		throw std::runtime_error("Cannot parse request vars");
	}
	while (pos < pos_end) {
		std::string name, value;
		pos = parseString(buffer, pos, name);
		pos = parseString(buffer, pos, value);
		vars_.insert(std::make_pair(name, value));
	}
	return pos;
}

std::uint64_t
RequestImpl::parseBody(DataBuffer buffer, std::uint64_t pos) {
	std::uint64_t field_size = 0;
	pos = parseInt(buffer, pos, field_size);
	std::uint64_t pos_end = pos + field_size;
	if (pos_end > buffer.size()) {
		throw std::runtime_error("Cannot parse request body");
	}
	body_ = DataBuffer(buffer, pos + buffer.beginIndex(), pos_end + buffer.beginIndex());
	return pos_end;
}

std::uint64_t
RequestImpl::parseFiles(DataBuffer buffer, std::uint64_t pos) {
	std::uint64_t field_size = 0;
	pos = parseInt(buffer, pos, field_size);
	std::uint64_t pos_end = pos + field_size;
	if (pos_end > buffer.size()) {
		throw std::runtime_error("Cannot parse request files");
	}
	while (pos < pos_end) {
		std::string name, remote_name, type;
		pos = parseString(buffer, pos, name);
		pos = parseString(buffer, pos, remote_name);
		pos = parseString(buffer, pos, type);
		std::uint64_t offset = 0;
		std::uint64_t length = 0;
		pos = parseInt(buffer, pos, offset);
		pos = parseInt(buffer, pos, length);
		DataBuffer file_buffer = DataBuffer(body_, offset + body_.beginIndex(),
			offset + length + body_.beginIndex());
		files_.insert(std::make_pair(name, File(remote_name, type, file_buffer)));
	}
	return pos;
}

std::uint64_t
RequestImpl::parseArgs(DataBuffer buffer, std::uint64_t pos) {
	std::uint64_t field_size = 0;
	pos = parseInt(buffer, pos, field_size);
	std::uint64_t pos_end = pos + field_size;
	if (pos_end > buffer.size()) {
		throw std::runtime_error("Cannot parse request args");
	}
	while (pos < pos_end) {
		std::string name, value;
		pos = parseString(buffer, pos, name);
		pos = parseString(buffer, pos, value);
		if (std::string::npos != value.find('\r') || std::string::npos != value.find('\n') ||
			std::string::npos != value.find('\0')) {
			value = StringUtils::urlencode(Range::fromString(value));
		}

		args_.push_back(std::make_pair(name, value));
	}
	return pos;
}

void
RequestImpl::parse(DataBuffer buffer) {
	std::uint64_t pos = parseHeaders(buffer, 0);
	pos = parseCookies(buffer, pos);
	pos = parseVars(buffer, pos);
	pos = parseBody(buffer, pos);
	pos = parseFiles(buffer, pos);
	pos = parseArgs(buffer, pos);
}

void
RequestImpl::saveToCache(Request *request) {
	if (cache_) {
		cache_->save(request, delay_);
	}
}

unsigned short
RequestImpl::status() const {
	return status_;
}

void
RequestImpl::flush() {
	if (stream_) {
		stream_->flush();
	}
}

} // namespace fastcgi
