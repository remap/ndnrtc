ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

pkginclude_HEADERS = include/ndnrtc-library.h include/params.h include/statistics.h include/interfaces.h include/name-components.h include/error-codes.h include/ndnrtc-defines.h include/simple-log.h

lib_LTLIBRARIES = libndnrtc.la
libndnrtc_la_SOURCES = src/audio-capturer.cpp src/audio-capturer.h \
  src/audio-controller.cpp src/audio-controller.h \
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
  src/ndnrtc-common.h \
  src/ndnrtc-debug.h \
  include/ndnrtc-defines.h \
  src/ndnrtc-exception.h src/ndnrtc-exception.cpp \
  src/ndnrtc-library.cpp include/ndnrtc-library.h \
  src/ndnrtc-manager.cpp src/ndnrtc-manager.h \
  include/interfaces.h \
  src/ndnrtc-namespace.cpp src/ndnrtc-namespace.h \
  src/ndnrtc-object.cpp src/ndnrtc-object.h \
  src/ndnrtc-testing.h \
  src/ndnrtc-utils.cpp src/ndnrtc-utils.h \
  include/params.h \
  src/params-validator.h src/params-validator.cpp \
  src/pipeliner.cpp src/pipeliner.h \
  src/playout.cpp src/playout.h \
  src/renderer.h \
  src/rtt-estimation.cpp src/rtt-estimation.h \
  src/segmentizer.cpp src/segmentizer.h \
  src/session.cpp src/session.h \
  src/simple-log.cpp \
  include/statistics.h src/statistics.cpp \
  src/threading-capability.h src/threading-capability.cpp \
  src/video-coder.cpp src/video-coder.h \
  src/video-consumer.cpp src/video-consumer.h \
  src/video-decoder.cpp src/video-decoder.h \
  src/video-playout.cpp src/video-playout.h \
  src/video-renderer.cpp src/video-renderer.h \
  src/video-thread.cpp src/video-thread.h \
  src/webrtc-audio-channel.cpp src/webrtc-audio-channel.h \
  src/webrtc.h


libndnrtc_la_CPPFLAGS = -fPIC -I$(top_srcdir)/include -I$(top_srcdir)/src -I@WEBRTCDIR@ -I@WEBRTCSRC@ -I@BOOSTDIR@ -I@NDNCPPDIR@ -I@OPENFECSRC@ -D BASE_FILE_NAME=\"$*\"
libndnrtc_la_LDFLAGS = -L@NDNCPPLIB@ -L@OPENFECLIB@ -L@WEBRTCLIB@ -L@BOOSTLIB@ -flat_namespace 
libndnrtc_la_LIBADD = -lndn-cpp -lopenfec -lboost_system -lboost_timer -lboost_chrono -lboost_thread


if OS_LINUX

make-fat-libs:
	$(shell for lib in $$(find @WEBRTCLIB@ -name '*.a'); do \
		ar -t $$lib | xargs ar rvs $$lib.new 1> /dev/null 2>&1 && mv -v $$lib.new $$lib > /dev/null; \
	done && \
	for lib in $$(find @WEBRTCLIB@ -name '*.a'); do cp -f $$lib @WEBRTCLIB@ 2> /dev/null; done;)

