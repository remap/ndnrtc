NDN-RTC: Real Time Conferencing Library for NDN
==

Prerequisites
==
These are prerequisites to build NDN-RTC.

**Required:**
* [NDN-CPP](https://github.com/named-data/ndn-cpp)
* [WebRTC branch-heads/44](https://code.google.com/p/webrtc/)
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

NDN-RTC library is statically linked to its prerequisites. Therefore it's important to verify that all prerequisites which are C++ libraries were built using the same standard c++ library (either libc++ or libstdc++) - otherwise, linking stage will fail. This can be ensured by passing flag `-stdlib=libc++` or `-stdlib=libstdc++` to the compiler. Please, make sure that all prerequisites were built against the same version of standard library (especially WebRTC and NDN-CPP). WebRTC is usually built against libstdc++. In general it makes sense to pass `-stdlib=libstdc++` flag while building other prerequisites. In further instructions the preference is given to libstdc++ library.

## Environment
Before building NDN-RTC, we suggest to create a folder for NDN-RTC environment where all prerequisites source code and NDN-RTC source code will be stored and compiled:

<pre>
$ mkdir ndnrtc-env && cd ndnrtc-env
</pre>

Future instructions assume everything is happening inside this folder. Console code blocks at each step represent general approach taken to build dependencies and show flags given used for configuration.

# Build instructions for OS X 10.9, 10.10, 10.11
## Prerequisites
### WebRTC
Here are detailed instructions on [how to build WebRTC](http://www.webrtc.org/native-code/development).

<pre>
$ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
$ export PATH=`pwd`/depot_tools:"$PATH"
$ mkdir webrtc-checkout && cd webrtc-checkout
$ fetch webrtc
$ git checkout -b head44 refs/remotes/branch-heads/44
$ ninja -C out/Release
</pre>

### Boost
For OS X, only the following Boost library versions are currently compatible with NDN-RTC:
* [Boost 1.54.0](http://sourceforge.net/projects/boost/files/boost/1.54.0/)

<pre>
$ wget http://downloads.sourceforge.net/project/boost/boost/1.54.0/boost_1_54_0.tar.gz?r=http%3A%2F%2Fsourceforge.net%2Fprojects%2Fboost%2Ffiles%2Fboost%2F1.54.0%2F&ts=1444678642&use_mirror=superb-dca2 -O boost_1_54_0.tar.gz
$ tar -xvf boost_1_54_0.tar.gz && cd boost_1_54_0
$ ./bootstrap.sh
$ mkdir -p build && ./b2 cxxflags="-stdlib=libstdc++" linkflags="-stdlib=libstdc++" --prefix=$(pwd)/build
$ ./b2 install
</pre>

### NDN-CPP

NDN-RTC uses Boost shared pointers. As NDN-RTC highly relies on NDN-CPP, types of shared pointers used in NDN-CPP and NDN-RTC should be the same.

In order to build NDN-CPP with boost shared pointers it's not enough to install them on the system, as NDN-CPP gives priority to the standard shared pointers.

<pre>
$ git clone https://github.com/named-data/ndn-cpp
$ cd ndn-cpp && mkdir -p build/share
$ echo CXXFLAGS="-stdlib=libstdc++ -I$(pwd)/../boost_1_54_0/build/include" > build/share/config.site
$ echo LDFLAGS="-stdlib=libstdc++ -L$(pwd)/../boost_1_54_0/build/lib" >> build/share/config.site
$ ./configure --with-std-shared-ptr=no --with-std-function=no --with-boost=$(pwd)/../boost_1_54_0/build --prefix=$(pwd)/build
$ make && make install
</pre>

> **If you plan to use same NDN-CPP build for compiling *ndncon* later, you must have NDN-CPP compiled with dependency on Protobuf as it is used in ChronoSync2013 which is in turn used in [ConferenceDiscovery](https://github.com/zhehaowang/ConferenceDiscovery) library used by *ndncon*.** 
>
> In order to do that, download the [latest Protobuf](https://github.com/google/protobuf/releases) and compile it with `-stdlib=libstdc++` flag. Then, add headers and compiled libraries paths to NDN-CPP's `config.site` file:
<pre>
CXXFLAGS="... -I&lt;path_to_protobuf_headers_parent_folder&gt;"
LDFLAGS="... -L&lt;path_to_protobuf_libraries_folder&gt;"
</pre>
> After that, configure NDN-CPP and **make sure** it has successfully found protobuf headers and libraries during configuration. **Check configure output! Check config.log!** **This is important: NDN-CPP WILL NOT output error in case if Protobuf was not found as it is considered optional for the library.** So you **MUST** make sure that NDN-CPP has discovered it corectly. If it didn't, check your paths that you've added to `config.site` and try configuring NDN-CPP again. Repeat this until NDN-CPP configuration successfully detects Protobuf.

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

### Libconfig
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
$ echo BOOSTDIR=`pwd`/../../boost_1_54_0/build/include >> build/share/config.site
$ echo BOOSTLIB=`pwd`/../../boost_1_54_0/build/lib >> build/share/config.site
$ echo NDNCPPDIR=`pwd`/../../ndn-cpp/build/include >> build/share/config.site
$ echo NDNCPPLIB=`pwd`/../../ndn-cpp/build/lib >> build/share/config.site
$ echo OPENFECDIR=`pwd`/../../openfec_v1.4.2 >> build/share/config.site
$ echo WEBRTCDIR=`pwd`/../../webrtc/src >> build/share/config.site
$ echo LCONFIGDIR=`pwd`/../../libconfig/build/include >> build/share/config.site
$ echo LCONFIGLIB=`pwd`/../../libconfig/build/lib >> build/share/config.site
$ ./configure --prefix=$(pwd)/build
$ make && make install
</pre>

# Build instructions for Ubuntu 12.04, 14.04, 15.04
## Prerequisites
### WebRTC
Here are detailed instructions on [how to build WebRTC](http://www.webrtc.org/native-code/development).

<pre>
$ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
$ export PATH=`pwd`/depot_tools:"$PATH"
$ mkdir webrtc-checkout && cd webrtc-checkout
$ fetch webrtc
$ git checkout -b head44 refs/remotes/branch-heads/44
$ ninja -C out/Release
</pre>

### Boost
<pre>
$ sudo apt-get install libboost-all-dev
</pre>

### NDN-CPP
Full instructions [here](https://github.com/named-data/ndn-cpp/blob/master/INSTALL.md#ubuntu-1204-64-bit-and-32-bit-ubuntu-1404-64-bit-and-32-bit-ubuntu-1504-64-bit)

<pre>
$ git clone https://github.com/named-data/ndn-cpp
$ cd ndn-cpp && mkdir -p build
$ ./configure --with-std-shared-ptr=no --with-std-function=no --prefix=$(pwd)/build
$ make && make install
</pre>

### OpenFEC
Before building OpenFEC, modify **src/CMakeLists.txt** file:

1. Change line `add_library(openfec SHARED ${openfec_sources})` to `add_library(openfec STATIC ${openfec_sources})`
2. Change line `target_link_libraries(openfec pthread IL)` to `target_link_libraries(openfec pthread)`
3. Add line `set(CMAKE_C_FLAGS "-fPIC")`

Check OpenFEC's README file for further build instructions.

<pre>
$ cd openfec/src
$ cmake ..
$ make
</pre>

### Libconfig
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
$ echo 'CPPFLAGS="-DWEBRTC_POSIX -DBOOST_ASIO_DISABLE_STD_CHRONO" CXXFLAGS="-DWEBRTC_POSIX -DBOOST_ASIO_DISABLE_STD_CHRONO"' >  build/share/config.site
$ echo NDNCPPDIR=`pwd`/../../ndn-cpp/build/include >> build/share/config.site
$ echo NDNCPPLIB=`pwd`/../../ndn-cpp/build/lib >> build/share/config.site
$ echo OPENFECDIR=`pwd`/../../openfec >> build/share/config.site
$ echo WEBRTCDIR=`pwd`/../../webrtc/src >> build/share/config.site
$ echo LCONFIGDIR=`pwd`/../../libconfig/build/include >> build/share/config.site
$ echo LCONFIGLIB=`pwd`/../../libconfig/build/lib >> build/share/config.site
$ ./configure --prefix=$(pwd)/build
$ make && make install
</pre>

# Headless client
If you want to build headless client application, make sure you have succesfully installed [libcobfig](http://www.hyperrealm.com/libconfig/) library and run:

<pre>
$ make ndnrtc-client
</pre>
