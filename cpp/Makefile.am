ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

pkginclude_HEADERS = include/ndnrtc-library.h include/params.h include/statistics.h include/interfaces.h include/name-components.h include/error-codes.h include/ndnrtc-defines.h

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
  src/ndnrtc-endian.h \
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
  src/renderer.h \
  src/rtt-estimation.cpp src/rtt-estimation.h \
  src/segmentizer.cpp src/segmentizer.h \
  src/session.cpp src/session.h \
  src/simple-log.cpp src/simple-log.h \
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
libndnrtc_la_LDFLAGS = -L@NDNCPPLIB@ -L@OPENFECLIB@ -L@WEBRTCLIB@ -L@BOOSTLIB@ -flat_namespace -undefined error

if OS_LINUX
libwebrtc-all.a:
	$(shell mkdir -p tmp && cd tmp && \
	for lib in $$(ls @WEBRTCLIB@/*.a) ; do \
		ar x $$lib; \
	done && \
	ar r @WEBRTCLIB@/$@ *.o && cd .. && rm -Rf tmp)	

libndnrtc_la_DEPENDENCIES = libwebrtc-all.a
libndnrtc_la_LIBADD = -lndn-cpp -lopenfec -lboost_system -lboost_timer -lboost_chrono -lboost_thread -lwebrtc-all
endif

if OS_DARWIN
libndnrtc_la_LIBADD = -lndn-cpp -lopenfec -lboost_system -lboost_timer -lboost_chrono -lboost_thread @WEBRTCLIBS@
endif


