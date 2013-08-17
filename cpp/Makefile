MODULE		= libndnrtc
NAME 		= ndnrtc

CURRENT     = $(shell pwd)
IDL_PATH 	= $(CURRENT)/idl
SRC_PATH	= $(CURRENT)/src
BIN_PATH	= $(CURRENT)/bin
OBJ_PATH	= $(CURRENT)/obj
ADDON_SRC_PATH = $(CURRENT)/addon
ADDON_PATH	= $(BIN_PATH)/$(NAME)
CONTENT_PATH = $(CURRENT)/content
THIRDPARTY_PATH = $(CURRENT)/thirdparty

# third party
NDNC = ndn-c
NDNCPP = ndn-cpp
WEBRTC = webrtc

NDNCPPLIB = lib$(NDNCPP).a
NDNCLIB = lib$(NDNC).a

LIBBIN_PATH = $(THIRDPARTY_PATH)/bin
NDNCPPLIB_PATH = $(THIRDPARTY_PATH)/$(NDNCPP)

# dir for storing add-on chrome resources
ADDON_CHROME_DIR = $(ADDON_PATH)/chrome
# dir for storing add-on resources, js, etc. files
ADDON_CHROME_CONTENT_DIR = $(ADDON_CHROME_DIR)/content
# dir for storing add-on .xpt and .dylib files
ADDON_COMPONENTS_DIR = $(ADDON_PATH)/components

# Change this to point at your Gecko SDK directory or setup XULDSDK variable
GECKO_SDK_PATH ?= $(XULSDK)#../../mozilla/obj-xulrunner/dist
PYTHON 			= python


XPIDL_H	    = $(PYTHON) $(GECKO_SDK_PATH)/sdk/bin/header.py
XPIDL_XPT	= $(PYTHON) $(GECKO_SDK_PATH)/sdk/bin/typelib.py
CXX	       	= clang++

