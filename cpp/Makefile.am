ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

pkginclude_HEADERS = include/ndnrtc-library.h include/params.h include/simple-log.h include/ndnrtc-defines.h include/statistics.h include/interfaces.h include/name-components.h include/error-codes.h

lib_LTLIBRARIES = libndnrtc.la

libndnrtc_la_SOURCES = src/audio-capturer.cpp src/audio-capturer.h \
  src/audio-consumer.cpp src/audio-consumer.h \
  src/audio-playout.cpp src/audio-playout.h \
  src/audio-renderer.cpp src/audio-renderer.h \
  src/audio-thread.cpp src/audio-thread.h \
  src/av-sync.cpp src/av-sync.h \
  src/buffer-estimator.cpp src/buffer-estimator.h \
  src/base-capturer.cpp src/base-capturer.h \
  src/chase-estimation.cpp src/chase-estimation.h \
  src/consumer.cpp src/consumer.h \
  src/endian.h \
  src/external-capturer.cpp src/external-capturer.hpp \
  src/face-wrapper.cpp src/face-wrapper.h \
  src/fec.cpp src/fec.h \
  src/frame-buffer.cpp src/frame-buffer.h \
  src/frame-data.cpp src/frame-data.h \
  src/interest-queue.cpp src/interest-queue.h \
  src/jitter-timing.cpp src/jitter-timing.h \
  src/media-thread.cpp src/media-thread.h \
  src/media-stream.cpp src/media-stream.h \
  src/name-components.cpp include/name-components.h \
  src/ndn-assembler.cpp src/ndn-assembler.h \
  src/ndnlib.h \
  src/ndnrtc-common.h \
  src/ndnrtc-debug.h \
  include/ndnrtc-defines.h \
  src/ndnrtc-library.cpp include/ndnrtc-library.h \
  include/interfaces.h \
  src/ndnrtc-namespace.cpp src/ndnrtc-namespace.h \
  src/ndnrtc-object.cpp src/ndnrtc-object.h \
  src/ndnrtc-testing.h \
  src/ndnrtc-utils.cpp src/ndnrtc-utils.h \
  include/params.h \
  src/pipeliner.cpp src/pipeliner.h \
  src/playout.cpp src/playout.h \
  src/rate-adaptation-module.h \
  src/rate-control.cpp src/rate-control.h \
  src/realtime-arc.cpp src/realtime-arc.h \
  src/renderer.h \
  src/rtt-estimation.cpp src/rtt-estimation.h \
  src/segmentizer.cpp src/segmentizer.h \
  src/service-channel.cpp src/service-channel.h \
  src/session.cpp src/session.h \
  src/simple-log.cpp include/simple-log.h \
  include/statistics.h src/statistics.cpp\
  src/video-coder.cpp src/video-coder.h \
  src/video-consumer.cpp src/video-consumer.h \
  src/video-decoder.cpp src/video-decoder.h \
  src/video-playout.cpp src/video-playout.h \
  src/video-renderer.cpp src/video-renderer.h \
  src/video-thread.cpp src/video-thread.h \
  src/webrtc-audio-channel.cpp src/webrtc-audio-channel.h \
  src/webrtc.h

libndnrtc_la_CPPFLAGS = -fPIC -I$(top_srcdir)/include -I$(top_srcdir)/src -I@WEBRTCDIR@ -I@WEBRTCSRC@ -I@BOOSTDIR@ -I@NDNCPPDIR@ -I@OPENFECSRC@ -D BASE_FILE_NAME=\"$*\"
libndnrtc_la_LDFLAGS = -L@NDNCPPLIB@ -L@OPENFECLIB@ -L@WEBRTCLIB@ -L@BOOSTLIB@ -flat_namespace -undefined error $(libndnrtc_la_OBJCFLAGS) -lndn-cpp -lopenfec @WEBRTCLIBS@ -lboost_system -lboost_timer -lboost_chrono -lboost_thread

.mm.o :
	$(OBJCXX) -x objective-c++ -c @DEFS@ $(CPPFLAGS) $(ndnrtc_demo_CPPFLAGS) $< -o $@

if OS_DARWIN
  EXTRA_PROGRAMS = ndnrtc-demo
  ndnrtc_demo_SOURCES = demoapp/config.cc demoapp/config.h \
    demoapp/main.mm demoapp/AppDelegate.h demoapp/AppDelegate.m \
    demoapp/renderer-stub.cc demoapp/renderer-stub.h \
    demoapp/view.cc demoapp/view.h \
    demoapp/CameraCapturer.h demoapp/CameraCapturer.m \
    src/simple-log.cpp include/simple-log.h \
    include/interfaces.h
    
  ndnrtc_demo_CPPFLAGS = -I$(top_srcdir)/include -I@LCONFIGDIR@ -I@PTHREADDIR@ -I@NCURSESDIR@
  
  ndnrtc_demo_LDFLAGS = -all-static -L@LCONFIGLIB@ -L@NCURSESLIB@ -L@PTHREADLIB@ -lconfig++ -lpanel -lmenu -lncurses -lpthread -framework Foundation -framework Cocoa -framework AVFoundation -framework CoreVideo -framework CoreMedia
  
  ndnrtc_demo_OBJCFLAGS = -fobjc-arc
  
  noinst_SCRIPTS = demoapp/rundemo.sh resources/ndnrtc.cfg
endif

check_PROGRAMS = bin/unit-tests/test-encoder

bin_unit_tests_test_encoder_SOURCES = test/new/test-encoder.cpp contrib/gtest-1.7.0/fused-src/gtest/gtest-all.cc
bin_unit_tests_test_encoder_CPPFLAGS = -I$(top_srcdir)/contrib/gtest-1.7.0/fused-src -I$(top_srcdir)/src -I$(top_srcdir)/include -I@WEBRTCDIR@ -I@WEBRTCSRC@ -I@BOOSTDIR@ -I@NDNCPPDIR@ -I@OPENFECSRC@
bin_unit_tests_test_encoder_LDADD = libndnrtc.la

TESTS = $(check_PROGRAMS)



