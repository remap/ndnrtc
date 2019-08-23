#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#

AC_PREREQ([2.69])
AC_INIT([NDN-RTC],[v4.0.0],[peter@remap.ucla.edu],[ndnrtc],[https://github.com/remap/ndnrtc])
AM_INIT_AUTOMAKE([1.10 -Wall no-define foreign subdir-objects])
AC_CONFIG_MACRO_DIR([m4])

AC_PREFIX_DEFAULT([/usr/local])
AC_CONFIG_SRCDIR([include/ndnrtc-common.hpp])
AC_CONFIG_FILES([Makefile])
AM_MAINTAINER_MODE
AM_SILENT_RULES([yes])

AC_ARG_ENABLE([logging], [AS_HELP_STRING([--disable-logging],[disable logging in library])],
	[],
	[
	AC_DEFINE([NDN_LOGGING])
	AC_DEFINE([NDN_TRACE])
	AC_DEFINE([NDN_INFO])
	AC_DEFINE([NDN_WARN])
	AC_DEFINE([NDN_ERROR])
	AC_DEFINE([NDN_DEBUG])
	])

# Checks for programs.
AC_CANONICAL_HOST
AC_PROG_CC
AC_PROG_CXX
AC_PROG_OBJC
AC_PROG_OBJCXX
AM_PROG_CC_C_O
AM_PROG_AR
LT_INIT
AC_LANG([C++])
LT_PREREQ([2.2])
LT_INIT()

# checking platform
case $host_os in
  darwin* )
		# Do something specific for MacOS
		# Special case.
		case "$host" in
		  *-darwin* | *-macos10*)
			if test -d /sw ; then
				CFLAGS="$CFLAGS -I/sw/include"
				CXXFLAGS="$CXXFLAGS -I/sw/include"
				CPPFLAGS="$CPPFLAGS -I/sw/include"
				LDFLAGS="$LDFLAGS -L/sw/lib"
			fi
			;;
		esac
		os_module=darwin
		;;
  linux-android*)
  		os_module=android
  		;;
  linux*)
		# Do something specific for linux
		os_module=linux
		;;
   *BSD*)
		# Do something specific for BSD
		os_module=bsd
		;;
    *)
		#Default Case
		AC_MSG_ERROR([Your platform is not currently supported])
		;;
esac

# setting conditionals for automake
AM_CONDITIONAL([OS_DARWIN],  [test $os_module = darwin])
AM_CONDITIONAL([OS_LINUX],  [test $os_module = linux])
AM_CONDITIONAL([OS_ANDROID],  [test $os_module = android])
AM_CONDITIONAL([OS_BSD],  [test $os_module = bsd])

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AX_CXX_COMPILE_STDCXX_11([], optional)

# Checks for library functions.
AC_CHECK_FUNCS([gettimeofday memset sqrt strstr])

# check for wget or curl
AC_CHECK_PROGS([WGET], [wget curl])

AC_MSG_CHECKING(if wget was wget)
if test "X$WGET" = "Xwget"; then
   AC_MSG_RESULT(yes)
   WGET="wget -q"
else
   AC_MSG_RESULT(no)
fi
AC_MSG_CHECKING(if wget was curl)
if test "X$WGET" = "Xcurl"; then
   AC_MSG_RESULT(yes)
   WGET="curl -O"
else
   AC_MSG_RESULT(no)
fi

AC_CHECK_PROGS([TAR], [tar])

###############################
# Check for Boost
AX_BOOST_BASE([1.54])

AX_BOOST_SYSTEM
AX_BOOST_ASIO

# this is because AX_BOOST_FILESYSTEM fails when cross-compiling
if test "$cross_compiling" != "yes"
then
    AX_BOOST_FILESYSTEM
fi

###############################
# Check for Protobuf library
AC_MSG_CHECKING([for Protobuf])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <google/protobuf/message.h>]]
    [[google::protobuf::Message *message;]])
], [
    AC_MSG_RESULT([yes])
    HAVE_PROTOBUF=1
], [
    AC_MSG_RESULT([no])
    HAVE_PROTOBUF=0
])
AC_DEFINE_UNQUOTED([HAVE_PROTOBUF], $HAVE_PROTOBUF, [1 if have Google Protobuf.])
if test "$HAVE_PROTOBUF" == "1" ; then
  AC_CHECK_LIB([protobuf], [main], [],
               [AC_MSG_FAILURE([have protobuf headers but no libprotobuf])])
  # libprotobuf needs libpthread.
  AC_CHECK_LIB([pthread], [main], [],
               [AC_MSG_FAILURE([have libprotobuf but no libpthread])])

  # See https://stackoverflow.com/questions/13939904/how-to-use-protocol-buffers-with-autoconf-automake
  AC_ARG_WITH([protoc],
      [AS_HELP_STRING([--with-protoc=/path/of/protoc],
          [Location of the protocol buffers compiler protoc. Defaults to looking on path.])],
      [PROTOC="$withval"],
      [ AS_IF([test "x${PROTOC}" == "x"],
          [AC_PATH_PROG([PROTOC], [protoc], [no])])
      ]
  )
  AS_IF([test "${PROTOC}" == "no"], [AC_MSG_ERROR([ProtoBuf compiler "protoc" not found.])])
