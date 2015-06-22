NDN-RTC: Real Time Conferencing Library for NDN
==

Prerequisites
==
These are prerequisites to build NDN-RTC.

**Required:**
* [NDN-CPP v0.7](https://github.com/named-data/ndn-cpp)
* [WebRTC branch-heads/44](https://code.google.com/p/webrtc/)
* [OpenFEC](http://openfec.org/downloads.html)
* [Boost v1.54.0](http://www.boost.org/users/download/)
* pthread

**Optional (for demo app only)**
* [libconfig](http://www.hyperrealm.com/libconfig/)
* [ncurses](http://www.gnu.org/software/ncurses/)

Prerequisites build instructions
==

MacOS build considerations
--

NDN-RTC library is statically linked to its prerequisites. Therefore it's important to verify that all prerequisites which are C++ libraries were built using the same standard c++ library (either libc++ or libstdc++) - otherwise, linking stage will fail. This can be ensured by passing flag `-stdlib=libc++` or `-stdlib=libstdc++` to the compiler. Please, make sure that all prerequisites were built against the same version of standard library (especially WebRTC and NDN-CPP). WebRTC is usually built against libstdc++. In general it makes sense to pass `-stdlib=libstdc++` flag while building other prerequisites. In further instructions the preference is given to libstdc++ library.

NDN-CPP (v0.7)
--
NDN-RTC uses Boost shared pointers. As NDN-RTC highly relies on NDN-CPP, types of shared pointers used in NDN-CPP and NDN-RTC should be the same.

In order to build NDN-CPP with boost shared pointers it's not enough to install them on the system, as NDN-CPP gives priority to the standard shared pointers. Therefore, to build NDN-CPP static library using Boost and libstdc++, one should configure NDN-CPP like this:
<pre>
$ ./configure --with-std-shared-ptr=no --with-std-function=no CXXFLAGS="-stdlib=libstdc++ -I &lt;path_to_Boost_1.54.0_folder&gt;" --enable-shared=no BOOST_LDFLAGS="-L &lt;path_to_Boost_1.54.0._stage_lib_folder&gt;"
</pre>

WebRTC (branch-heads/44)
--
Here are the detailed instructions on [how to build WebRTC](http://www.webrtc.org/native-code/development).

By default, WebRTC is built for 32-bit architecture (event on 64-bit machines). To override this behavior run:
<pre>
$ export GYP_DEFINES="target_arch=x64"
$ ninja -C out/Release
</pre>

WebRTC's libraries will be places in **trunk/out/Release** folder.

OpenFEC
--
Before building OpenFEC, modify **src/CMakeLists.txt** file:

1. Change line `add_library(openfec SHARED ${openfec_sources})` to `add_library(openfec STATIC ${openfec_sources})`
2. Change line `target_link_libraries(openfec pthread IL)` to `target_link_libraries(openfec pthread)`

Check OpenFEC's README file for further build instructions.

>*If you expreience errors while compiling OpenFEC library on MacOS, try using different compiler or compiler version. Here is the build process for MacOS 10.9.4 with gcc-4.3:*
<pre>
$ export CC=/opt/local/bin/gcc-mp-4.3
$ export CXX=/opt/local/bin/g++-mp-4.3
$ cmake ..
$ make
</pre>

NDN-RTC build instructions
==
Use these variables for NDN-RTC configure script for providing custom paths to the prerequisites:
* **BOOSTDIR** - Path to the directory which contains Boost library headers folder (default is /usr/local/include)
* **NDNCPPDIR** - Path to the directory which contains NDN-CPP library headers folder (default is /usr/local/include)
* **NDNCPPLIB** - Path to the directory which contains NDN-CPP library binaries (default is /usr/local/lib)
* **OPENFECDIR** - Path to the directory which contains OpenFEC library
* **OPENFECSRC** - Path to the directory which contains OpenFEC library header files (default is $OPENFECDIR/src)
* **OPENFECLIB** - Path to the directory which contains OpenFEC library binaries (default is $OPENFECDIR/bin/Release)
* **PTHREADDIR** - Path to the directory which contains pthread library headers (default is /usr/local/include)
* **PTHREADLIB** - Path to the directory which contains pthread library binaries (default is /usr/local/lib)
* **WEBRTCDIR** - Path to the directory which contains WebRTC trunk
* **WEBRTCSRC** - Path to the directory which contains WebRTC header files (default is $WEBRTCDIR/wbertc)
* **WEBRTCLIB** - Path to the directory which contains WebRTC libraries (default is $WEBRTCDIR/out/Release)
* **LCONFIGDIR** - Path to the directory which contains libconfig library headers (default is /usr/local/include)
* **LCONFIGLIB** - Path to the directory which contains libconfig library binaries (default is /usr/local/lib)
* **NCURSESDIR** - Path to the directory which contains ncurses library headers (default is /usr/local/include)
* **NCURSESLIB** - Path to the directory which contains ncurses library binaries (default is /usr/local/lib)

Provide this flags while configuring:
* **CPPFLAGS="-DWEBRTC_POSIX"**

In case if prerequisites were built, but not installed, the build process for NDN-RTC may look like this:
<pre>
$ ./configure CPPFLAGS="-DWEBRTC_POSIX" WEBRTCDIR=&lt;path to WebRTC trunk folder> OPENFECDIR=&lt;path to OpenFEC folder> NDNCPPDIR=&lt;path to NDN-CPP checkout folder>/include NDNCPPLIB=&lt;path to NDN-CPP checkout folder>/.libs
$ make
$ sudo make install
</pre>


