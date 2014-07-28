ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

SUBDIRS = src/objc .

pkginclude_HEADERS = include/external-renderer.h include/ndnrtc-library.h include/params.h include/simple-log.h include/ndnrtc-defines.h include/statistics.h

lib_LTLIBRARIES = libndnrtc.la

libndnrtc_la_SOURCES = src/audio-capturer.cpp src/audio-capturer.h \
  src/audio-consumer.cpp src/audio-consumer.h \
  src/audio-playout.cpp src/audio-playout.h \
  src/audio-renderer.cpp src/audio-renderer.h \
  src/audio-sender.cpp src/audio-sender.h \
  src/av-sync.cpp src/av-sync.h \
  src/buffer-estimator.cpp src/buffer-estimator.h \
  src/camera-capturer.cpp src/camera-capturer.h \
  src/chase-estimation.cpp src/chase-estimation.h \
  src/consumer-channel.cpp src/consumer-channel.h \
  src/consumer.cpp src/consumer.h \
  src/endian.h \
  src/face-wrapper.cpp src/face-wrapper.h \
  src/fec.cpp src/fec.h \
  src/frame-buffer.cpp src/frame-buffer.h \
  src/frame-data.cpp src/frame-data.h \
  src/interest-queue.cpp src/interest-queue.h \
  src/jitter-timing.cpp src/jitter-timing.h \
  src/media-sender.cpp src/media-sender.h \
  src/ndn-assembler.cpp src/ndn-assembler.h \
  src/ndnlib.h \
  src/ndnrtc-common.h \
  src/ndnrtc-debug.h \
  include/ndnrtc-defines.h \
  src/ndnrtc-library.cpp include/ndnrtc-library.h \
  include/external-renderer.h \
  src/ndnrtc-namespace.cpp src/ndnrtc-namespace.h \
  src/ndnrtc-object.cpp src/ndnrtc-object.h \
  src/ndnrtc-testing.h \
  src/ndnrtc-utils.cpp src/ndnrtc-utils.h \
  src/params.cpp include/params.h \
  src/pipeliner.cpp src/pipeliner.h \
  src/playout.cpp src/playout.h \
  src/rate-adaptation-module.h \
  src/rate-control.cpp src/rate-control.h \
  src/realtime-arc.cpp src/realtime-arc.h \
  src/renderer.h \
  src/rtt-estimation.cpp src/rtt-estimation.h \
  src/segmentizer.cpp src/segmentizer.h \
  src/sender-channel.cpp src/sender-channel.h \
  src/service-channel.cpp src/service-channel.h \
  src/simple-log.cpp include/simple-log.h \
  include/statistics.h \
  src/video-coder.cpp src/video-coder.h \
  src/video-consumer.cpp src/video-consumer.h \
  src/video-decoder.cpp src/video-decoder.h \
  src/video-playout.cpp src/video-playout.h \
  src/video-renderer.cpp src/video-renderer.h \
  src/video-sender.cpp src/video-sender.h \
  src/webrtc-audio-channel.cpp src/webrtc-audio-channel.h \
  src/webrtc.h

if OS_DARWIN
  libndnrtc_la_OBJCFLAGS = -framework Cocoa -framework OpenGL -framework CoreVideo -framework QTKit -framework CoreAudio -framework AudioToolbox
endif

libndnrtc_la_CPPFLAGS = -fPIC -I$(top_srcdir)/include -I$(top_srcdir)/src -I@WEBRTCDIR@ -I@WEBRTCSRC@ -I@BOOSTDIR@ -I@NDNCPPDIR@ -I@OPENFECSRC@ -D BASE_FILE_NAME=\"$*\" -stdlib=libc++
libndnrtc_la_LDFLAGS = -stdlib=libc++ -L@NDNCPPLIB@ -L@OPENFECLIB@ -L@WEBRTCLIB@ -flat_namespace -undefined error $(libndnrtc_la_OBJCFLAGS) -lndn-cpp -lopenfec @WEBRTCLIBS@
libndnrtc_la_LIBADD = src/objc/libcocoa-renderer.a

.mm.o :
	$(OBJCXX) -x objective-c++ -c @DEFS@ $(ndnrtc_demo_CPPFLAGS) $< -o $@

if OS_DARWIN
  EXTRA_PROGRAMS = ndnrtc-demo
  ndnrtc_demo_SOURCES = demoapp/config.cc demoapp/config.h \
    demoapp/main.mm demoapp/AppDelegate.m \
    demoapp/renderer-stub.cc demoapp/renderer-stub.h \
    demoapp/view.cc demoapp/view.h \
    src/simple-log.cpp include/simple-log.h
  ndnrtc_demo_CPPFLAGS = -I$(top_srcdir)/include -I@LCONFIGDIR@ -I@PTHREADDIR@ -I@NCURSESDIR@ -stdlib=libstdc++
  ndnrtc_demo_LDFLAGS = -L@LCONFIGLIB@ -L@NCURSESLIB@ -L@PTHREADLIB@ -lconfig++ -lpanel -lmenu -lncurses -lpthread -framework Foundation -framework Cocoa -stdlib=libstdc++
  noinst_SCRIPTS = demoapp/rundemo.sh resources/ndnrtc.cfg
endif