fi

###############################
# Check for NDN-CPP
AC_CHECK_HEADERS([ndn-cpp/interest.hpp],,[AC_MSG_FAILURE([can't find interest.hpp])])
AC_MSG_CHECKING([for NDN-CPP library...])

AC_CHECK_LIB([ndn-cpp], [ndn_getNowMilliseconds])
AC_MSG_CHECKING([if NDN-CPP library was compiled with std::shared_ptr])
if test "$build_os" == "$host_os"; then
	AC_RUN_IFELSE(
		[AC_LANG_PROGRAM([
			#include <ndn-cpp/interest.hpp>
			#include <memory>
			],
			[
				ndn::ptr_lib::shared_ptr<int> ptrlibPtr(new int(5));
				std::shared_ptr<int> boostPtr(new int(6));
				ptrlibPtr = boostPtr;
			]
		)],
		[AC_MSG_RESULT([yes])],
		[AC_MSG_RESULT([no])]
		[AC_MSG_FAILURE([NDN-CPP installation at $NDNCPPBIN does not provide support for boost::shared_ptr])]
	)
fi

AC_CHECK_LIB([ndn-cpp-tools], [ndntools_getGeneralizedContentMetaComponent])

###############################
# Check for OpenFEC library
AC_CHECK_HEADERS([lib_common/of_openfec_api.h],,[AC_MSG_FAILURE([can't find OpenFEC headers])])
AC_CHECK_LIB([openfec],[of_create_codec_instance],,[AC_MSG_FAILURE([can't find OpenFEC library])])

###############################
# Check for nanomsg
AC_CHECK_LIB([nanomsg], [nn_socket])
AM_CONDITIONAL([HAVE_NANOMSG], [test "$ac_cv_lib_nanomsg_nn_socket" = yes])

###############################
# Check for persistent storage libs
SAVED_LIBS=$LIBS

if test "$os_module" != "android"
then
    LIBS="$LIBS -lrocksdb"
    AC_CHECK_LIB([rocksdb], [rocksdb_restore_options_create], [AC_DEFINE(HAVE_PERSISTENT_STORAGE)], [LIBS=$SAVED_LIBS])
    AM_CONDITIONAL([HAVE_PERSISTENT_STORAGE], [test "$ac_cv_lib_rocksdb_rocksdb_restore_options_create" = yes])
    AC_SUBST([PSTORAGE_LIB], ["-lrocksdb"])
else
    # check headers for leveldb
    AC_CHECK_HEADERS([leveldb/db.h],[AM_CONDITIONAL([HAVE_PERSISTENT_STORAGE], [test "true" = true])])

    AC_MSG_CHECKING([for leveldb library...])
    AC_LINK_IFELSE(
    [AC_LANG_PROGRAM([#include <leveldb/db.h>],
        [leveldb::DB* db])],
        [AC_MSG_RESULT([yes]) ; AM_CONDITIONAL([HAVE_PERSISTENT_STORAGE], [test "true" = true])],
        [AC_MSG_RESULT([can't link with leveldb lib. no persistent storage support]) ; AM_CONDITIONAL([HAVE_PERSISTENT_STORAGE], [test "false" = true])]
    )
    AC_SUBST([PSTORAGE_LIB], ["-lleveldb"])
fi

###############################
# Check for CNL
SAVED_LIBS=$LIBS
LIBS="$LIBS -lcnl-cpp"

AC_CHECK_HEADERS([cnl-cpp/namespace.hpp],,[AC_MSG_WARN([can't find namespace.hpp])])
AC_MSG_CHECKING([for CNL-CPP library...])

AC_LINK_IFELSE(
    [AC_LANG_PROGRAM([
					#include <cnl-cpp/namespace.hpp>
				],[
					cnl_cpp::Namespace n(ndn::Name("/test"))
		])],[
			AC_MSG_RESULT([yes])
			AC_DEFINE(HAVE_CNL_CPP)
			AM_CONDITIONAL([HAVE_CNL_CPP], [test "true" = true])
		],[
			AC_MSG_WARN([can't find CNL-CPP lib])
			LIBS=$SAVED_LIBS
		])

###############################
# Check for VPX
AC_CHECK_LIB([vpx], [vpx_codec_version])
AM_CONDITIONAL([HAVE_VPX], [test "$ac_cv_lib_vpx_vpx_codec_version" = yes])


AC_OUTPUT
