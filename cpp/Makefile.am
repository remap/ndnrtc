ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

pkginclude_HEADERS = include/ndnrtc-common.hpp \
  include/params.hpp \
  include/statistics.hpp \
  include/interfaces.hpp \
  include/name-components.hpp \
  include/error-codes.hpp \
  include/ndnrtc-defines.hpp \
  include/simple-log.hpp \
  include/stream.hpp \
  include/local-stream.hpp \
  include/remote-stream.hpp \
  include/c-wrapper.h

helpersincludedir = $(pkgincludedir)/helpers
helpersinclude_HEADERS = include/helpers/key-chain-manager.hpp \
  include/helpers/face-processor.hpp

lib_LTLIBRARIES = libndnrtc.la
libndnrtc_la_SOURCES = src/async.cpp src/async.hpp \
  src/audio-capturer.cpp src/audio-capturer.hpp \
  src/audio-controller.cpp src/audio-controller.hpp \
  src/audio-playout.cpp src/audio-playout.hpp \
  src/audio-playout-impl.cpp src/audio-playout-impl.hpp \
  src/audio-renderer.cpp src/audio-renderer.hpp \
  src/audio-stream-impl.cpp src/audio-stream-impl.hpp \
  src/audio-thread.cpp src/audio-thread.hpp \
  src/av-sync.cpp src/av-sync.hpp \
  src/buffer-control.cpp src/buffer-control.hpp \
  src/clock.cpp src/clock.hpp \
  src/c-wrapper.cpp include/c-wrapper.h \
  src/consumer-storage.hpp src/consumer-storage.cpp \
  src/data-validator.cpp src/data-validator.hpp \
  src/drd-estimator.cpp src/drd-estimator.hpp \
  src/estimators.cpp src/estimators.hpp \
  src/helpers/face-processor.cpp \
  src/fec.cpp src/fec.hpp \
  src/frame-buffer.cpp src/frame-buffer.hpp \
  src/frame-converter.cpp src/frame-converter.hpp \
  src/frame-data.cpp src/frame-data.hpp \
  src/interest-control.cpp src/interest-control.hpp \
  src/interest-queue.cpp src/interest-queue.hpp \
  src/jitter-timing.cpp src/jitter-timing.hpp \
  src/helpers/key-chain-manager.cpp \
  src/latency-control.cpp src/latency-control.hpp \
  src/local-stream.cpp include/local-stream.hpp \
  src/media-stream-base.cpp src/media-stream-base.hpp \
  src/meta-fetcher.cpp src/meta-fetcher.hpp \
  src/name-components.cpp include/name-components.hpp \
  src/network-data.cpp src/network-data.hpp \
  src/ndnrtc-debug.hpp \
  src/ndnrtc-endian.hpp \
  src/ndnrtc-object.cpp src/ndnrtc-object.hpp \
  src/ndnrtc-testing.hpp \
  src/packet-publisher.cpp src/packet-publisher.hpp \
  src/periodic.cpp src/periodic.hpp \
  src/pipeline-control-state-machine.cpp src/pipeline-control-state-machine.hpp \
  src/pipeline-control.cpp src/pipeline-control.hpp \
  src/pipeliner.cpp src/pipeliner.hpp \
  src/playout-control.cpp src/playout-control.hpp \
  src/playout.cpp src/playout.hpp \
  src/playout-impl.cpp src/playout-impl.hpp \
  src/rate-adaptation-module.hpp \
  src/remote-audio-stream.cpp src/remote-audio-stream.hpp \
  src/remote-stream-impl.cpp src/remote-stream-impl.hpp \
  src/remote-stream.cpp include/remote-stream.hpp \
  src/remote-video-stream.cpp src/remote-video-stream.hpp \
  src/renderer.hpp \
  src/rtx-controller.cpp src/rtx-controller.hpp \
  src/sample-estimator.cpp src/sample-estimator.hpp \
  src/sample-validator.cpp src/sample-validator.hpp \
  src/segment-controller.cpp src/segment-controller.hpp \
  src/segment-fetcher.cpp src/segment-fetcher.hpp \
  src/simple-log.cpp include/simple-log.hpp \
  src/slot-buffer.cpp src/slot-buffer.hpp \
  src/statistics.cpp include/statistics.hpp \
  src/stream.hpp include/stream.hpp \
  src/threading-capability.cpp src/threading-capability.hpp \
  src/video-coder.cpp src/video-coder.hpp \
  src/video-decoder.cpp src/video-decoder.hpp \
  src/video-playout.cpp src/video-playout.hpp \
  src/video-playout-impl.cpp src/video-playout-impl.hpp \
  src/video-stream-impl.cpp src/video-stream-impl.hpp \
  src/video-thread.cpp src/video-thread.hpp \
  src/webrtc-audio-channel.cpp src/webrtc-audio-channel.hpp \
  src/webrtc.hpp \
  src/persistent-storage/frame-fetcher.cpp include/frame-fetcher.hpp \
  src/persistent-storage/fetching-task.cpp src/persistent-storage/fetching-task.hpp \
  src/persistent-storage/persistent-storage.cpp src/persistent-storage/persistent-storage.hpp \
  src/persistent-storage/storage-engine.cpp include/storage-engine.hpp


