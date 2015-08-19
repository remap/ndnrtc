#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([NDN-RTC],[1.0.1],[peter@remap.ucla.edu],[ndnrtc],[https://github.com/remap/ndnrtc])
AM_INIT_AUTOMAKE([1.10 -Wall no-define foreign subdir-objects])
AC_CONFIG_MACRO_DIR([m4])

AC_PREFIX_DEFAULT([/usr/local])
AC_CONFIG_SRCDIR([src/ndnrtc-common.h])
AC_CONFIG_FILES([Makefile])
AM_MAINTAINER_MODE

AC_ARG_VAR([BOOSTDIR], [Path to the directory which contains Boost library headers folder (default is /usr/local/include)])
AC_ARG_VAR([BOOSTLIB], [Path to the directory which contains Boost library binaries folder (default is /usr/local/lib)])
AC_ARG_VAR([NDNCPPDIR], [Path to the directory which contains NDN-CPP library headers folder (default is /usr/local/include)])
AC_ARG_VAR([NDNCPPLIB], [Path to the directory which contains NDN-CPP library binaries (default is /usr/local/lib)])
AC_ARG_VAR([OPENFECDIR], [Path to the directory which contains OpenFEC library])
AC_ARG_VAR([OPENFECSRC], [Path to the directory which contains OpenFEC library header files (default is $OPENFECDIR/src)])
AC_ARG_VAR([OPENFECLIB], [Path to the directory which contains OpenFEC library binaries (default is $OPENFECDIR/bin/Release)])
AC_ARG_VAR([PTHREADDIR], [Path to the directory which contains pthread library headers (default is /usr/local/include)])
AC_ARG_VAR([PTHREADLIB], [Path to the directory which contains pthread library binaries (default is /usr/local/lib)])
AC_ARG_VAR([WEBRTCDIR], [Path to the directory which contains WebRTC trunk])
AC_ARG_VAR([WEBRTCSRC], [Path to the directory which contains WebRTC header files (default is $WEBRTCDIR/wbertc)])
AC_ARG_VAR([WEBRTCLIB], [Path to the directory which contains WebRTC libraries (default is $WEBRTCDIR/out/Release)])

AC_ARG_VAR([LCONFIGDIR], [Path to the directory which contains libconfig library headers (default is /usr/local/include)])
AC_ARG_VAR([LCONFIGLIB], [Path to the directory which contains libconfig library binaries (default is /usr/local/lib)])
AC_ARG_VAR([NCURSESDIR], [Path to the directory which contains ncurses library headers (default is /usr/local/include)])
AC_ARG_VAR([NCURSESLIB], [Path to the directory which contains ncurses library binaries (default is /usr/local/lib)])

if test ${BOOSTDIR}; then
	BOOSTDIR=$(cd $BOOSTDIR; pwd)
else
	BOOSTDIR="/usr/local/include"
fi

if test ${BOOSTLIB}; then
	BOOSTLIB=$(cd $BOOSTLIB; pwd)
else
	BOOSTLIB="/usr/local/lib"
fi

if test ${NDNCPPDIR}; then
	NDNCPPDIR=$(cd $NDNCPPDIR; pwd)
else
	NDNCPPDIR="/usr/local/include"
fi

if test ${NDNCPPLIB}; then
	NDNCPPLIB=$(cd $NDNCPPLIB; pwd)
else
	NDNCPPLIB="/usr/local/lib"
fi

if test ${OPENFECDIR}; then
	OPENFECDIR=$(cd $OPENFECDIR; pwd)
fi

if test "x${OPENFECSRC}" = "x"; then
	OPENFECSRC="$OPENFECDIR/src"
fi

if test "x${OPENFECLIB}" = "x"; then
	OPENFECLIB="$OPENFECDIR/bin/Release"
fi

if test ${PTHREADDIR}; then
	PTHREADDIR=$(cd $PTHREADDIR; pwd)
else
	PTHREADDIR="/usr/local/include"
fi

if test ${PTHREADLIB}; then
	PTHREADLIB=$(cd $PTHREADLIB; pwd)
else
	PTHREADLIB="/usr/local/lib"
fi

if test ${WEBRTCDIR}; then
	WEBRTCDIR=$(cd $WEBRTCDIR; pwd)
fi

if test "x${WEBRTCSRC}" = "x"; then
	WEBRTCSRC="$WEBRTCDIR/webrtc"
fi

