noinst_LIBRARIES = libcocoa-renderer.a
libcocoa_renderer_a_SOURCES = cocoa-renderer.mm cocoa-renderer.h
libcocoa_renderer_a_OBJCFLAGS = -framework Cocoa#-framework OpenGL -framework CoreVideo -framework QTKit -framework CoreAudio -framework AudioToolbox
libcocoa_renderer_a_CPPFLAGS = -fPIC $(CPPFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/src -I@WEBRTCDIR@ -I@WEBRTCSRC@ -I@BOOSTDIR@ -I@NDNCPPDIR@ -I@OPENFECSRC@ -D BASE_FILE_NAME=\"$*\"

.mm.o :
	$(OBJCXX) -x objective-c++ -c @DEFS@ $(libcocoa_renderer_a_CPPFLAGS) $(libcocoa_renderer_a_OBJCFLAGS) $< -o $@