libndnrtc_la_CPPFLAGS = -fPIC -I$(top_srcdir)/include -I$(top_srcdir)/src ${BOOST_CPPFLAGS} -I@WEBRTCDIR@ -I@WEBRTCSRC@ -I@NDNCPPDIR@ -I@OPENFECSRC@ -D BASE_FILE_NAME=\"$*\"
libndnrtc_la_LDFLAGS = -L@NDNCPPLIB@ -L@OPENFECLIB@ -L@WEBRTCLIB@ -L@BOOSTLIB@ ${BOOST_LDFLAGS}
libndnrtc_la_LIBADD = -lndn-cpp -lopenfec ${BOOST_SYSTEM_LIB} ${BOOST_TIMER_LIB} ${BOOST_CHRONO_LIB} ${BOOST_ASIO_LIB} ${BOOST_THREAD_LIB} ${BOOST_REGEX_LIB}

if OS_DARWIN

libndnrtc_la_LDFLAGS += -flat_namespace -L@WEBRTCDIR@/out/Default/allibs
libndnrtc_la_LIBADD += -lana_config_proto -lana_debug_dump_proto -lapprtc_common -lapprtc_signaling -laudio -laudio_coder -laudio_coding -laudio_conference_mixer -laudio_device -laudio_encoder_interface -laudio_format_conversion -laudio_frame_manipulator -laudio_frame_operations -laudio_level -laudio_mixer_impl -laudio_network_adaptor -laudio_processing -laudio_processing_sse2 -laudioproc_debug_proto -laudioproc_protobuf_utils -laudioproc_unittest_proto -lbitrate_controller -lboringssl -lbuiltin_audio_decoder_factory -lbuiltin_audio_decoder_factory_internal -lbuiltin_audio_encoder_factory -lbwe_simulator_lib -lcall -lchart_proto -lcng -lcommand_line_parser -lcommon_audio -lcommon_audio_sse2 -lcommon_video -lcongestion_controller -ldesktop_capture -ldesktop_capture_differ_sse2 -ldl -levent_log_visualizer_utils -lexpat -lfield_trial_default -lfile_player -lfile_recorder -lframe_editing_lib -lg711 -lg722 -lgmock -lgtest -lilbc -lisac -lisac_c -lisac_common -lisac_fix -ljingle_peerconnection -ljingle_peerconnection_api -ljpeg -llegacy_encoded_audio_frame -lmedia_file -lmetrics_default -lneteq -lneteq_unittest_proto -lopus -lortc -lpacing -lpcm16b -lprimitives -lprotobuf_full -lprotobuf_lite -lprotoc_lib -lred -lreference_less_video_analysis_lib -lremote_bitrate_estimator -lrent_a_codec -lrtc_base -lrtc_base_approved -lrtc_event_log_impl -lrtc_event_log_parser -lrtc_event_log_proto -lrtc_event_log_source -lrtc_media -lrtc_media_base -lrtc_numerics -lrtc_p2p -lrtc_pc -lrtc_sdk_common_objc -lrtc_sdk_peerconnection_objc -lrtc_stats -lrtc_task_queue -lrtp_rtcp -lsimd -lsimd_asm -lsocketrocket -lsrtp -lstunprober -lsystem_wrappers -lusrsctp -lutility -lvideo -lvideo_capture -lvideo_capture_internal_impl -lvideo_capture_module -lvideo_coding -lvideo_coding_utility -lvideo_processing -lvideo_processing_sse2 -lvideo_quality_analysis -lvoice_engine -lvpx -lvpx_yasm -lwebrtc -lwebrtc_common -lwebrtc_h264 -lwebrtc_h264_video_toolbox -lwebrtc_i420 -lwebrtc_opus -lwebrtc_vp8 -lwebrtc_vp9 -lyasm_utils -lyuv_internal

else

.PHONY: libwebrtc-all.mri libwebrtc-all.a

libwebrtc-all.mri:
  $(shell echo "create libwebrtc-all.a" > @WEBRTCLIB@/libwebrtc-all.mri && \
  for lib in $$(find @WEBRTCLIB@ -name '*.a'); do \
  echo "addlib $$lib" >> @WEBRTCLIB@/libwebrtc-all.mri; done && \
  echo "save" >> @WEBRTCLIB@/libwebrtc-all.mri && \
  echo "end" >> @WEBRTCLIB@/libwebrtc-all.mri)

libwebrtc-all.a: libwebrtc-all.mri
  $(shell cd @WEBRTCLIB@ && @AR@ -M <libwebrtc-all.mri)

libndnrtc_la_DEPENDENCIES = libwebrtc-all.a
libndnrtc_la_LIBADD += -lwebrtc-all

endif

if OS_ANDROID

libndnrtc_la_CPPFLAGS += -fexceptions -frtti
libndnrtc_la_LIBADD += -llog -lOpenSLES -lboost_timer -lboost_thread

endif

# bin programs
bin_PROGRAMS = frame-fetcher stream-scrubber

frame_fetcher_SOURCES = tools/frame-fetcher/main.cpp \
    contrib/docopt/docopt.cpp
frame_fetcher_CXXFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src ${BOOST_CPPFLAGS} -I@NDNCPPDIR@ -I@WEBRTCDIR@ -I@WEBRTCSRC@ -I@OPENFECSRC@
frame_fetcher_LDFLAGS =  -L@NDNCPPLIB@ -L@BOOSTLIB@ ${BOOST_LDFLAGS}
frame_fetcher_LDADD = libndnrtc.la -lndn-cpp ${BOOST_SYSTEM_LIB} ${BOOST_TIMER_LIB} ${BOOST_CHRONO_LIB} ${BOOST_ASIO_LIB} ${BOOST_THREAD_LIB}

