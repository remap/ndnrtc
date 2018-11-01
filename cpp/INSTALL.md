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

# Build instructions (macOS, Ubuntu)
## General
<details>
<summary>NDN-RTC configure variables <i>(expand for more info)</i></summary>
  
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

> Before building NDN-RTC, we suggest to create a folder for NDN-RTC environment where all prerequisites source code and NDN-RTC source code will be stored and compiled.

<details>
  <summary>#0 <b>NDN-RTC environment </b><i>(expand for more info)</i></summary>
  
  > Eventually, your `ndnrtc-env` should look like this:
  > `ndnrtc-env/`
  >
  > - &emsp; `depot-tools/` 
  > - &emsp; `libconfig/` 
  > - &emsp; `ndn-cpp/` 
  > - &emsp; `ndnrtc/` 
  > - &emsp; `ndnrtc/` 
  > - &emsp; `openfec_v1.4.2/` 
  > - &emsp; `webrtc-checkout/` 
</details>

```Shell
mkdir ndnrtc-env && cd ndnrtc-env
export NDNRTC_ENV=`pwd`
```

<details>
  <summary>#0.5 <b>Out-of-the box prerequisites</b></summary>
  
  > These are required prerequisites which can be installed using [`homebrew`](https://brew.sh/) (macOS) or `apt-get` (Ubuntu).
  
</details>

```Shell
brew install boost cmake wget autoconf automake libtool openssl libconfig nanomsg rocksdb
```

-- or (for Ubuntu) --

```Shell
sudo apt-get install libboost-all-dev cmake wget autoconf automake libtool git protobuf-compiler libconfig++-dev libconfig++9v5
```

## Compiled prerequisites
> Don't forget to complete [**Before you start**](https://webrtc.org/native-code/development/prerequisite-sw/) step for installing WebRTC prerequisites. Last time checked, [`depot-tools`](http://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up) needed to be installed first:
> ```sh
> cd $NDNRTC_ENV
> git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
> export PATH=$PATH:`pwd`/depot_tools
> ```

<details>
  <summary>#1 <b>WebRTC </b><i>(expand for more info)</i></summary>
  
  > Here are detailed and latest instructions on [how to build WebRTC](http://www.webrtc.org/native-code/development).
  > Follow the instructions and build WebRTC **branch-heads/59** (Release version).

</details>

```Shell
cd $NDNRTC_ENV
mkdir webrtc-checkout && cd webrtc-checkout/
fetch --nohooks webrtc
cd src
git checkout -b branch-heads-59 refs/remotes/branch-heads/59
gclient sync
```

<details>
  <summary>&emsp;<i>// additional step for Ubuntu (expand for more info)</i></summary>
  
  > As part of installing prerequisites for WebRTC:
  
  ```Shell
  ./build/install-build-deps.sh
  ```
  
</details>

<details>
  <summary>&emsp;<i><b>WebRTC </b>(continuation)</i></summary>
  
  > Compilation may take some time
  
</details>

```Shell
gn gen out/Default --args='is_debug=false'
ninja -C out/Default
```

<details>
  <summary>&emsp;<i>// additional step for macOS (expand for more info)</i></summary>
  
  > Do this:
  ```Shell
  mkdir -p out/Default/allibs && for lib in `find out/Default -name "*.a"`; do cp $lib out/Default/allibs/; done;
  ```
</details>

<details>
  <summary>#2 <b>NDN-CPP </b><i>(expand for more info)</i></summary>

  > NDN-RTC uses Boost shared pointers. As NDN-RTC highly relies on NDN-CPP, types of shared pointers used in NDN-CPP and NDN-RTC should be the same.
  > In order to build NDN-CPP with boost shared pointers it's not enough to install them on the system, as NDN-CPP gives priority to `std::shared_ptr` by default.
  
</details>

```Shell
cd $NDNRTC_ENV
git clone https://github.com/named-data/ndn-cpp
cd ndn-cpp && mkdir -p build/share
```

<details>
  <summary>&emsp;<i>// additional step for macOS >= 10.11 (expand for more info)</i></summary>
  
   > Depending on your system configuration, you may need to add header and library search paths to your NDN-CPP configuration using `ADD_CFLAGS`, `ADD_CXXFLAGS` and `ADD_LDFLAGS` (create [`config.site`](https://www.gnu.org/software/automake/manual/html_node/config_002esite.html) for that). 
   > For macOS 10.11 (El Capitan), `openssl` library is no longer a default, thus one needs to provide paths, such as:
   >
   ```Shell
   echo ADD_CFLAGS="-I/usr/local/opt/openssl/include" > build/share/config.site
   echo ADD_CXXFLAGS="-I/usr/local/opt/openssl/include" >> build/share/config.site
   echo ADD_LDFLAGS="-L/usr/local/opt/openssl/lib" >> build/share/config.site
   ```
</details>

<details>
  <summary>&emsp;<i><b>NDN-CPP</b> (continuation)</i></summary>
  
  > Nothing's here :grin:
</details>

```Shell
./configure --with-std-shared-ptr=no --with-std-function=no --prefix=$(pwd)/build
make && make install
```

<details>
  <summary>#3 <b>OpenFEC </b><i>(expand for more info)</i></summary>
  
   > To build OpenFEC, few edits need to be made for **src/CMakeLists.txt** file (applied as [ndnrtc-openfec.patch](https://raw.githubusercontent.com/remap/ndnrtc/master/cpp/resources/ndnrtc-openfec.patch) in instructions below):
   >
   > 1. Change line `add_library(openfec SHARED ${openfec_sources})` to `add_library(openfec STATIC ${openfec_sources})`
   > 2. Change line `target_link_libraries(openfec pthread IL)` to `target_link_libraries(openfec pthread)`
   > 3. Add line `set(CMAKE_C_FLAGS "-fPIC")`
   
</details>

```Shell
cd $NDNRTC_ENV
wget http://openfec.org/files/openfec_v1_4_2.tgz
tar -xvf openfec_v1_4_2.tgz && rm openfec_v1_4_2.tgz
mkdir -p openfec_v1.4.2/build && cd openfec_v1.4.2/
wget https://raw.githubusercontent.com/remap/ndnrtc/master/cpp/resources/ndnrtc-openfec.patch && patch src/CMakeLists.txt ndnrtc-openfec.patch
cd build/
cmake .. -DDEBUG:STRING=OFF
make
```

## NDN-RTC
```Shell
cd $NDNRTC_ENV
git clone --recursive https://github.com/remap/ndnrtc
cd ndnrtc/cpp && mkdir -p build/share
echo 'CPPFLAGS="-g -O2 -DWEBRTC_POSIX" CXXFLAGS="-g -O2 -DWEBRTC_POSIX"' >  build/share/config.site
echo NDNCPPDIR=`pwd`/../../ndn-cpp/build/include >> build/share/config.site
echo NDNCPPLIB=`pwd`/../../ndn-cpp/build/lib >> build/share/config.site
echo OPENFECDIR=`pwd`/../../openfec_v1.4.2 >> build/share/config.site
echo WEBRTCDIR=`pwd`/../../webrtc-checkout/src >> build/share/config.site
./configure --prefix=$(pwd)/build
make && make install
```

<details>
  <summary><b>Headless client </b><i>(expand for more info)</i></summary>

  > If you want to build headless client application, make sure you have succesfully installed **libconfig**.
</details>

```Shell
make ndnrtc-client
```