if test "x${WEBRTCLIB}" = "x"; then
	WEBRTCLIB="$WEBRTCDIR/out/Release"
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

if test ${NCURSESDIR}; then
	NCURSESDIR=$(cd $NCURSESDIR; pwd)
else
	NCURSESDIR="/usr/local/include"
fi

if test ${NCURSESLIB}; then
	NCURSESLIB=$(cd $NCURSESLIB; pwd)
else
	NCURSESLIB="/usr/local/lib"
fi

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

AC_ARG_WITH([libc++], [AS_HELP_STRING([--with-libc++],[use libc++ if available [default=use libstdc++]])], 
	[
	STDLIBFLAGS_CPP="-stdlib=libc++" 
	STDLIBFLAGS_LD="-stdlib=libc++"
	],
	[
	STDLIBFLAGS_CPP="-stdlib=libstdc++" 
	STDLIBFLAGS_LD="-stdlib=libstdc++"	
	])

# Checks for programs.
AC_CANONICAL_HOST
AC_PROG_CC
AC_PROG_CXX
AC_PROG_OBJC
AC_PROG_OBJCXX
AM_PROG_CC_C_O
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

# check for compiler flags
if [[[ $CPPFLAGS =~ .*stdlib.* ]]]; then
	AC_MSG_NOTICE([stdlib compiler flag provided. adding $STDLIBFLAGS_CPP skipped])	
else
	if [[[ $STDLIBFLAGS_CPP =~ .*libc\+\+.* ]]]; then
		AX_CHECK_COMPILE_FLAG([-stdlib=libc++], [CPPFLAGS="$CPPFLAGS $STDLIBFLAGS_CPP"])
	else
		AX_CHECK_COMPILE_FLAG([-stdlib=libstdc++], [CPPFLAGS="$CPPFLAGS $STDLIBFLAGS_CPP"])
	fi
fi

# check for linker flags
if [[[ $LDFLAGS =~ .*stdlib.* ]]]; then
	AC_MSG_NOTICE([stdlib linker flag provided. adding $STDLIBFLAGS_LD skipped])
else
	if [[[ $STDLIBFLAGS_LD =~ .*libc\+\+.* ]]]; then
		AX_CHECK_LINK_FLAG([-stdlib=libc++], [LDFLAGS="$LDFLAGS $STDLIBFLAGS_LD"])
	else
		AX_CHECK_LINK_FLAG([-stdlib=libstdc++], [LDFLAGS="$LDFLAGS $STDLIBFLAGS_LD"])
	fi
fi

# create links for demo script
AC_CONFIG_LINKS([demoapp/rundemo.sh:demoapp/rundemo.sh])

# create links for resources folder
#AC_CONFIG_LINKS([resources/dummy:resources/dummy])

# setting conditionals for automake
AM_CONDITIONAL([OS_DARWIN],  [test $os_module = darwin])
AM_CONDITIONAL([OS_LINUX],  [test $os_module = linux])
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

if [[[ $OBJCXXFLAGS =~ .*gnu++11.* ]]]; then
	AC_MSG_NOTICE([gnu++11 compiler flag provided])	
else
	OBJCXXFLAGS="$OBJCXXFLAGS -std=gnu++11"
fi

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([gettimeofday memset sqrt strstr])

# Check for Boost
SAVED_CPPFLAGS=$CPPFLAGS
CPPFLAGS="$CPPFLAGS -I$BOOSTDIR"
SAVED_LDFLAGS=$LDFLAGS
LDFLAGS="$LDFLAGS -L$BOOSTLIB"
SAVED_LIBS=$LIBS
LIBS="$LIBS -lboost_system -lboost_timer -lboost_chrono"

AC_MSG_CHECKING([for boost::shared_ptr...])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <boost/shared_ptr.hpp>]]
    [[boost::shared_ptr<int> have_shared_ptr;]])
], [
    AC_MSG_RESULT([yes])
], [
    AC_MSG_FAILURE([not found])
])

AC_MSG_CHECKING([for boost::enabled_shared_from_this...])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <boost/enable_shared_from_this.hpp>]]
    [[class A:public boost::enable_shared_from_this<A>{};]])
], [
    AC_MSG_RESULT([yes])
], [
    AC_MSG_FAILURE([not found])
])

