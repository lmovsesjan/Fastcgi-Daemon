AC_INIT([fastcgi2], [2.0-1], [toshik@yandex-team.ru])
AM_INIT_AUTOMAKE([foreign])

AC_CONFIG_MACRO_DIR([config])

AC_GNU_SOURCE
AC_CONFIG_HEADERS(config/settings.h)

AC_PROG_CXX
AC_PROG_CXXCPP
AC_PROG_LIBTOOL

AC_LANG([C++])
AM_MAINTAINER_MODE

PKG_CHECK_MODULES(yandex_platform, [yandex-platform], [], [echo yandex-platform not found])

LIBS="$yandex_platform_LIBS"
CPPFLAGS="$yandex_platform_CFLAGS -pthread"

AC_SYS_LARGEFILE
AC_CXX_GNUCXX_HASHMAP
AC_CXX_STLPORT_HASHMAP
AC_CXX_HAVE_EXT_HASH_MAP

PKG_CHECK_MODULES(xml, [libxml-2.0], [], 
	AC_MSG_ERROR([libxml not found]))

AX_CHECK_FASTCGI([],
	AC_MSG_ERROR([libfcgi not found]))

AX_CHECK_DMALLOC([], 
	AC_MSG_WARN([dmalloc library not found]))

AX_CHECK_CPPUNIT([1.10.0], [], 
	AC_MSG_WARN([cppunit library not found. compilation of tests disabled]))

have_va_copy="yes"
AC_MSG_CHECKING([trying fo find to find va_copy macro])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
	[[#include <cstdarg>]],
	[[ va_list args1; va_list args2; va_copy(args1, args2); ]])],
	[ have_va_copy="yes" ],
	[ have_va_copy="no" ])
AC_MSG_RESULT([$have_va_copy])

have_nonstandard_va_copy="yes"
AC_MSG_CHECKING([trying fo find to find __va_copy macro])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
	[[#include <cstdarg>]],
	[[ va_list args1; va_list args2; __va_copy(args1, args2); ]])],
	[ have_nonstandard_va_copy="yes" ],
	[ have_nonstandard_va_copy="no" ])
AC_MSG_RESULT([$have_nonstandard_va_copy])

if test "f$have_va_copy" = "fyes"; then
	AC_DEFINE(HAVE_VA_COPY, 1, [define to 1 if you have va_copy macro definition])
elif test "f$have_nonstandard_va_copy" = "fyes"; then
	AC_DEFINE(HAVE_NONSTANDARD_VA_COPY, 1, [define to 1 if you have __va_copy macro definition])
else
	AC_MSG_ERROR([can not find va_copy or similar])
fi

AX_BOOST_BASE([1.30])
AX_BOOST_THREAD

if test "f$BOOST_THREAD_LDFLAGS" == "f"
then
    AC_MSG_ERROR([boost thread lib not found, unable to compile])
fi

AX_BOOST_REGEX
if test "f$BOOST_REGEX_LDFLAGS" == "f"
then
    AC_MSG_ERROR([boost regex lib not found, unable to compile])
fi

for i in -W -Wall -Wextra -fexceptions -frtti -ftemplate-depth-128; do
	AX_CHECK_COMPILER_FLAGS([$i], [CXXFLAGS="$CXXFLAGS $i"], [])
done

LIBS=$stored_libs

AC_CONFIG_FILES([Makefile include/Makefile include/fastcgi2/Makefile 
	include/details/Makefile library/Makefile main/Makefile tests/Makefile 
	example/Makefile syslog/Makefile statistics/Makefile
	file-logger/Makefile])

AC_OUTPUT