stream_scrubber_SOURCES = tools/stream-scrubber/main.cpp \
    contrib/docopt/docopt.cpp \
    tools/stream-scrubber/renderer.cpp tools/stream-scrubber/renderer.hpp
stream_scrubber_CXXFLAGS =  -I$(top_srcdir)/include -I$(top_srcdir)/src ${BOOST_CPPFLAGS} -I@NDNCPPDIR@ -I@WEBRTCDIR@ -I@WEBRTCSRC@ -I@OPENFECSRC@
stream_scrubber_LDFLAGS = -L@NDNCPPLIB@ -L@BOOSTLIB@ ${BOOST_LDFLAGS}
stream_scrubber_LDADD = libndnrtc.la -lndn-cpp ${BOOST_SYSTEM_LIB} ${BOOST_TIMER_LIB} ${BOOST_CHRONO_LIB} ${BOOST_ASIO_LIB} ${BOOST_THREAD_LIB}

if HAVE_PERSISTENT_STORAGE

libndnrtc_la_CPPFLAGS += -I@PSTORAGEDIR@ -DHAVE_PERSISTENT_STORAGE
libndnrtc_la_LDFLAGS += -L@PSTORAGELIB@
libndnrtc_la_LIBADD += ${PSTORAGE_LIB}

bin_PROGRAMS += stream-recorder networked-storage

stream_recorder_SOURCES = tools/stream-recorder/main.cpp \
    tools/stream-recorder/stream-recorder.hpp tools/stream-recorder/stream-recorder.cpp \
    contrib/docopt/docopt.cpp
stream_recorder_CXXFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src ${BOOST_CPPFLAGS} -I@NDNCPPDIR@ 
stream_recorder_LDFLAGS =  -L@NDNCPPLIB@ -L@BOOSTLIB@ ${BOOST_LDFLAGS}
stream_recorder_LDADD = libndnrtc.la -lndn-cpp ${BOOST_SYSTEM_LIB} ${BOOST_TIMER_LIB} ${BOOST_CHRONO_LIB} ${BOOST_ASIO_LIB} ${BOOST_THREAD_LIB}

networked_storage_SOURCES = tools/networked-storage/main.cpp \
    contrib/docopt/docopt.cpp
networked_storage_CXXFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src ${BOOST_CPPFLAGS} -I@NDNCPPDIR@ 
networked_storage_LDFLAGS =  -L@NDNCPPLIB@ -L@BOOSTLIB@ ${BOOST_LDFLAGS}
networked_storage_LDADD = libndnrtc.la -lndn-cpp ${BOOST_SYSTEM_LIB} ${BOOST_TIMER_LIB} ${BOOST_CHRONO_LIB} ${BOOST_ASIO_LIB} ${BOOST_THREAD_LIB}

endif

#################
# client app	  #
#################
#bin_PROGRAMS = ndnrtc-client
EXTRA_PROGRAMS = ndnrtc-client
ndnrtc_client_SOURCES = client/src/main.cpp client/src/renderer.hpp client/src/renderer.cpp client/src/config.cpp client/src/config.hpp client/src/stat-collector.cpp client/src/stat-collector.hpp client/src/client.cpp client/src/client.hpp client/src/frame-io.hpp client/src/frame-io.cpp client/src/video-source.cpp client/src/video-source.hpp client/src/precise-generator.hpp client/src/precise-generator.cpp
ndnrtc_client_CPPFLAGS = -I$(top_srcdir)/client/src -I@LCONFIGDIR@ ${BOOST_CPPFLAGS} -I$(includedir) -I@NDNCPPDIR@
ndnrtc_client_LDFLAGS = -L@LCONFIGLIB@ -L@NDNCPPLIB@ ${BOOST_LDFLAGS} -L$(libdir)
ndnrtc_client_LDADD = -lconfig++ -lndn-cpp ${BOOST_SYSTEM_LIB} ${BOOST_CHRONO_LIB} ${BOOST_THREAD_LIB} $(top_builddir)/libndnrtc.la 
noinst_SCRIPTS = client/params.cfg

if HAVE_NANOMSG
ndnrtc_client_SOURCES += client/src/ipc-shim.c client/src/ipc-shim.h
ndnrtc_client_CPPFLAGS += -I@NANOMSGDIR@ -DHAVE_NANOMSG
ndnrtc_client_LDFLAGS += -L@NANOMSGLIB@
ndnrtc_client_LDADD += -lnanomsg
endif

if OS_LINUX

ndnrtc_client_LDFLAGS += -pthread 
ndnrtc_client_LDADD += -ldl -lX11 -lXdamage -lXrender -lXext -lnss3 -lssl3 -lXfixes -lXcomposite
# TODO: check ubuntu build after commenting below line (part of LDADD flags)
#/usr/lib/x86_64-linux-gnu/libboost_system.so

endif

if OS_DARWIN