AC_MSG_CHECKING([for boost::chrono...])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <boost/chrono.hpp>]]
    [[auto now = boost::chrono::system_clock::now().time_since_epoch();
  	boost::chrono::nanoseconds nsec = boost::chrono::steady_clock::now().time_since_epoch();
  	boost::chrono::microseconds usec = boost::chrono::duration_cast<boost::chrono::microseconds>(boost::chrono::steady_clock::now().time_since_epoch());
    boost::chrono::milliseconds msec = boost::chrono::duration_cast<boost::chrono::milliseconds>(boost::chrono::steady_clock::now().time_since_epoch());
    boost::chrono::seconds sec = boost::chrono::duration_cast<boost::chrono::seconds>(boost::chrono::steady_clock::now().time_since_epoch());
    boost::chrono::duration<double> secd = now;]])
], [
    AC_MSG_RESULT([yes])
], [
    AC_MSG_FAILURE([not found])
])

AC_MSG_CHECKING([for boost::asio::steady_timer...])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <boost/asio.hpp>
	  #include <boost/asio/steady_timer.hpp>]]
    [[
    boost::asio::io_service io_service_;
    boost::asio::steady_timer timer_(io_service_);]])
], [
    AC_MSG_RESULT([yes])
], [
    AC_MSG_FAILURE([not found])
])

AC_MSG_CHECKING([for boost::crc...])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <boost/crc.hpp>]]
    [[
    boost::crc_16_type crc_computer_;]])
], [
    AC_MSG_RESULT([yes])
], [
    AC_MSG_FAILURE([not found])
])

AC_MSG_CHECKING([for boost::thread...])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[#include <boost/thread.hpp>]]
    [[boost::thread thread;]])
], [
    AC_MSG_RESULT([yes])
], [
    AC_MSG_FAILURE([not found])
])