XPIDLSRCS	= $(patsubst $(IDL_PATH)/%.idl,%.idl,$(wildcard $(IDL_PATH)/*.idl))
CPPSRCS		= $(patsubst $(SRC_PATH)/%.cpp,%.cpp,$(wildcard $(SRC_PATH)/*.cpp))

CPPFLAGS +=     -fno-rtti		\
                -fshort-wchar		\
		-fPIC			\
        --std=c++0x \
		$(NULL)
		
LDFLAGS +=		-Wl, -dynamiclib -undefined suppress -flat_namespace

WEBRTC_TRUNK = $(THIRDPARTY_PATH)/$(WEBRTC)/trunk
WEBRTC_SRCPATH = $(WEBRTC_TRUNK)/webrtc

# change it for your WebRTC build directory
WEBRTC_LIBPATH = $(WEBRTC_TRUNK)/xcodebuild/Debug # change to Release for releasing
WEBRTC_INCLUDES = -I$(WEBRTC_SRCPATH) -I$(WEBRTC_TRUNK)

# add necessary libs from WebRTC
WEBRTC_LDFLAGS = -L$(WEBRTC_LIBPATH) \
	-laudio_coding_module \
	-laudio_conference_mixer \
	-laudio_device \
	-laudio_processing \
	-laudio_processing_sse2 \
	-laudioproc_debug_proto \
	-laudioproc_unittest_proto \
	-lbitrate_controller \
	-lchannel_transport \
	-lCNG \
	-lcommand_line_parser \
	-lcommon_audio \
	-lcommon_audio_sse2 \
	-lcommon_video \
	-ldesktop_capture \
	-ldesktop_capture_differ_sse2 \
	-lframe_editing_lib \
	-lG711 \
	-lG722 \
	-lgenperf_libs \
	-lgmock \
	-lgoogle-gflags \
	-lgtest \
	-liLBC \
	-liSAC \
	-liSACFix \
	-ljpeg_turbo \
	-lmedia_file \
	-lmetrics \
	-lNetEq \
	-lNetEq4 \
	-lNetEq4TestTools \
	-lneteq_unittest_tools \
	-lNetEqTestTools \
	-lopus \
	-lpaced_sender \
	-lPCM16B \
	-lprotobuf_full_do_not_use \
	-lprotobuf_lite \
	-lrbe_components \
	-lrbe_components_unittests \
	-lremote_bitrate_estimator \
	-lrtp_rtcp \
	-lsystem_wrappers \
	-ltest \
	-ltest_framework \
	-ltest_support \
	-ltest_support_main \
	-ltest_support_main_threaded_mac \
	-lvideo_capture_module \
	-lvideo_codecs_test_framework \
	-lvideo_coding_utility \
	-lvideo_engine_core \
	-lvideo_processing \
	-lvideo_processing_sse2 \
	-lvideo_quality_analysis \
	-lvideo_render_module \
	-lvideo_tests_common \
	-lvietest \
	-lvoice_engine \
	-lvpx \
	-lvpx_asm_offsets_vp8 \
	-lvpx_intrinsics_mmx \
	-lvpx_intrinsics_sse2 \
	-lvpx_intrinsics_ssse3 \
	-lwebrtc_i420 \
	-lwebrtc_opus \
	-lwebrtc_utility \
	-lwebrtc_video_coding \
	-lwebrtc_vp8 \
	-lyuv
#                    -lwebrtc
#-lvideo_engine_core \
#                    -lsystem_wrappers \
#                    -lrbe_components \
#                    -lwebrtc_utility \
#                    -lbitrate_controller \
#                    -lremote_bitrate_estimator

LDLIBFLAGS = -L$(LIBBIN_PATH) -l$(NDNCPP) -l$(NDNC) $(WEBRTC_LDFLAGS)
LIB_INCLUDES = -I$(LIBBIN_PATH)/include $(WEBRTC_INCLUDES)

# GCC only define which allows us to not have to #include mozilla-config
# in every .cpp file.  If your not using GCC remove this line and add
# #include "mozilla-config.h" to each of your .cpp files.
GECKO_CONFIG_INCLUDE = -include mozilla-config.h

GECKO_DEFINES  = -DMOZILLA_STRICT_API -DXP_UNIX -DXP_MACOSX

GECKO_INCLUDES = -I$(GECKO_SDK_PATH)                    \
                 -I$(GECKO_SDK_PATH)/idl                \
				 -I$(GECKO_SDK_PATH)/sdk/bin			\
                 -I$(GECKO_SDK_PATH)/include			\
				 -I$(GECKO_SDK_PATH)/include/nspr
                 
GECKO_LDFLAGS =  -L$(GECKO_SDK_PATH)/bin \
		 -L$(GECKO_SDK_PATH)/lib \
		 -lxpcomglue_s	\
#                 -lnspr4	\
#                 -lplds4	\
#		 -lxul		\
		 -shared	\
		 $(NULL)

.PHONY: build addon all

all: addon

build: $(MODULE).dylib

libclean:
	cd $(NDNCPPLIB_PATH) && make clean && cd $(CURRENT)

configlib:
	cd $(NDNCPPLIB_PATH) && ./configure && cd $(CURRENT)

libbuild: ndnbuild webrtcbuild

webrtcbuild:
	export GYP_GENERATORS=make
	cd $(WEBRTC_TRUNK) && gclient sync && make BUILDTYPE=Release

ndnbuild:
	cd $(NDNCPPLIB_PATH) && make  && cd $(CURRENT)
	mkdir -p $(LIBBIN_PATH)/include/$(NDNCPP)
	cp $(NDNCPPLIB_PATH)/.libs/$(NDNCLIB) $(LIBBIN_PATH)
	cp $(NDNCPPLIB_PATH)/.libs/$(NDNCPPLIB) $(LIBBIN_PATH)
	cp -Rf $(NDNCPPLIB_PATH)/$(NDNCPP) $(LIBBIN_PATH)/include/
	find $(LIBBIN_PATH)/include/$(NDNCPP)/ -name '*.[co]*' | xargs rm
	cp $(NDNCPPLIB_PATH)/config.h $(LIBBIN_PATH)/include/    
	cp -Rf $(NDNCPPLIB_PATH)/ndnboost $(LIBBIN_PATH)/include/

# creates a package for add-on in $(BIN_PATH)
addon: $(MODULE).dylib
	mkdir -p $(BIN_PATH)
	mkdir -p $(ADDON_PATH)
	mkdir -p $(ADDON_CHROME_DIR)
	mkdir -p $(ADDON_CHROME_CONTENT_DIR)
	mkdir -p $(ADDON_COMPONENTS_DIR)
	cp $(BIN_PATH)/*.* $(ADDON_COMPONENTS_DIR)
	cp $(CONTENT_PATH)/* $(ADDON_CHROME_CONTENT_DIR) 2>/dev/null || :
	cp -r $(ADDON_SRC_PATH)/* $(ADDON_PATH) 
	cd $(ADDON_PATH) && zip -X -r ../$(NAME).xpi * && cd ../..

# generates header files from .idl
gen_hdrs: $(XPIDLSRCS:%.idl=%.h)

# generates xpt files from .idl 
gen_xpt: $(XPIDLSRCS:%.idl=%.xpt)

%.h: $(IDL_PATH)/%.idl
	$(XPIDL_H) $(GECKO_INCLUDES) $< -o $(SRC_PATH)/$@

%.xpt: $(IDL_PATH)/%.idl
	mkdir -p $(BIN_PATH)
	$(XPIDL_XPT) $(GECKO_INCLUDES) $< -o $(BIN_PATH)/$@

%.o: $(SRC_PATH)/%.cpp Makefile
	mkdir -p $(OBJ_PATH)
	$(CXX) -c $(CPPFLAGS) $(OTHER_CFLAGS) $(CXXFLAGS) $(WEBRTC_INCLUDES) $(GECKO_CONFIG_INCLUDE) $(GECKO_DEFINES) $(GECKO_INCLUDES) $(LIB_INCLUDES) $< -o $(OBJ_PATH)/$@

$(MODULE).dylib: $(XPIDLSRCS:%.idl=%.h) $(XPIDLSRCS:%.idl=%.xpt) $(CPPSRCS:%.cpp=%.o)
	mkdir -p $(BIN_PATH)
	$(CXX) -o $(BIN_PATH)/$@ $(LDFLAGS) $(LDLIBFLAGS) $(CPPSRCS:%.cpp=$(OBJ_PATH)/%.o) $(GECKO_LDFLAGS)
	chmod +x $(BIN_PATH)/$@

clean: libclean
	rm -Rf $(wildcard $(BIN_PATH))
	rm -Rf $(wildcard $(OBJ_PATH))
	rm -Rf $(wildcard $(LIBBIN_PATH))