ndnrtc_client_CPPFLAGS += -DOS_DARWIN
ndnrtc_client_LDFLAGS += -framework CoreServices -framework CoreFoundation -framework AudioUnit -framework AudioToolbox -framework CoreAudio -L@WEBRTCDIR@/out/Default/allibs
ndnrtc_client_LDADD += -lana_config_proto -lana_debug_dump_proto -lapprtc_common -lapprtc_signaling -laudio -laudio_coder -laudio_coding -laudio_conference_mixer -laudio_device -laudio_encoder_interface -laudio_format_conversion -laudio_frame_manipulator -laudio_frame_operations -laudio_level -laudio_mixer_impl -laudio_network_adaptor -laudio_processing -laudio_processing_sse2 -laudioproc_debug_proto -laudioproc_protobuf_utils -laudioproc_unittest_proto -lbitrate_controller -lboringssl -lbuiltin_audio_decoder_factory -lbuiltin_audio_decoder_factory_internal -lbuiltin_audio_encoder_factory -lbwe_simulator_lib -lcall -lchart_proto -lcng -lcommand_line_parser -lcommon_audio -lcommon_audio_sse2 -lcommon_video -lcongestion_controller -ldesktop_capture -ldesktop_capture_differ_sse2 -ldl -levent_log_visualizer_utils -lexpat -lfield_trial_default -lfile_player -lfile_recorder -lframe_editing_lib -lg711 -lg722 -lgmock -lgtest -lilbc -lisac -lisac_c -lisac_common -lisac_fix -ljingle_peerconnection -ljingle_peerconnection_api -ljpeg -llegacy_encoded_audio_frame -lmedia_file -lmetrics_default -lneteq -lneteq_unittest_proto -lopus -lortc -lpacing -lpcm16b -lprimitives -lprotobuf_full -lprotobuf_lite -lprotoc_lib -lred -lreference_less_video_analysis_lib -lremote_bitrate_estimator -lrent_a_codec -lrtc_base -lrtc_base_approved -lrtc_event_log_impl -lrtc_event_log_parser -lrtc_event_log_proto -lrtc_event_log_source -lrtc_media -lrtc_media_base -lrtc_numerics -lrtc_p2p -lrtc_pc -lrtc_sdk_common_objc -lrtc_sdk_peerconnection_objc -lrtc_stats -lrtc_task_queue -lrtp_rtcp -lsimd -lsimd_asm -lsocketrocket -lsrtp -lstunprober -lsystem_wrappers -lusrsctp -lutility -lvideo -lvideo_capture -lvideo_capture_internal_impl -lvideo_capture_module -lvideo_coding -lvideo_coding_utility -lvideo_processing -lvideo_processing_sse2 -lvideo_quality_analysis -lvoice_engine -lvpx -lvpx_yasm -lwebrtc -lwebrtc_common -lwebrtc_h264 -lwebrtc_h264_video_toolbox -lwebrtc_i420 -lwebrtc_opus -lwebrtc_vp8 -lwebrtc_vp9 -lyasm_utils -lyuv_internal

endif

if OS_ANDROID

ndnrtc_client_CPPFLAGS += -fPIE -fexceptions -frtti
ndnrtc_client_LDFLAGS += -fPIE -pie
ndnrtc_client_LDADD += -llog -lOpenSLES -lboost_timer -lboost_thread

endif

if HAVE_NANOMSG

EXTRA_PROGRAMS += nanopipe-adaptor
 # nanopipe.c ipc-shim.c -std=c++11 -I/usr/local/include -L/usr/local/lib -lnanomsg -onanopipe
nanopipe_adaptor_SOURCES = client/nanopipe-adaptor/nanopipe.cpp client/nanopipe-adaptor/ipc-shim.c
nanopipe_adaptor_CXXFLAGS = -std=c++11
nanopipe_adaptor_CPPFLAGS = -I@NANOMSGDIR@ -I$(includedir)
nanopipe_adaptor_LDFLAGS = -L@NANOMSGLIB@
nanopipe_adaptor_LDADD = -lnanomsg

endif

#################
# unit tests    #
#################

GTEST_ROOT=$(top_srcdir)/contrib/gtest/googletest
GMOCK_ROOT=$(top_srcdir)/contrib/gtest/googlemock
UNIT_TESTS_CPPFLAGS_= -I${GTEST_ROOT} -I${GTEST_ROOT}/include -I${GMOCK_ROOT} -I${GMOCK_ROOT}/include \
    -I$(top_srcdir) -I$(top_srcdir)/include -I$(includedir) ${ndnrtc_client_CPPFLAGS} ${libndnrtc_la_CPPFLAGS} @DEFS@
UNIT_TESTS_LDFLAGS_=${libndnrtc_la_LDFLAGS} ${ndnrtc_client_LDFLAGS}
UNIT_TESTS_LDADD_=${libndnrtc_la_LIBADD} -lconfig++
UNIT_TESTS_COMMON_SOURCES_ = contrib/gtest/googlemock/src/gmock-all.cc contrib/gtest/googletest/src/gtest-all.cc

if HAVE_NANOMSG
UNIT_TESTS_COMMON_SOURCES_ += client/src/ipc-shim.c client/src/ipc-shim.h
UNIT_TESTS_CPPFLAGS_ += -I@NANOMSGDIR@ -DHAVE_NANOMSG
UNIT_TESTS_LDFLAGS_ += -L@NANOMSGLIB@
UNIT_TESTS_LDADD_ += -lnanomsg
endif

if OS_LINUX

UNIT_TESTS_LDADD_ += -ldl -lX11

endif

if OS_DARWIN

UNIT_TESTS_CPPFLAGS_ += -DOS_DARWIN

endif

