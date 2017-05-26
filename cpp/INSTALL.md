NDN-RTC: Real Time Conferencing Library for NDN
==

Prerequisites
==
These are prerequisites to build NDN-RTC.

**Required:**
* [NDN-CPP](https://github.com/named-data/ndn-cpp)
* [WebRTC branch-heads/59](https://code.google.com/p/webrtc/)
* [OpenFEC](http://openfec.org/downloads.html)
* [Boost](http://www.boost.org/users/download/)

**Optional (for ndnrtc-client app only)**
* [libconfig](http://www.hyperrealm.com/libconfig/)

# General
## NDN-RTC configure variables
NDN-RTC depends on its prerequisites. Paths to these sources and/or libraries can be set during configure phase. Use these variables for NDN-RTC configure script for providing custom paths to the prerequisites:
* **BOOSTDIR** - Path to the directory which contains Boost library headers folder (default is /usr/local/include)
* **NDNCPPDIR** - Path to the directory which contains NDN-CPP library headers folder (default is /usr/local/include)
* **NDNCPPLIB** - Path to the directory which contains NDN-CPP library binaries (default is /usr/local/lib)
* **OPENFECDIR** - Path to the directory which contains OpenFEC library
* **OPENFECSRC** - Path to the directory which contains OpenFEC library header files (default is $OPENFECDIR/src)
* **OPENFECLIB** - Path to the directory which contains OpenFEC library binaries (default is $OPENFECDIR/bin/Release)
* **WEBRTCDIR** - Path to the directory which contains WebRTC trunk
* **WEBRTCSRC** - Path to the directory which contains WebRTC header files (default is $WEBRTCDIR/wbertc)
* **WEBRTCLIB** - Path to the directory which contains WebRTC libraries (default is $WEBRTCDIR/out/Release)
* **LCONFIGDIR** - (Optional) path to the directory which contains libconfig library headers (default is /usr/local/include)
* **LCONFIGLIB** - (Optional) path to the directory which contains libconfig library binaries (default is /usr/local/lib)

### MacOS build considerations

NDN-RTC library is statically linked to its prerequisites. Therefore it's important to verify that all prerequisites which are C++ libraries were built using the same standard c++ library (either libc++ or libstdc++) - otherwise, linking stage will fail. This can be ensured by passing flag `-stdlib=libc++` or `-stdlib=libstdc++` to the compiler. Please, make sure that all prerequisites were built against the same version of standard library (especially WebRTC and NDN-CPP).

## Environment
Before building NDN-RTC, we suggest to create a folder for NDN-RTC environment where all prerequisites source code and NDN-RTC source code will be stored and compiled:

<pre>
$ mkdir ndnrtc-env && cd ndnrtc-env
</pre>

Future instructions assume everything is happening inside `ndnrtc-env` folder. Console code blocks at each step represent general approach taken to build dependencies and show flags used for configuration.

# Build instructions for OS X 10.10, 10.11, 10.12
## Prerequisites
### WebRTC
Here are the detailed instructions on [how to build WebRTC](http://www.webrtc.org/native-code/development).
Follow the instructions and build WebRTC (Release version).

### Boost
One can use `homebrew` to install boost:

<pre>
$ brew install boost
</pre>

### NDN-CPP

NDN-RTC uses Boost shared pointers. As NDN-RTC highly relies on NDN-CPP, types of shared pointers used in NDN-CPP and NDN-RTC should be the same.

In order to build NDN-CPP with boost shared pointers it's not enough to install them on the system, as NDN-CPP gives priority to the standard shared pointers.

<pre>
$ git clone https://github.com/named-data/ndn-cpp
$ cd ndn-cpp && mkdir -p build/share
$ ./configure --with-std-shared-ptr=no --with-std-function=no --prefix=$(pwd)/build
$ make && make install
</pre>

> Depending on your system configuration, you may need to add header and library search paths to your NDN-CPP configuration using `CFLAGS`, `CPPFLAGS`, `CXXFLAGS` and `LDFLAGS` (create [`config.site`](https://www.gnu.org/software/automake/manual/html_node/config_002esite.html) for that). 
> For macOS 10.12 (Sierra), `openssl` library is no longer a default, thus one need to provide paths, such as: `LDFLAGS="-L/usr/local/opt/openssl/lib"` and `CXXFLAGS="-I/usr/local/opt/openssl/include"`.

### OpenFEC
Before building OpenFEC, modify **src/CMakeLists.txt** file:

1. Change line `add_library(openfec SHARED ${openfec_sources})` to `add_library(openfec STATIC ${openfec_sources})`
2. Change line `target_link_libraries(openfec pthread IL)` to `target_link_libraries(openfec pthread)`
3. Add line `set(CMAKE_C_FLAGS "-fPIC")`

Check OpenFEC's README file for further build instructions.

>*If you experience errors while compiling OpenFEC library on MacOS, try using different compiler or compiler version. Here is the build process for MacOS 10.9.4 with gcc-4.3:*
<pre>
$ export CC=/opt/local/bin/gcc-mp-4.3
$ export CXX=/opt/local/bin/g++-mp-4.3
$ cmake ..
$ make
</pre>

### Libconfig (Optional)
Download it from [here](http://www.hyperrealm.com/libconfig/).

<pre>
$ cd libconfig
$ mkdir -p build/share
$ ./configure --prefix=$(pwd)/build
$ make && make install
</pre>

## NDN-RTC
<pre>
$ git clone https://github.com/remap/ndnrtc
$ cd ndnrtc/cpp
$ mkdir -p build/share
$ touch build/share/config.site
$ echo 'CPPFLAGS="-DWEBRTC_POSIX" CXXFLAGS="-DWEBRTC_POSIX"' >  build/share/config.site
$ echo NDNCPPDIR=`pwd`/../../ndn-cpp/build/include >> build/share/config.site
$ echo NDNCPPLIB=`pwd`/../../ndn-cpp/build/lib >> build/share/config.site
$ echo OPENFECDIR=`pwd`/../../openfec_v1.4.2 >> build/share/config.site
$ echo WEBRTCDIR=`pwd`/../../webrtc/src >> build/share/config.site
$ ./configure --prefix=$(pwd)/build
$ make && make install
</pre>

To run unit tests (compilation takes a while):

<pre>
$ make check
</pre>

# Build instructions for Ubuntu 12.04, 14.04, 15.04
These should be similar to macOS instructions. However it hasn't been tested yet. If you succesfully built NDN-RTC for Ubuntu, let me know your steps and I'll incorporate them on this page!

# Headless client
If you want to build headless client application, make sure you have succesfully installed [libcobfig](http://www.hyperrealm.com/libconfig/) library and run:

<pre>
$ make ndnrtc-client
</pre>
