#include "settings.h"

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include "fastcgi2/config.h"

#ifdef HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

class ConfigTest : public CppUnit::TestFixture
{
public:
	void testConfig();

private:
	CPPUNIT_TEST_SUITE(ConfigTest);
	CPPUNIT_TEST(testConfig);
	CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_REGISTRATION(ConfigTest);

void
ConfigTest::testConfig() {

	using namespace fastcgi;
	
	std::vector<std::string> v;
	std::unique_ptr<Config> config = Config::create("test.conf");
	
	CPPUNIT_ASSERT_EQUAL(10, config->asInt("/fastcgi/daemon/endpoint/backlog"));
	CPPUNIT_ASSERT_EQUAL(std::string("/tmp/example.sock"), config->asString("/fastcgi/daemon/endpoint/socket"));
	
	config->subKeys("/fastcgi/modules/module", v);
	CPPUNIT_ASSERT_EQUAL(static_cast<std::vector<std::string>::size_type>(1), v.size());
}