res/test-source-1280x720.argb:
	mkdir -p $(top_builddir)/res/
	$(WGET) https://s3.amazonaws.com/ndnrtc-test-files/raw/test-source-1280x720.argb.tar.gz
	$(TAR) -xf test-source-1280x720.argb.tar.gz -C $(top_builddir)/res/

res/test-source-320x240.argb:
	mkdir -p $(top_builddir)/res/
	$(WGET) https://s3.amazonaws.com/ndnrtc-test-files/raw/test-source-320x240.argb.tar.gz
	$(TAR) -xf test-source-320x240.argb.tar.gz -C $(top_builddir)/res/

check_PROGRAMS = bin/tests/test-params bin/tests/test-network-data bin/tests/test-packet-publisher bin/tests/test-data-validator bin/tests/test-video-coder bin/tests/test-video-decoder bin/tests/test-webrtc-audio-channel bin/tests/test-media-thread bin/tests/test-audio-capturer bin/tests/test-frame-converter bin/tests/test-estimators bin/tests/test-async bin/tests/test-name-components bin/tests/test-local-media-stream bin/tests/test-frame-buffer bin/tests/test-rtx-controller bin/tests/test-playout bin/tests/test-video-playout bin/tests/test-audio-playout bin/tests/test-segment-controller bin/tests/test-periodic bin/tests/test-sample-estimator bin/tests/test-drd-estimator bin/tests/test-latency-control bin/tests/test-buffer-control bin/tests/test-interest-control bin/tests/test-pipeline-control bin/tests/test-pipeliner bin/tests/test-pipeline-control-state-machine bin/tests/test-interest-queue bin/tests/test-playout-control bin/tests/test-loop bin/tests/test-video-source bin/tests/test-config-load bin/tests/test-client-params bin/tests/test-frame-io bin/tests/test-generator bin/tests/test-video-source bin/tests/test-renderer bin/tests/test-stat-collector bin/tests/test-client

if HAVE_PERSISTENT_STORAGE
    check_PROGRAMS += bin/tests/test-persistent-storage
endif

bin_tests_test_config_load_SOURCES = tests/test-config-load.cc client/src/config.cpp src/simple-log.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_config_load_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_config_load_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_config_load_LDADD = ${UNIT_TESTS_LDADD_} $(top_builddir)/libndnrtc.la 

bin_tests_test_client_params_SOURCES = tests/test-client-params.cc tests/tests-helpers.cc client/src/config.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_client_params_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_client_params_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_client_params_LDADD = $(top_builddir)/libndnrtc.la ${UNIT_TESTS_LDADD_} 

bin_tests_test_stat_collector_SOURCES = tests/test-stat-collector.cc client/src/stat-collector.cpp client/src/precise-generator.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_stat_collector_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_stat_collector_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_stat_collector_LDADD = $(top_builddir)/libndnrtc.la ${UNIT_TESTS_LDADD_}

bin_tests_test_renderer_SOURCES = tests/test-renderer.cc client/src/renderer.cpp client/src/frame-io.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_renderer_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_renderer_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_renderer_LDADD = $(top_builddir)/libndnrtc.la ${UNIT_TESTS_LDADD_} 

bin_tests_test_frame_io_SOURCES = tests/test-frame-io.cc client/src/frame-io.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_frame_io_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_frame_io_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_frame_io_LDADD = $(top_builddir)/libndnrtc.la ${UNIT_TESTS_LDADD_} 

bin_tests_test_video_source_SOURCES = tests/test-video-source.cc client/src/frame-io.cpp client/src/video-source.cpp client/src/precise-generator.cpp src/simple-log.cpp src/estimators.cpp src/clock.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_video_source_DEPENDENCIES = res/test-source-1280x720.argb
bin_tests_test_video_source_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_video_source_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_video_source_LDADD = ${libndnrtc_la_LIBADD} $(top_builddir)/libndnrtc.la ${UNIT_TESTS_LDADD_}

bin_tests_test_generator_SOURCES = tests/test-generator.cc client/src/precise-generator.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_generator_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_generator_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_generator_LDADD = $(top_builddir)/libndnrtc.la ${UNIT_TESTS_LDADD_} 

bin_tests_test_client_SOURCES = tests/test-client.cc client/src/client.cpp client/src/stat-collector.cpp client/src/renderer.cpp client/src/frame-io.cpp client/src/video-source.cpp client/src/precise-generator.cpp client/src/config.cpp tests/tests-helpers.cc ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_client_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_client_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_client_LDADD = $(top_builddir)/libndnrtc.la ${UNIT_TESTS_LDADD_} 


### NDN-RTC tests