libwebrtc-all.a: make-fat-libs
	$(shell mkdir -p tmp && cd tmp && \
	for lib in $$(ls @WEBRTCLIB@/*.a) ; do \
		ar x $$lib; \
	done && \
	ar r @WEBRTCLIB@/$@ *.o && cd .. && rm -Rf tmp)	

libndnrtc_la_DEPENDENCIES = libwebrtc-all.a
libndnrtc_la_LIBADD += -lwebrtc-all

endif

if OS_DARWIN

  libndnrtc_la_LIBADD += @WEBRTCLIBS@

endif

#################
# client app	#
#################
#bin_PROGRAMS = ndnrtc-client
EXTRA_PROGRAMS = ndnrtc-client
ndnrtc_client_SOURCES = client/src/main.cpp client/src/renderer.h client/src/renderer.cpp client/src/config.cpp client/src/config.h client/src/stat-collector.cpp client/src/stat-collector.h client/src/capturer.h client/src/capturer.cpp client/src/client.cpp client/src/client.h client/src/frame-io.h client/src/frame-io.cpp client/src/video-source.cpp client/src/video-source.h client/src/precise-generator.h client/src/precise-generator.cpp
ndnrtc_client_CPPFLAGS = -I$(top_srcdir)/client/src -I@LCONFIGDIR@ -I@BOOSTDIR@ -I$(includedir)
ndnrtc_client_LDFLAGS = -L@LCONFIGLIB@ -L$(libdir)
ndnrtc_client_LDADD = -lconfig++ $(top_builddir)/libndnrtc.la 
noinst_SCRIPTS = client/params.cfg

if OS_LINUX

#ndnrtc_client_LDFLAGS += -Wl,-rpath -Wl,/usr/lib/x86_64-linux-gnu
ndnrtc_client_LDADD += -lX11 -lXdamage -lXrender -lXext -lnss3 -lssl3 -lXfixes -lXcomposite /usr/lib/x86_64-linux-gnu/libboost_system.so

endif

if OS_DARWIN

ndnrtc_client_CPPFLAGS += -DOS_DARWIN

endif

#################
# unit tests    #
#################
GTEST_ROOT=$(top_srcdir)/contrib/gtest/googletest
GMOCK_ROOT=$(top_srcdir)/contrib/gtest/googlemock
UNIT_TESTS_CPPFLAGS_= -I${GTEST_ROOT} -I${GTEST_ROOT}/include -I${GMOCK_ROOT} -I${GMOCK_ROOT}/include -I$(top_srcdir) -I$(top_srcdir)/include -I$(includedir) ${ndnrtc_client_CPPFLAGS}
UNIT_TESTS_LDFLAGS_=${libndnrtc_la_LDFLAGS}  ${ndnrtc_client_LDFLAGS}
UNIT_TESTS_LDADD_=${libndnrtc_la_LIBADD} ${ndnrtc_client_LDADD}
UNIT_TESTS_COMMON_SOURCES_ = tests/tests-helpers.cc contrib/gtest/googlemock/src/gmock-all.cc contrib/gtest/googletest/src/gtest-all.cc

if OS_DARWIN

UNIT_TESTS_CPPFLAGS_ += -DOS_DARWIN

endif

check_PROGRAMS = bin/tests/test-params bin/tests/test-client-params bin/tests/test-config-load bin/tests/test-frame-io bin/tests/test-generator bin/tests/test-video-source bin/tests/test-renderer bin/tests/test-stat-collector bin/tests/test-client

bin_tests_test_config_load_SOURCES = tests/test-config-load.cc client/src/config.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_config_load_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_config_load_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_config_load_LDADD = ${UNIT_TESTS_LDADD_}

bin_tests_test_params_SOURCES = tests/test-params.cc ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_params_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_params_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_params_LDADD = ${UNIT_TESTS_LDADD_}

bin_tests_test_client_params_SOURCES = tests/test-client-params.cc ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_client_params_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_client_params_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_client_params_LDADD = ${UNIT_TESTS_LDADD_}

bin_tests_test_client_SOURCES = tests/test-client.cc client/src/client.cpp client/src/stat-collector.cpp client/src/renderer.cpp client/src/frame-io.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_client_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_client_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_client_LDADD = ${UNIT_TESTS_LDADD_}

bin_tests_test_stat_collector_SOURCES = tests/test-stat-collector.cc client/src/stat-collector.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_stat_collector_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_stat_collector_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_stat_collector_LDADD = ${UNIT_TESTS_LDADD_}

bin_tests_test_renderer_SOURCES = tests/test-renderer.cc client/src/renderer.cpp client/src/frame-io.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_renderer_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_renderer_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_renderer_LDADD = ${UNIT_TESTS_LDADD_}

bin_tests_test_frame_io_SOURCES = tests/test-frame-io.cc client/src/frame-io.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_frame_io_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_frame_io_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_frame_io_LDADD = ${UNIT_TESTS_LDADD_}

bin_tests_test_video_source_SOURCES = tests/test-video-source.cc client/src/frame-io.cpp client/src/video-source.cpp client/src/precise-generator.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_video_source_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_video_source_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_video_source_LDADD = ${UNIT_TESTS_LDADD_}

bin_tests_test_generator_SOURCES = tests/test-generator.cc client/src/precise-generator.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_generator_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_generator_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_generator_LDADD = ${UNIT_TESTS_LDADD_}

TESTS = ${check_PROGRAMS}