AC_MSG_CHECKING([check linking with libs $LIBS...])
AC_LINK_IFELSE(
 	[AC_LANG_PROGRAM([#include <boost/asio.hpp>
	  #include <boost/asio/steady_timer.hpp>],
 		[boost::asio::io_service io_service_;
    boost::asio::steady_timer timer_(io_service_);])],
 		[AC_MSG_RESULT([yes])],
 		[AC_MSG_FAILURE([can't find required boost libraries (boost_system-mt, boost_timer-mt, boost_chrono-mt)])]
)

LIBS=$SAVED_LIBS
LDFLAGS=$SAVED_LDFLAGS
CPPFLAGS=$SAVED_CPPFLAGS

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

LIBS=$SAVED_LIBS
LDFLAGS=$SAVED_LDFLAGS
CPPFLAGS=$SAVED_CPPFLAGS

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

# Check for pthread library
SAVED_CPPFLAGS=$CPPFLAGS
CPPFLAGS="$CPPFLAGS -I${PTHREADDIR}"
SAVED_LDFLAGS=$LDFLAGS
LDFLAGS="$LDLFAGS -L$PTHREADLIB"
SAVED_LIBS="$LIBS"

AC_CHECK_HEADERS([pthread.h],,[AC_MSG_FAILURE([can't find pthread headers])])
AC_CHECK_LIB([pthread], [pthread_atfork],,[AC_MSG_FAILURE([can't find pthread library])])

CPPFLAGS=$SAVED_CPPFLAGS
LDFLAGS=$SAVED_LDFLAGS
LIBS=$SAVED_LIBS

# Check for WebRTC headers
SAVED_CPPFLAGS=$CPPFLAGS
CPPFLAGS="$CPPFLAGS -I${WEBRTCDIR} -I${WEBRTCSRC}"

AC_MSG_NOTICE([checking for WebRTC header files:])
AC_CHECK_HEADERS([common_types.h system_wrappers/interface/event_wrapper.h system_wrappers/interface/thread_wrapper.h system_wrappers/interface/critical_section_wrapper.h system_wrappers/interface/tick_util.h system_wrappers/interface/rw_lock_wrapper.h  modules/video_capture/include/video_capture_defines.h modules/video_capture/include/video_capture.h modules/video_capture/include/video_capture_factory.h common_video/libyuv/include/webrtc_libyuv.h modules/video_coding/codecs/vp8/include/vp8.h modules/video_coding/main/interface/video_coding_defines.h modules/video_coding/main/source/codec_database.h modules/video_coding/main/source/internal_defines.h voice_engine/include/voe_base.h voice_engine/include/voe_network.h voice_engine/include/voe_audio_processing.h modules/audio_coding/main/interface/audio_coding_module.h], 
[],
[AC_MSG_FAILURE([
	   can't find WebRTC header files
	   ])])

AC_SUBST([WEBRTCLIBS], ["-laudio_coding_module \
   -laudio_decoder_interface \
   -lCNG \
   -lG711 \
   -lG722 \
   -liLBC \
   -lPCM16B \
   -laudio_encoder_interface \
   -laudio_conference_mixer \
   -laudioproc_debug_proto \
   -laudio_device \
   -laudio_processing \
   -laudio_processing_sse2 \
   -lbitrate_controller \
   -lchannel_transport \
   -lcommon_audio \
   -lcommon_audio_sse2 \
   -lcommon_video \
   -ldesktop_capture \
   -ldesktop_capture_differ_sse2 \
   -lframe_editing_lib \
   -lframe_generator \
   -lhistogram \
   -ljpeg_turbo \
   -liSAC \
   -lmedia_file \
   -lmetrics \
   -lneteq \
   -lopus \
   -lpaced_sender \
   -lprotobuf_lite \
   -lrtp_rtcp \
   -lrtc_base_approved \
   -lsystem_wrappers \
   -lvideo_capture_module \
   -lvideo_coding_utility \
   -lvideo_engine_core \
   -lvideo_processing \
   -lvideo_processing_sse2 \
   -lvideo_quality_analysis \
   -lvideo_render_module \
   -lvideo_render_module_internal_impl \
   -lvoice_engine \
   -lvpx \
   -lvpx_intrinsics_avx2 \
   -lvpx_intrinsics_mmx \
   -lvpx_intrinsics_sse2 \
   -lvpx_intrinsics_ssse3 \
   -lvpx_intrinsics_sse4_1 \
   -lwebrtc_i420 \
   -lwebrtc_opus \
   -lwebrtc_utility \
   -lwebrtc_video_coding \
   -lwebrtc_vp8 \
   -lwebrtc_vp9 \
   -lwebrtc_common \
   -lwebrtc \   
   -lyuv"])

CPPFLAGS=$SAVED_CPPFLAGS

# Check for libconfig library
SAVED_CPPFLAGS=$CPPFLAGS
CPPFLAGS="$CPPFLAGS -I${LCONFIGDIR} -std=c++0x -stdlib=libstdc++"
SAVED_LDFLAGS=$LDFLAGS
LDFLAGS="$LDLFAGS -L$LCONFIGLIB -L$OPENFECLIB"
SAVED_LIBS=$LIBS
LIBS="$LIBS -lconfig++"

AC_CHECK_HEADERS([libconfig.h++],,[AC_MSG_WARN([can't find libconfig++ headers. demo app requires libconfig++])])
AC_MSG_CHECKING([for libconfig++...])
AC_LINK_IFELSE(
 	[AC_LANG_PROGRAM([#include <libconfig.h++>],
 		[libconfig::Config cfg;])],
 		[AC_MSG_RESULT([yes])],
 		[AC_MSG_WARN([can't find libconfig. demo app requires libconfig++])]
)

LIBS=$SAVED_LIBS
CPPFLAGS=$SAVED_CPPFLAGS
LDFLAGS=$SAVED_LDFLAGS

# Check for ncurses library
SAVED_CPPFLAGS=$CPPFLAGS
CPPFLAGS="$CPPFLAGS -I${NCURSESDIR}"
SAVED_LDFLAGS=$LDFLAGS
LDFLAGS="$LDLFAGS -L$NCURSESLIB"
SAVED_LIBS=$LIBS
LIBS="$LIBS -lncurses -lpanel -lmenu"

AC_CHECK_HEADERS([menu.h panel.h curses.h],,[AC_MSG_WARN([can't find ncurses headers. demo app requires ncurses])])
AC_CHECK_LIB([ncurses],[initscr], ,[AC_MSG_WARN([can't find ncurses lib. demo app requires ncurses])])
AC_CHECK_LIB([panel],[panel_window], ,[AC_MSG_WARN([can't find ncurses lib. demo app requires ncurses])])
AC_CHECK_LIB([menu],[menu_items], ,[AC_MSG_WARN([can't find ncurses lib. demo app requires ncurses])])

LIBS=$SAVED_LIBS
CPPFLAGS=$SAVED_CPPFLAGS
LDFLAGS=$SAVED_LDFLAGS

AC_OUTPUT