bin_tests_test_params_SOURCES = tests/test-params.cc tests/tests-helpers.cc src/fec.cpp src/name-components.cpp src/frame-data.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_params_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_params_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_params_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_data_validator_SOURCES = tests/test-data-validator.cc src/data-validator.cpp  src/simple-log.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_data_validator_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_data_validator_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_data_validator_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_network_data_SOURCES = tests/test-network-data.cc tests/tests-helpers.cc src/frame-data.cpp src/fec.cpp src/name-components.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_network_data_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_network_data_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_network_data_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_packet_publisher_SOURCES = tests/test-packet-publisher.cc tests/tests-helpers.cc src/packet-publisher.cpp src/frame-data.cpp src/fec.cpp src/ndnrtc-object.cpp src/simple-log.cpp src/name-components.cpp src/statistics.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_packet_publisher_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_packet_publisher_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_packet_publisher_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_video_coder_SOURCES = tests/test-video-coder.cc tests/tests-helpers.cc src/video-coder.cpp src/ndnrtc-object.cpp src/simple-log.cpp src/fec.cpp src/name-components.cpp src/frame-data.cpp src/threading-capability.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_video_coder_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_video_coder_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_video_coder_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_video_decoder_SOURCES = tests/test-video-decoder.cc tests/tests-helpers.cc src/video-decoder.cpp src/video-coder.cpp src/ndnrtc-object.cpp src/simple-log.cpp src/fec.cpp src/name-components.cpp src/frame-data.cpp src/clock.cpp src/threading-capability.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_video_decoder_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_video_decoder_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_video_decoder_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_media_thread_SOURCES = tests/test-media-thread.cc src/video-thread.cpp tests/tests-helpers.cc src/video-coder.cpp src/frame-data.cpp src/fec.cpp src/audio-thread.cpp src/audio-capturer.cpp src/webrtc-audio-channel.cpp src/audio-controller.cpp src/threading-capability.cpp src/ndnrtc-object.cpp src/simple-log.cpp src/estimators.cpp src/clock.cpp src/name-components.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_media_thread_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_media_thread_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_media_thread_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_webrtc_audio_channel_SOURCES = tests/test-webrtc-audio-channel.cc src/webrtc-audio-channel.cpp src/audio-controller.cpp src/threading-capability.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_webrtc_audio_channel_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_webrtc_audio_channel_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_webrtc_audio_channel_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_} 

bin_tests_test_audio_capturer_SOURCES = tests/test-audio-capturer.cc tests/tests-helpers.cc src/audio-capturer.cpp src/webrtc-audio-channel.cpp src/audio-controller.cpp src/simple-log.cpp src/threading-capability.cpp src/ndnrtc-object.cpp src/fec.cpp src/name-components.cpp src/frame-data.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_audio_capturer_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_audio_capturer_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_audio_capturer_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_} 

