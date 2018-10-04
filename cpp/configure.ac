#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# 

AC_PREREQ([2.69])
AC_INIT([NDN-RTC],[v3.0.1-34-gee6be],[peter@remap.ucla.edu],[ndnrtc],[https://github.com/remap/ndnrtc])
AM_INIT_AUTOMAKE([1.10 -Wall no-define foreign subdir-objects])
AC_CONFIG_MACRO_DIR([m4])

AC_PREFIX_DEFAULT([/usr/local])
AC_CONFIG_SRCDIR([include/ndnrtc-common.hpp])
AC_CONFIG_FILES([Makefile])
AM_MAINTAINER_MODE
AM_SILENT_RULES([yes])

AC_ARG_VAR([BOOSTDIR], [Path to the directory which contains Boost library headers folder (default is /usr/local/include)])
AC_ARG_VAR([BOOSTLIB], [Path to the directory which contains Boost library binaries folder (default is /usr/local/lib)])
AC_ARG_VAR([NDNCPPDIR], [Path to the directory which contains NDN-CPP library headers folder (default is /usr/local/include)])
AC_ARG_VAR([NDNCPPLIB], [Path to the directory which contains NDN-CPP library binaries (default is /usr/local/lib)])
AC_ARG_VAR([OPENFECDIR], [Path to the directory which contains OpenFEC library])
AC_ARG_VAR([OPENFECSRC], [Path to the directory which contains OpenFEC library header files (default is $OPENFECDIR/src)])
AC_ARG_VAR([OPENFECLIB], [Path to the directory which contains OpenFEC library binaries (default is $OPENFECDIR/bin/Release)])
AC_ARG_VAR([WEBRTCDIR], [Path to the directory which contains WebRTC trunk])
AC_ARG_VAR([WEBRTCSRC], [Path to the directory which contains WebRTC header files (default is $WEBRTCDIR/webrtc)])
AC_ARG_VAR([WEBRTCLIB], [Path to the directory which contains WebRTC libraries (default is $WEBRTCDIR/out/Release)])
AC_ARG_VAR([LCONFIGDIR], [Optional (for client demo app): path to the directory which contains libconfig library headers (default is /usr/local/include)])
AC_ARG_VAR([LCONFIGLIB], [Optional (for client demo app): path to the directory which contains libconfig library binaries (default is /usr/local/lib)])
AC_ARG_VAR([NANOMSGGDIR], [Optional (for client demo app): path to the directory which contains nanomsg library headers (default is /usr/local/include)])
AC_ARG_VAR([NANOMSGLIB], [Optional (for client demo app): path to the directory which contains nanomsg library binaries (default is /usr/local/lib)])
AC_ARG_VAR([PSTORAGEDIR], [Optional: path to the directory which contains persistent storage library headers (LevelDB for Android or RocksDB otherwise, default is /usr/local/include)])
AC_ARG_VAR([PSTORAGELIB], [Optional: path to the directory which contains persistent storage library binaries (LevelDB for Android or RocksDB otherwise, default is /usr/local/lib)])

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

if test ${BOOSTDIR}; then
	DIR=$(cd $BOOSTDIR && pwd || echo "not found")
	if [[[ "$DIR" = "not found" ]]];
	then
		AC_MSG_ERROR([directory not found - $BOOSTDIR])
	fi
	BOOSTDIR=$DIR
else
	BOOSTDIR="/usr/local/include"
fi

if test ${BOOSTLIB}; then
	DIR=$(cd $BOOSTLIB && pwd || echo "not found")
	if [[[ "$DIR" = "not found" ]]];
	then
		AC_MSG_ERROR([directory not found - $BOOSTLIB])
	fi
	BOOSTLIB=$DIR
else
	BOOSTLIB="/usr/local/lib"
fi

if test ${NDNCPPDIR}; then
	DIR=$(cd $NDNCPPDIR && pwd || echo "not found")
	if [[[ "$DIR" = "not found" ]]];
	then
		AC_MSG_ERROR([directory not found - $NDNCPPDIR])
	fi
	NDNCPPDIR=$DIR
else
	NDNCPPDIR="/usr/local/include"
fi

if test ${NDNCPPLIB}; then
	DIR=$(cd $NDNCPPLIB && pwd || echo "not found")
	if [[[ "$DIR" = "not found" ]]];
	then
		AC_MSG_ERROR([directory not found - $NDNCPPLIB])
	fi
	NDNCPPLIB=$DIR
else
	NDNCPPLIB="/usr/local/lib"
fi

if test ${OPENFECDIR}; then
	DIR=$(cd $OPENFECDIR && pwd || echo "not found")
	if [[[ "$DIR" = "not found" ]]];
	then
		AC_MSG_ERROR([directory not found - $OPENFECDIR])
	fi
	OPENFECDIR=$DIR
fi

if test "x${OPENFECSRC}" = "x"; then
	OPENFECSRC="$OPENFECDIR/src"
fi

if test "x${OPENFECLIB}" = "x"; then
	OPENFECLIB="$OPENFECDIR/bin/Release"
fi

if test ${WEBRTCDIR}; then
	DIR=$(cd $WEBRTCDIR && pwd || echo "not found")
	if [[[ "$DIR" = "not found" ]]];
	then
		AC_MSG_ERROR([directory not found - $W])
	fi
	WEBRTCDIR=$DIR
fi

if test "x${WEBRTCSRC}" = "x"; then
	WEBRTCSRC="$WEBRTCDIR/webrtc"
fi

if test "x${WEBRTCLIB}" = "x"; then
	WEBRTCLIB="$WEBRTCDIR/out/Default/obj/webrtc"
fi

if test ${LCONFIGDIR}; then
	LCONFIGDIR=$(cd $LCONFIGDIR; pwd)
else
	LCONFIGDIR="/usr/local/include"
fi

if test ${LCONFIGLIB}; then
	LCONFIGLIB=$(cd $LCONFIGLIB; pwd)
else
	LCONFIGLIB="/usr/local/lib"
fi

if test ${NANOMSGDIR}; then
	NANOMSGDIR=$(cd $NANOMSGDIR; pwd)
else
	NANOMSGDIR="/usr/local/include"
fi

if test ${NANOMSGLIB}; then
	NANOMSGLIB=$(cd $NANOMSGLIB; pwd)
else
	NANOMSGLIB="/usr/local/lib"
fi

if test ${PSTORAGEDIR}; then
	PSTORAGEDIR=$(cd $PSTORAGEDIR; pwd)
else
	PSTORAGEDIR="/usr/local/include"
fi

if test ${PSTORAGELIB}; then
	PSTORAGELIB=$(cd $PSTORAGELIB; pwd)
else
	PSTORAGELIB="/usr/local/lib"
fi

# create links for client config file
AC_CONFIG_LINKS([client/params.cfg:tests/default.cfg])
AC_CONFIG_LINKS([client/sample-consumer.cfg:tests/sample-consumer.cfg])
AC_CONFIG_LINKS([client/sample-producer.cfg:tests/sample-producer.cfg])
AC_CONFIG_LINKS([client/test-freeculture.cfg:tests/test-freeculture.cfg])
AC_CONFIG_LINKS([tests/default.cfg:tests/default.cfg])
AC_CONFIG_LINKS([tests/sample-consumer.cfg:tests/sample-consumer.cfg])
AC_CONFIG_LINKS([tests/sample-producer.cfg:tests/sample-producer.cfg])

AC_CONFIG_LINKS([tests/sample-bad1.cfg:tests/sample-bad1.cfg])
AC_CONFIG_LINKS([tests/consumer-audio.cfg:tests/consumer-audio.cfg])
AC_CONFIG_LINKS([tests/consumer-video.cfg:tests/consumer-video.cfg])

# create links for resources folder
#AC_CONFIG_LINKS([resources/dummy:resources/dummy])

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
#AC_FUNC_MALLOC
#AC_FUNC_REALLOC
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
AX_BOOST_CHRONO
AX_BOOST_REGEX

# this is because AX_BOOST_TIMER and AX_BOOST_FILESYSTEM fail when cross-compiling
# I've added libboost_timer to ndnrtc_client_LDADD in Makefile.am manually (for android)
if test "$cross_compiling" != "yes"
then
    AX_BOOST_THREAD
    AX_BOOST_TIMER
    AX_BOOST_FILESYSTEM
fi

###############################
# Check for NDN-CPP
SAVED_CPPFLAGS=$CPPFLAGS
CPPFLAGS="$CPPFLAGS -I$NDNCPPDIR -I$BOOSTDIR"
SAVED_LDFLAGS=$LDFLAGS
LDFLAGS="$LDFLAGS -L$NDNCPPLIB"
SAVED_LIBS=$LIBS
LIBS="$LIBS -lndn-cpp"

AC_CHECK_HEADERS([ndn-cpp/interest.hpp],,[AC_MSG_FAILURE([can't find interest.hpp])])
AC_MSG_CHECKING([for NDN-CPP library...])

AC_LINK_IFELSE(
 	[AC_LANG_PROGRAM([#include <ndn-cpp/interest.hpp>],
 		[ndn::Interest interest])],
 		[AC_MSG_RESULT([yes])],
 		[AC_MSG_FAILURE([can't find NDN-CPP lib])]
)

AC_MSG_CHECKING([if NDN-CPP library was compiled for using boost::shared_ptr])

if test "$build_os" == "$host_os"; then
	AC_RUN_IFELSE(
		[AC_LANG_PROGRAM([
			#include <ndn-cpp/interest.hpp>
			#include <boost/shared_ptr.hpp>
			],
			[
				ndn::ptr_lib::shared_ptr<int> ptrlibPtr(new int(5));
				boost::shared_ptr<int> boostPtr(new int(6));
				ptrlibPtr = boostPtr;
			]
		)],
		[AC_MSG_RESULT([yes])],
		[AC_MSG_RESULT([no])]
		[AC_MSG_FAILURE([NDN-CPP installation at $NDNCPPBIN does not provide support for boost::shared_ptr])]	
	)
fi

LIBS=$SAVED_LIBS
LDFLAGS=$SAVED_LDFLAGS
CPPFLAGS=$SAVED_CPPFLAGS

###############################
# Check for OpenFEC library
SAVED_CPPFLAGS=$CPPFLAGS
CPPFLAGS="$CPPFLAGS -I${OPENFECSRC}"
SAVED_LDFLAGS=$LDFLAGS
LDFLAGS="$LDLFAGS -L$OPENFECLIB"
SAVED_LIBS="$LIBS"

AC_CHECK_HEADERS([lib_common/of_openfec_api.h],,[AC_MSG_FAILURE([can't find OpenFEC headers])])
AC_CHECK_LIB([openfec],[of_create_codec_instance],,[AC_MSG_FAILURE([can't find OpenFEC library])])

CPPFLAGS=$SAVED_CPPFLAGS
LDFLAGS=$SAVED_LDFLAGS
LIBS=$SAVED_LIBS

###############################
# Check for nanomsg
SAVED_CPPFLAGS=$CPPFLAGS
CPPFLAGS="$CPPFLAGS -I$NANOMSGDIR"
SAVED_LDFLAGS=$LDFLAGS
LDFLAGS="$LDFLAGS -L$NANOMSGLIB"
SAVED_LIBS=$LIBS
LIBS="$LIBS -lnanomsg"

AC_CHECK_LIB([nanomsg], [nn_socket])
AM_CONDITIONAL([HAVE_NANOMSG], [test "$ac_cv_lib_nanomsg_nn_socket" = yes])

CPPFLAGS=$SAVED_CPPFLAGS
LDFLAGS=$SAVED_LDFLAGS
LIBS=$SAVED_LIBS

###############################
# Check for persistent storage libs
SAVED_CPPFLAGS=$CPPFLAGS
CPPFLAGS="$CPPFLAGS -I$PSTORAGEDIR"
SAVED_LDFLAGS=$LDFLAGS
LDFLAGS="$LDFLAGS -L$PSTORAGELIB"
SAVED_LIBS=$LIBS

if test "$os_module" != "android"
then
    LIBS="$LIBS -lrocksdb"
    AC_CHECK_LIB([rocksdb], [rocksdb_restore_options_create], [AC_DEFINE(HAVE_PERSISTENT_STORAGE)])
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

CPPFLAGS=$SAVED_CPPFLAGS
LDFLAGS=$SAVED_LDFLAGS
LIBS=$SAVED_LIBS

###############################
AC_SUBST([WEBRTCLIBS], ["-lwebrtc"])

AC_OUTPUT
