#include "settings.h"

#include <sstream>
#include <fstream>
#include <cppunit/TestFixture.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

#include "fastcgi2/component.h"
#include "fastcgi2/config.h"
#include "fastcgi2/logger.h"
#include "fastcgi2/request.h"
#include "fastcgi2/request_io_stream.h"

#include "details/componentset.h"
#include "details/globals.h"
#include "details/request_cache.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

namespace fastcgi {

class RequestTest : public CppUnit::TestFixture
{
public:
	RequestTest();

	void testGet();
	void testEmptyGet();
	void testPost();
	void testCookie();
	void testMultipartN();
	void testMultipartRN();
	void testMultipartRN2();

private:
	void testPostImpl(RequestCache* cache);
	void testMultipartNImpl(RequestCache* cache);
	void testMultipartRNImpl(RequestCache* cache);
	void testMultipartRN2Impl(RequestCache* cache);

private:
	std::unique_ptr<Logger> logger_;

	CPPUNIT_TEST_SUITE(RequestTest);
	CPPUNIT_TEST(testGet);
	CPPUNIT_TEST(testEmptyGet);
	CPPUNIT_TEST(testPost);
	CPPUNIT_TEST(testCookie);
	CPPUNIT_TEST(testMultipartN);
	CPPUNIT_TEST(testMultipartRN);
	CPPUNIT_TEST(testMultipartRN2);
	CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_REGISTRATION(RequestTest);

class TestIOStream : public RequestIOStream {
public:
	TestIOStream(std::istream *in, std::ostream *out) : in_(in), out_(out)
	{}
	virtual int read(char *buf, int size) {
		in_->read(buf, size);
		return in_->gcount();
	}
	virtual int write(const char *buf, int size) {
		out_->write(buf, size);
		return size;
	}
	virtual void write(std::streambuf *buf) {
		(*out_) << buf;
	}
	virtual void flush() {
	}
private:
	std::istream *in_;
	std::ostream *out_;
};

RequestTest::RequestTest() : logger_(new BulkLogger) {
}

void
RequestTest::testEmptyGet() {
	char *env[] = { "REQUEST_METHOD=GET", "QUERY_STRING=", "HTTP_HOST=yandex.ru", NULL };

	std::unique_ptr<Request> req(new Request(logger_.get(), NULL));

	std::stringstream in, out;
	TestIOStream stream(&in, &out);
	req->attach(&stream, env);

	CPPUNIT_ASSERT_EQUAL(std::string("GET"), req->getRequestMethod());

	CPPUNIT_ASSERT_EQUAL(std::string("yandex.ru"), req->getHeader("Host"));
}

void
RequestTest::testGet() {
	char *env[] = { "REQUEST_METHOD=GET", "QUERY_STRING=test=pass&success=try%20again", "HTTP_HOST=yandex.ru", NULL };
	std::unique_ptr<Request> req(new Request(logger_.get(), NULL));

	std::stringstream in, out;
	TestIOStream stream(&in, &out);

	req->attach(&stream, env);

	CPPUNIT_ASSERT_EQUAL(std::string("GET"), req->getRequestMethod());
	CPPUNIT_ASSERT_EQUAL(std::string("yandex.ru"), req->getHeader("Host"));
	CPPUNIT_ASSERT_EQUAL(std::string("test=pass&success=try%20again"), req->getQueryString());
	CPPUNIT_ASSERT_EQUAL(std::string("pass"), req->getArg("test"));
	CPPUNIT_ASSERT_EQUAL(std::string("try again"), req->getArg("success"));
}

void
RequestTest::testCookie() {
	char *env[] = { "REQUEST_METHOD=GET", "QUERY_STRING=test=pass&success=try%20again", "HTTP_HOST=yandex.ru", 
		"HTTP_COOKIE=yandexuid=921562781154947430; yandex_login=highpower; my=Yx4CAAA", NULL };
	
	std::unique_ptr<Request> req(new Request(logger_.get(), NULL));
	std::stringstream in, out;
	TestIOStream stream(&in, &out);
	req->attach(&stream, env);
	
	CPPUNIT_ASSERT_EQUAL(std::string("GET"), req->getRequestMethod());
	
	CPPUNIT_ASSERT_EQUAL(std::string("yandex.ru"), req->getHeader("Host"));
	CPPUNIT_ASSERT_EQUAL(std::string("test=pass&success=try%20again"), req->getQueryString());

	CPPUNIT_ASSERT_EQUAL(std::string("pass"), req->getArg("test"));
	CPPUNIT_ASSERT_EQUAL(std::string("try again"), req->getArg("success"));
	
	CPPUNIT_ASSERT_EQUAL(std::string("Yx4CAAA"), req->getCookie("my"));
	CPPUNIT_ASSERT_EQUAL(std::string("highpower"), req->getCookie("yandex_login"));
	CPPUNIT_ASSERT_EQUAL(std::string("921562781154947430"), req->getCookie("yandexuid"));
}

void
RequestTest::testPost() {
	RequestTest::testPostImpl((RequestCache*)NULL);
}

void
RequestTest::testMultipartN() {
	testMultipartNImpl((RequestCache*)NULL);
}

void
RequestTest::testMultipartRN() {
	testMultipartRNImpl((RequestCache*)NULL);
}

void
RequestTest::testMultipartRN2() {
	testMultipartRN2Impl((RequestCache*)NULL);
}

void
RequestTest::testPostImpl(RequestCache *cache) {
	char *env[] = { "REQUEST_METHOD=POST", "HTTP_CONTENT_LENGTH=29", "HTTP_HOST=yandex.ru", NULL };
	std::unique_ptr<Request> req(new Request(logger_.get(), cache));

	std::stringstream in("test=pass&success=try%20again"), out;
	TestIOStream stream(&in, &out);
	req->attach(&stream, env);

	CPPUNIT_ASSERT_EQUAL(std::string("POST"), req->getRequestMethod());

	CPPUNIT_ASSERT_EQUAL(std::string("yandex.ru"), req->getHeader("Host"));
	CPPUNIT_ASSERT_EQUAL(std::string(""), req->getQueryString());

	CPPUNIT_ASSERT_EQUAL(std::string("pass"), req->getArg("test"));
	CPPUNIT_ASSERT_EQUAL(std::string("try again"), req->getArg("success"));
}

void
RequestTest::testMultipartNImpl(RequestCache *cache) {
	std::unique_ptr<Request> req(new Request(logger_.get(), cache));

	char *env[] = { "REQUEST_METHOD=POST", "HTTP_HOST=yandex.ru", "HTTP_CONTENT_LENGTH=1508", 
		"CONTENT_TYPE=multipart/form-data; boundary=---------------------------15403834263040891721303455736", NULL };

	std::fstream f("multipart-test-n.dat");
	std::stringstream out;
	TestIOStream stream(&f, &out);
	req->attach(&stream, env);

	CPPUNIT_ASSERT_EQUAL(3u, req->countArgs());
	CPPUNIT_ASSERT_EQUAL(std::string("test"), req->getArg("username"));
	CPPUNIT_ASSERT_EQUAL(std::string("test"), req->getArg("filename"));
	CPPUNIT_ASSERT_EQUAL(std::string("file content"), req->getArg("some_long_long_long_long_long_long_filename.ext"));

	CPPUNIT_ASSERT_EQUAL(std::string("POST"), req->getRequestMethod());
	CPPUNIT_ASSERT_EQUAL(std::string("yandex.ru"), req->getHeader("Host"));
	
	CPPUNIT_ASSERT_EQUAL(true, req->hasFile("uploaded"));
	DataBuffer file = req->remoteFile("uploaded");

	CPPUNIT_ASSERT_EQUAL((std::uint64_t)887, file.size());
}

void
RequestTest::testMultipartRNImpl(RequestCache *cache) {
	std::unique_ptr<Request> req(new Request(logger_.get(), cache));

	char *env[] = { "REQUEST_METHOD=POST", "HTTP_HOST=yandex.ru", "HTTP_CONTENT_LENGTH=1361", 
		"CONTENT_TYPE=multipart/form-data; boundary=\"---------------------------15403834263040891721303455736\"", NULL };

	std::fstream f("multipart-test-rn.dat");
	std::stringstream out;
	TestIOStream stream(&f, &out);
	req->attach(&stream, env);

	CPPUNIT_ASSERT_EQUAL(2u, req->countArgs());
	CPPUNIT_ASSERT_EQUAL(std::string("test"), req->getArg("username"));
	CPPUNIT_ASSERT_EQUAL(std::string("test"), req->getArg("filename"));

	CPPUNIT_ASSERT_EQUAL(std::string("POST"), req->getRequestMethod());
	CPPUNIT_ASSERT_EQUAL(std::string("yandex.ru"), req->getHeader("Host"));
	
	CPPUNIT_ASSERT_EQUAL(true, req->hasFile("uploaded"));
	DataBuffer file = req->remoteFile("uploaded");
	CPPUNIT_ASSERT_EQUAL((std::uint64_t)887, file.size());
}

void
RequestTest::testMultipartRN2Impl(RequestCache *cache) {
	std::unique_ptr<Request> req(new Request(logger_.get(), cache));

	char *env[] = { "REQUEST_METHOD=POST", "HTTP_HOST=yandex.ru", "HTTP_CONTENT_LENGTH=485",
		"CONTENT_TYPE=multipart/form-data; boundary=\"AaB03x\"", NULL };

	std::fstream f("multipart-test-rn2.dat");
	std::stringstream out;
	TestIOStream stream(&f, &out);
	req->attach(&stream, env);

	CPPUNIT_ASSERT_EQUAL(2u, req->countArgs());
	CPPUNIT_ASSERT_EQUAL(std::string("Joe Blow"), req->getArg("field1"));
    CPPUNIT_ASSERT_EQUAL(true, req->hasArg("pics"));

	CPPUNIT_ASSERT_EQUAL(std::string("POST"), req->getRequestMethod());
	CPPUNIT_ASSERT_EQUAL(std::string("yandex.ru"), req->getHeader("Host"));
}

} // namespace fastcgi