bin_tests_test_frame_converter_SOURCES = tests/test-frame-converter.cc tests/tests-helpers.cc src/fec.cpp src/frame-converter.cpp src/name-components.cpp src/frame-data.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_frame_converter_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_frame_converter_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_frame_converter_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_estimators_SOURCES = tests/test-estimators.cc src/estimators.cpp src/clock.cpp client/src/precise-generator.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_estimators_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_estimators_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_estimators_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_async_SOURCES = tests/test-async.cc src/async.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_async_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_async_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_async_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_name_components_SOURCES = tests/test-name-components.cc src/name-components.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_name_components_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_name_components_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_name_components_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_local_media_stream_SOURCES = tests/test-local-media-stream.cc tests/tests-helpers.cc src/local-stream.cpp src/video-stream-impl.cpp src/video-thread.cpp src/video-coder.cpp src/frame-data.cpp src/fec.cpp src/audio-thread.cpp src/audio-capturer.cpp src/webrtc-audio-channel.cpp src/audio-controller.cpp src/threading-capability.cpp src/ndnrtc-object.cpp src/simple-log.cpp src/name-components.cpp src/frame-converter.cpp src/estimators.cpp src/clock.cpp src/async.cpp src/audio-stream-impl.cpp src/media-stream-base.cpp src/periodic.cpp src/statistics.cpp src/persistent-storage/storage-engine.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_local_media_stream_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_local_media_stream_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_local_media_stream_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_frame_buffer_SOURCES = tests/test-frame-buffer.cc tests/tests-helpers.cc src/frame-buffer.cpp src/name-components.cpp src/frame-data.cpp src/fec.cpp src/clock.cpp src/simple-log.cpp src/ndnrtc-object.cpp src/statistics.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_frame_buffer_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_frame_buffer_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_frame_buffer_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_rtx_controller_SOURCES = tests/test-rtx-controller.cc tests/tests-helpers.cc src/rtx-controller.cpp src/frame-buffer.cpp src/name-components.cpp src/frame-data.cpp src/fec.cpp src/clock.cpp src/simple-log.cpp src/ndnrtc-object.cpp src/statistics.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_rtx_controller_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_rtx_controller_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_rtx_controller_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_playout_SOURCES = tests/test-playout.cc tests/tests-helpers.cc src/frame-buffer.cpp src/name-components.cpp src/frame-data.cpp src/fec.cpp src/clock.cpp src/simple-log.cpp src/ndnrtc-object.cpp src/async.cpp src/jitter-timing.cpp src/playout.cpp src/playout-impl.cpp src/statistics.cpp client/src/video-source.cpp client/src/precise-generator.cpp client/src/frame-io.cpp src/frame-converter.cpp src/video-thread.cpp src/video-coder.cpp src/threading-capability.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_playout_DEPENDENCIES = res/test-source-320x240.argb
bin_tests_test_playout_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_playout_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_playout_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_video_playout_SOURCES = tests/test-video-playout.cc tests/tests-helpers.cc src/video-playout.cpp src/frame-buffer.cpp src/name-components.cpp src/frame-data.cpp src/fec.cpp src/clock.cpp src/simple-log.cpp src/ndnrtc-object.cpp src/async.cpp src/jitter-timing.cpp src/playout.cpp src/playout-impl.cpp src/video-playout-impl.cpp src/statistics.cpp client/src/video-source.cpp client/src/precise-generator.cpp client/src/frame-io.cpp src/frame-converter.cpp src/video-thread.cpp src/video-coder.cpp src/threading-capability.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_video_playout_DEPENDENCIES = res/test-source-320x240.argb
bin_tests_test_video_playout_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_video_playout_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_video_playout_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_audio_playout_SOURCES = tests/test-audio-playout.cc tests/tests-helpers.cc src/audio-playout.cpp src/frame-buffer.cpp src/name-components.cpp src/frame-data.cpp src/fec.cpp src/clock.cpp src/simple-log.cpp src/ndnrtc-object.cpp src/async.cpp src/jitter-timing.cpp src/playout.cpp src/playout-impl.cpp src/audio-playout-impl.cpp src/statistics.cpp  src/audio-thread.cpp src/estimators.cpp src/audio-capturer.cpp src/audio-controller.cpp src/webrtc-audio-channel.cpp src/threading-capability.cpp src/audio-renderer.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_audio_playout_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_audio_playout_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_audio_playout_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_segment_controller_SOURCES = tests/test-segment-controller.cc src/segment-controller.cpp src/simple-log.cpp src/ndnrtc-object.cpp src/name-components.cpp src/frame-data.cpp src/async.cpp src/periodic.cpp src/clock.cpp src/statistics.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_segment_controller_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_segment_controller_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_segment_controller_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_periodic_SOURCES = tests/test-periodic.cc src/periodic.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_periodic_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_periodic_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_periodic_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_sample_estimator_SOURCES = tests/test-sample-estimator.cc tests/tests-helpers.cc src/fec.cpp src/sample-estimator.cpp src/estimators.cpp src/clock.cpp src/frame-data.cpp src/name-components.cpp src/statistics.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_sample_estimator_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_sample_estimator_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_sample_estimator_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_drd_estimator_SOURCES = tests/test-drd-estimator.cc src/drd-estimator.cpp src/estimators.cpp src/clock.cpp tests/tests-helpers.cc src/fec.cpp src/name-components.cpp src/frame-data.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_drd_estimator_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_drd_estimator_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_drd_estimator_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_latency_control_SOURCES = tests/test-latency-control.cc tests/tests-helpers.cc src/fec.cpp src/name-components.cpp src/latency-control.cpp src/estimators.cpp src/clock.cpp src/simple-log.cpp client/src/precise-generator.cpp src/frame-data.cpp src/drd-estimator.cpp src/ndnrtc-object.cpp src/statistics.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_latency_control_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_latency_control_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_latency_control_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_buffer_control_SOURCES = tests/test-buffer-control.cc src/buffer-control.cpp tests/tests-helpers.cc src/fec.cpp src/name-components.cpp src/frame-buffer.cpp src/frame-data.cpp src/clock.cpp src/simple-log.cpp src/drd-estimator.cpp src/ndnrtc-object.cpp src/estimators.cpp src/statistics.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_buffer_control_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_buffer_control_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_buffer_control_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_interest_control_SOURCES = tests/test-interest-control.cc src/interest-control.cpp tests/tests-helpers.cc src/fec.cpp src/name-components.cpp src/frame-data.cpp src/clock.cpp src/simple-log.cpp src/drd-estimator.cpp src/ndnrtc-object.cpp src/estimators.cpp src/statistics.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_interest_control_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_interest_control_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_interest_control_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_pipeline_control_state_machine_SOURCES = tests/test-pipeline-control-state-machine.cc src/pipeline-control-state-machine.cpp src/clock.cpp src/simple-log.cpp src/ndnrtc-object.cpp src/latency-control.cpp src/interest-control.cpp src/drd-estimator.cpp src/estimators.cpp tests/tests-helpers.cc src/name-components.cpp src/fec.cpp src/frame-data.cpp src/statistics.cpp src/sample-estimator.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_pipeline_control_state_machine_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_pipeline_control_state_machine_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_pipeline_control_state_machine_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_pipeliner_SOURCES = tests/test-pipeliner.cc src/pipeliner.cpp src/interest-control.cpp src/name-components.cpp src/frame-data.cpp src/clock.cpp src/simple-log.cpp src/ndnrtc-object.cpp src/estimators.cpp src/interest-queue.cpp src/segment-controller.cpp src/frame-buffer.cpp src/sample-estimator.cpp src/periodic.cpp src/fec.cpp src/async.cpp tests/tests-helpers.cc src/drd-estimator.cpp src/statistics.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_pipeliner_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_pipeliner_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_pipeliner_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_interest_queue_SOURCES = tests/test-interest-queue.cc tests/tests-helpers.cc src/interest-queue.cpp src/clock.cpp src/async.cpp src/simple-log.cpp src/ndnrtc-object.cpp src/statistics.cpp src/name-components.cpp src/fec.cpp src/frame-data.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_interest_queue_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_interest_queue_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_interest_queue_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_pipeline_control_SOURCES = tests/test-pipeline-control.cc src/pipeline-control.cpp src/interest-control.cpp src/segment-controller.cpp src/name-components.cpp src/frame-data.cpp src/clock.cpp src/simple-log.cpp src/ndnrtc-object.cpp src/estimators.cpp src/periodic.cpp src/pipeline-control-state-machine.cpp src/pipeliner.cpp src/frame-buffer.cpp src/fec.cpp src/sample-estimator.cpp src/interest-queue.cpp src/async.cpp tests/tests-helpers.cc src/drd-estimator.cpp src/statistics.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_pipeline_control_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_pipeline_control_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_pipeline_control_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_playout_control_SOURCES = tests/test-playout-control.cc src/playout-control.cpp src/ndnrtc-object.cpp src/simple-log.cpp src/estimators.cpp src/clock.cpp src/rtx-controller.cpp src/frame-buffer.cpp src/fec.cpp src/name-components.cpp src/frame-data.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_playout_control_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_playout_control_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
bin_tests_test_playout_control_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_loop_SOURCES = tests/test-loop.cc tests/tests-helpers.cc src/async.cpp src/audio-capturer.cpp src/audio-controller.cpp src/audio-playout.cpp src/audio-playout-impl.cpp src/audio-renderer.cpp src/audio-stream-impl.cpp src/audio-thread.cpp src/buffer-control.cpp src/clock.cpp src/data-validator.cpp src/drd-estimator.cpp src/estimators.cpp src/fec.cpp src/frame-buffer.cpp src/frame-converter.cpp src/frame-data.cpp src/interest-control.cpp src/interest-queue.cpp src/jitter-timing.cpp src/latency-control.cpp src/local-stream.cpp src/media-stream-base.cpp src/name-components.cpp src/ndnrtc-object.cpp src/packet-publisher.cpp src/periodic.cpp src/pipeline-control-state-machine.cpp src/pipeline-control.cpp src/pipeliner.cpp src/playout-control.cpp src/playout.cpp src/playout-impl.cpp src/remote-stream-impl.cpp src/remote-stream.cpp src/sample-estimator.cpp src/segment-controller.cpp src/simple-log.cpp src/slot-buffer.cpp src/statistics.cpp src/threading-capability.cpp src/video-coder.cpp src/video-decoder.cpp src/video-playout.cpp src/video-playout-impl.cpp src/video-stream-impl.cpp src/video-thread.cpp src/webrtc-audio-channel.cpp client/src/video-source.cpp client/src/precise-generator.cpp client/src/frame-io.cpp src/meta-fetcher.cpp src/remote-video-stream.cpp src/remote-audio-stream.cpp src/segment-fetcher.cpp src/sample-validator.cpp src/rtx-controller.cpp src/persistent-storage/storage-engine.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_loop_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
bin_tests_test_loop_LDFLAGS = ${UNIT_TESTS_LDFLAGS_} ${BOOST_FILESYSTEM_LIB}

