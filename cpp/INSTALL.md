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
<details>
<summary>NDN-RTC configure variables</summary>
  
  > Paths to prerequisites sources and/or libraries can be set during configure phase. Use these variables for NDN-RTC _configure_ script for providing custom paths:
  > * **BOOSTDIR** - Path to the directory which contains Boost library headers folder (default is /usr/local/include)
  > * **NDNCPPDIR** - Path to the directory which contains NDN-CPP library headers folder (default is /usr/local/include)
  > * **NDNCPPLIB** - Path to the directory which contains NDN-CPP library binaries (default is /usr/local/lib)
  > * **OPENFECDIR** - Path to the directory which contains OpenFEC library
  > * **OPENFECSRC** - Path to the directory which contains OpenFEC library header files (default is $OPENFECDIR/src)
  > * **OPENFECLIB** - Path to the directory which contains OpenFEC library binaries (default is $OPENFECDIR/bin/Release)
  > * **WEBRTCDIR** - Path to the directory which contains WebRTC trunk
  > * **WEBRTCSRC** - Path to the directory which contains WebRTC header files (default is $WEBRTCDIR/webrtc)
  > * **WEBRTCLIB** - Path to the directory which contains WebRTC libraries (default is $WEBRTCDIR/out/Release)
  > * **LCONFIGDIR** - (Optional) path to the directory which contains libconfig library headers (default is /usr/local/include)
  > * **LCONFIGLIB** - (Optional) path to the directory which contains libconfig library binaries (default is /usr/local/lib)

</details>
<details>
  <summary>NDN-RTC environment:</summary>
   
   > Before building NDN-RTC, we suggest to create a folder for NDN-RTC environment where all prerequisites source code and NDN-RTC source code will be stored and compiled.
   > Future instructions assume everything is happening inside `ndnrtc-env` folder.
  
</details>

<pre>
$ mkdir ndnrtc-env && cd ndnrtc-env
</pre>


# Build instructions (macOS, Ubuntu)
## Prerequisites
<details>
  <summary>WebRTC</summary>
  
  > Here are detailed and latest instructions on [how to build WebRTC](http://www.webrtc.org/native-code/development).
  > Follow the instructions and build WebRTC **branch-heads/59** (Release version).

</details>

<pre>
$ mkdir webrtc-checkout && cd webrtc-checkout/
$ fetch --nohooks webrtc
$ cd src
$ git checkout -b branch-heads-59 refs/remotes/branch-heads/59
$ gclient sync
$ gn gen out/Default --args='is_debug=false'
$ ninja -C out/Default
</pre>
<details>
  <summary>// additional WebRTC step for macOS</summary>
  
  > Do this:
  <pre>
  $ mkdir -p out/Default/allibs && for lib in `find out/Default -name "*.a"`; do cp $lib out/Default/allibs/; done;
  </pre>
</details>


<details> 
  <summary> Boost </summary>
  
  > Using `homebrew` to install boost proved to be sufficient.
  
</details>

<pre>
$ brew install boost
</pre>

-- or --

<pre>
$ sudo apt-get install libboost-all-dev
</pre>

<details>
  <summary>NDN-CPP</summary>

  > NDN-RTC uses Boost shared pointers. As NDN-RTC highly relies on NDN-CPP, types of shared pointers used in NDN-CPP and NDN-RTC should be the same.
  > In order to build NDN-CPP with boost shared pointers it's not enough to install them on the system, as NDN-CPP gives priority to `std::shared_ptr` by default.
  
</details>

<pre>
$ git clone https://github.com/named-data/ndn-cpp
$ cd ndn-cpp && mkdir -p build/share
</pre>

<details>
  <summary>// additional step for macOS > 10.11</summary>
  
   > Depending on your system configuration, you may need to add header and library search paths to your NDN-CPP configuration using `CFLAGS`, `CXXFLAGS` and `LDFLAGS` (create [`config.site`](https://www.gnu.org/software/automake/manual/html_node/config_002esite.html) for that). 
   > For macOS 10.12 (Sierra), `openssl` library is no longer a default, thus one needs to provide paths, such as:
   >
   <pre>
   $ echo CFLAGS="-I/usr/local/opt/openssl/include" > build/share/config.site
   $ echo CXXFLAGS="-I/usr/local/opt/openssl/include" >> build/share/config.site
   $ echo LDFLAGS="-L/usr/local/opt/openssl/lib" >> build/share/config.site
   </pre>
</details>

<pre>
$ ./configure --with-std-shared-ptr=no --with-std-function=no --prefix=$(pwd)/build
$ make && make install
</pre>


<details>
<summary>OpenFEC</summary>
  
   > Before building OpenFEC, modify **src/CMakeLists.txt** file (stored in (ndnrtc-openfec.patch)[https://raw.githubusercontent.com/remap/ndnrtc/master/cpp/resources/ndnrtc-openfec.patch]):
   >
   > 1. Change line `add_library(openfec SHARED ${openfec_sources})` to `add_library(openfec STATIC ${openfec_sources})`
   > 2. Change line `target_link_libraries(openfec pthread IL)` to `target_link_libraries(openfec pthread)`
   > 3. Add line `set(CMAKE_C_FLAGS "-fPIC")`
   > Check OpenFEC's README file for further build instructions.
</details>

<pre>
$ wget http://openfec.org/files/openfec_v1_4_2.tgz
$ tar -xvf openfec_v1_4_2.tgz && rm openfec_v1_4_2.tgz
$ mkdir -p openfec_v1_4_2/build 
$ cd openfec_v1.4.2/
$ wget https://raw.githubusercontent.com/remap/ndnrtc/master/cpp/resources/ndnrtc-openfec.patch && patch src/CMakeLists.txt ndnrtc-openfec.patch
$ cd build_release/
$ cmake .. -DDEBUG:STRING=OFF
$ make
</pre>

<details>
<summary>Libconfig (optional)</summary>
  
  > Optional, needed by headless client app.
</details>

<pre>
$ git clone https://github.com/hyperrealm/libconfig.git
$ cd libconfig
$ mkdir -p build/share
$ autoreconf -i .
$ ./configure --prefix=$(pwd)/build
$ make && make install
</pre>

## NDN-RTC
<pre>
$ git clone https://github.com/remap/ndnrtc
$ cd ndnrtc/cpp && mkdir -p build/share
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
$ make test
</pre>

<details>
  <summary>Headless client</summary>

  > If you want to build headless client application, make sure you have succesfully installed **libconfig**.
</details>

<pre>
$ make ndnrtc-client
</pre>

# Headless client

*TBD: How to run and use*