bin_tests_test_loop_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_}

bin_tests_test_persistent_storage_SOURCES = tests/test-persistent-storage.cc tests/tests-helpers.cc src/packet-publisher.cpp src/frame-data.cpp src/fec.cpp src/ndnrtc-object.cpp src/simple-log.cpp src/name-components.cpp src/statistics.cpp  client/src/video-source.cpp client/src/precise-generator.cpp client/src/frame-io.cpp src/video-thread.cpp src/frame-converter.cpp src/video-coder.cpp src/frame-buffer.cpp src/persistent-storage/fetching-task.cpp src/persistent-storage/storage-engine.cpp src/persistent-storage/frame-fetcher.cpp src/clock.cpp src/video-decoder.cpp src/local-stream.cpp src/video-stream-impl.cpp src/media-stream-base.cpp src/audio-capturer.cpp src/periodic.cpp src/audio-stream-impl.cpp src/estimators.cpp src/audio-controller.cpp src/webrtc-audio-channel.cpp src/async.cpp src/audio-thread.cpp src/threading-capability.cpp ${UNIT_TESTS_COMMON_SOURCES_}
bin_tests_test_persistent_storage_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_} -I@PSTORAGEDIR@
bin_tests_test_persistent_storage_LDFLAGS = ${UNIT_TESTS_LDFLAGS_} -L@PSTORAGELIB@
bin_tests_test_persistent_storage_LDADD = ${libndnrtc_la_LIBADD} ${UNIT_TESTS_LDADD_} -lboost_filesystem ${PSTORAGE_LIB}

TESTS = ${check_PROGRAMS}

#################
# extra apps    #
#################

#noinst_PROGRAMS = bin/benchmark-local-stream

#bin_benchmark_local_stream_SOURCES = extra/benchmark-local-stream.cc tests/tests-helpers.cc src/local-stream.cpp src/video-stream-impl.cpp src/video-thread.cpp src/video-coder.cpp src/frame-data.cpp src/fec.cpp src/audio-thread.cpp src/audio-capturer.cpp src/webrtc-audio-channel.cpp src/audio-controller.cpp src/threading-capability.cpp src/ndnrtc-object.cpp src/simple-log.cpp src/name-components.cpp src/frame-converter.cpp src/estimators.cpp src/clock.cpp src/async.cpp src/audio-stream-impl.cpp src/media-stream-base.cpp src/periodic.cpp src/statistics.cpp client/src/video-source.cpp client/src/precise-generator.cpp client/src/frame-io.cpp ${UNIT_TESTS_COMMON_SOURCES_}
#bin_benchmark_local_stream_DEPENDENCIES = res/test-source-320x240.argb res/test-source-1280x720.argb
#bin_benchmark_local_stream_CPPFLAGS = ${UNIT_TESTS_CPPFLAGS_}
#bin_benchmark_local_stream_LDFLAGS = ${UNIT_TESTS_LDFLAGS_}
#bin_benchmark_local_stream_LDADD = ${libndnrtc_la_LIBADD}
