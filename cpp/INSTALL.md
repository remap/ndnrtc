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

<pre>
$ mkdir ndnrtc-env && cd ndnrtc-env
$ export NDNRTC_ENV=`pwd`
</pre>

## Prerequisites
> Don't forget to complete [**Before you start**](https://webrtc.org/native-code/development/prerequisite-sw/) step for installing WebRTC prerequisites. Last time checked, [`depot-tools`](http://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up) needed to be installed first:
> <pre>
> $ cd $NDNRTC_ENV
> $ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
> $ export PATH=$PATH:`pwd`/depot_tools
> </pre>

<details>
  <summary>#1 <b>WebRTC </b><i>(expand for more info)</i></summary>
  
  > Here are detailed and latest instructions on [how to build WebRTC](http://www.webrtc.org/native-code/development).
  > Follow the instructions and build WebRTC **branch-heads/59** (Release version).

</details>

<pre>
$ cd $NDNRTC_ENV
$ mkdir webrtc-checkout && cd webrtc-checkout/
$ fetch --nohooks webrtc
$ cd src
$ git checkout -b branch-heads-59 refs/remotes/branch-heads/59
$ gclient sync
</pre>

<details>
  <summary>&emsp;<i>// additional step for Ubuntu (expand for more info)</i></summary>
  
  > As part of installing prerequisites for WebRTC:
  
  <pre>
  $ ./build/install-build-deps.sh
  </pre>
  
</details>

<details>
  <summary>&emsp;<i><b>WebRTC </b>(continuation)</i></summary>
  
  > Compilation may take some time
  
</details>

<pre>
$ gn gen out/Default --args='is_debug=false'
$ ninja -C out/Default
</pre>

<details>
  <summary>&emsp;<i>// additional step for macOS (expand for more info)</i></summary>
  
  > Do this:
  <pre>
  $ mkdir -p out/Default/allibs && for lib in `find out/Default -name "*.a"`; do cp $lib out/Default/allibs/; done;
  </pre>
</details>


<details> 
  <summary>#2 <b>Boost </b><i>(expand for more info)</i></summary>
  
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
  <summary>#3 <b>NDN-CPP </b><i>(expand for more info)</i></summary>

  > NDN-RTC uses Boost shared pointers. As NDN-RTC highly relies on NDN-CPP, types of shared pointers used in NDN-CPP and NDN-RTC should be the same.
  > In order to build NDN-CPP with boost shared pointers it's not enough to install them on the system, as NDN-CPP gives priority to `std::shared_ptr` by default.
  
</details>

<pre>
$ cd $NDNRTC_ENV
$ git clone https://github.com/named-data/ndn-cpp
$ cd ndn-cpp && mkdir -p build/share
</pre>

<details>
  <summary>&emsp;<i>// additional step for macOS > 10.11 (expand for more info)</i></summary>
  
   > Depending on your system configuration, you may need to add header and library search paths to your NDN-CPP configuration using `CFLAGS`, `CXXFLAGS` and `LDFLAGS` (create [`config.site`](https://www.gnu.org/software/automake/manual/html_node/config_002esite.html) for that). 
   > For macOS 10.12 (Sierra), `openssl` library is no longer a default, thus one needs to provide paths, such as:
   >
   <pre>
   $ echo CFLAGS="-I/usr/local/opt/openssl/include" > build/share/config.site
   $ echo CXXFLAGS="-I/usr/local/opt/openssl/include" >> build/share/config.site
   $ echo LDFLAGS="-L/usr/local/opt/openssl/lib" >> build/share/config.site
   </pre>
</details>

<details>
  <summary>&emsp;<i><b>NDN-CPP</b> (continuation)</i></summary>
  
  > Nothing's here :grin:
</details>

<pre>
$ ./configure --with-std-shared-ptr=no --with-std-function=no --prefix=$(pwd)/build
$ make && make install
</pre>

<details>
  <summary>#3.5 <b>OpenFEC prerequisites</b></summary>
    
   > Nothing's here :bowtie:
</details>

> One must have `cmake` in order to build OpenFEC:
<pre>
$ brew install cmake
</pre>

-- or --
<pre>
$ sudo apt-get install cmake
</pre>

<details>
  <summary>#4 <b>OpenFEC </b><i>(expand for more info)</i></summary>
  
   > To build OpenFEC, few edits need to be made for **src/CMakeLists.txt** file (applied as [ndnrtc-openfec.patch](https://raw.githubusercontent.com/remap/ndnrtc/master/cpp/resources/ndnrtc-openfec.patch) in instructions below):
   >
   > 1. Change line `add_library(openfec SHARED ${openfec_sources})` to `add_library(openfec STATIC ${openfec_sources})`
   > 2. Change line `target_link_libraries(openfec pthread IL)` to `target_link_libraries(openfec pthread)`
   > 3. Add line `set(CMAKE_C_FLAGS "-fPIC")`
   
</details>

<pre>
$ cd $NDNRTC_ENV
$ wget http://openfec.org/files/openfec_v1_4_2.tgz
$ tar -xvf openfec_v1_4_2.tgz && rm openfec_v1_4_2.tgz
$ mkdir -p openfec_v1.4.2/build && cd openfec_v1.4.2/
$ wget https://raw.githubusercontent.com/remap/ndnrtc/master/cpp/resources/ndnrtc-openfec.patch && patch src/CMakeLists.txt ndnrtc-openfec.patch
$ cd build/
$ cmake .. -DDEBUG:STRING=OFF
$ make
</pre>

<details>
  <summary>#5 <b>Libconfig </b><i>(expand for more info)</i></summary>
  
  > Optional, needed by **headless client app**.
</details>

<pre>
$ cd $NDNRTC_ENV
$ git clone https://github.com/hyperrealm/libconfig.git
$ cd libconfig
$ mkdir -p build/share
$ autoreconf -i .
$ ./configure --prefix=$(pwd)/build
$ make && make install
</pre>

## NDN-RTC
<pre>
$ cd $NDNRTC_ENV
$ git clone https://github.com/remap/ndnrtc
$ cd ndnrtc/cpp && mkdir -p build/share
$ touch build/share/config.site
$ echo 'CPPFLAGS="-DWEBRTC_POSIX" CXXFLAGS="-DWEBRTC_POSIX"' >  build/share/config.site
$ echo NDNCPPDIR=`pwd`/../../ndn-cpp/build/include >> build/share/config.site
$ echo NDNCPPLIB=`pwd`/../../ndn-cpp/build/lib >> build/share/config.site
$ echo OPENFECDIR=`pwd`/../../openfec_v1.4.2 >> build/share/config.site
$ echo WEBRTCDIR=`pwd`/../../webrtc-checkout/src >> build/share/config.site
$ ./configure --prefix=$(pwd)/build
$ make && make install
</pre>

To run unit tests (compilation takes a while):

<pre>
$ make test
</pre>

<details>
  <summary><b>Headless client </b><i>(expand for more info)</i></summary>

  > If you want to build headless client application, make sure you have succesfully installed **libconfig**.
</details>

<pre>
$ make ndnrtc-client
</pre>


# Headless client

Headless client is a C++ console application which demonstrates use of NDN-RTC library and allows one to publish and fetch audio and video streams over NDN network. Since application is console and cross-platform, it does not provide capabilities to capture video frames from connected hardware or software cameras. Instead, it can read and write raw ARGB frames from/to three types of endpoints:

- file;
- file pipe;
- [nanomsg](http://nanomsg.org/) unix socket.

 For audio, headless app acquires default audio recording device in the system and it is not configurable (in other words, if there are two audio recording devices, it'll get whatever is set as default in OS).
 
## Config file
Configuration of the app is done via config file. [Here](tests/default.cfg) is an example of a configuration file with comments, explaining different sections and parameters.
 
 There are **3 sections** in config file: *general*, *producer* and *consumer*. Config file **MUST** have at least two sections - *general* and (*consumer* or *producer*) for consumer OR producer behaviour; but also **MAY** have all three sections for both consumer AND producer behaviour.
 
 ### General
This section describes general parameters such as logging level, log file name, log file folder, NDN network configuration (one may want to connect to remote NFD, for example) and others (see [example config](tests/default.cfg) for more/latest information):

<details>
  <summary><i>Expand to see example general configuration</i></summary>

    general = {
      log_level = "default";  // all, debug, stat, default, none
      log_file = "ndnrtc-client.log";
      log_path = "/tmp";
      
      use_fec = true; // [true | false] -- use Forward Error Correction
      use_avsync = true; // [true | false] -- TBD: enable synchronization between audio/video streams
      
      ndnnetwork ={
        connect_host = "localhost";
        connect_port = 6363;
      };
    };
</pre>
</details>
 
### Producer
This section specifies producer behaviour of headless app. It is structured as an array (`streams`) of media stream configurations. Each stream configuration contains details about its' type, name (will be used as part of NDN name), segmenting, data freshness and source (for video streams) from where application will read raw frames. Besides this basic details, one MUST specify at least 1 **media encoding thread** with encoding parameters. By having multiple media encoding threads, one can simultaneously encode incoming stream into different bitrates and publish them over NDN (each thread gets its' own name) to be fetched by consumers. Unlike video, audio streams can't have multiple encoding threads currently.
 
<details>
  <summary><i>Expand to see example producer configuration</i></summary>

    produce = {
      streams = ({ // video stream configuration
        type = "video";             // [video | audio] 
        name = "camera";            // video stream name
        segment_size = 1000;        // in bytes
        freshness = 2000;           // in milliseconds
        source = "camera.argb";     // file from where raw frames will 
                                    // be read. frame resolution should be
                                    // equal to the maximum encoding resolution 
                                    // among threads
        sync = "sound";             // name of the audio stream to sync this video stream to
    
        threads = ({    // an array of stream's threads that will be published
          name = "low";                       // thread name
          coder = {                           // encoder parameters
            frame_rate = 30;
            gop = 30;                       //group of picture
            start_bitrate = 1000;
            max_bitrate = 10000;
            encode_height = 405;
            encode_width = 720;
            drop_frames = true;     // whether encoder should drop frames
                                    // to maintain start bitrate
          };
        },
        {
          name = "hi";
          coder = {
            frame_rate = 30;
            gop = 30;
            start_bitrate = 3000;
            max_bitrate = 10000;
            encode_height = 1080;
            encode_width = 1920;
            drop_frames = true;
          };
        });
      },
      {  // audio stream configuration
        type = "audio";
        name = "sound";
        thread = "pcmu";
        segment_size = 1000;
        freshness = 2000;           // in milliseconds
        codec = "g722";
        capture_device = 0;
      });
    };

</details>
 
### Consumer
This section of config file specifies parameters for fetching remote media streams. `basic` subsection is used for configuring interest lifetimes and jitter buffer sizes for audio and video streams. 

One can also configure real-time statistics gathering through the optional `stat_gathering` sub-subsection. Each entry in `stat_gathering` array will result in creating `.stat` CSV file for every fetched stream (specified later in `streams` section) with specified statistics. Statistics keywords and their descriptions can be found in [statistics.hpp](include/statistics.hpp) and [statistics.cpp](src/statistics.cpp#L180) source files.

`streams` subsection specifies which stream will application attempt to fetch from the network. Each entry describes type of stream, base prefix (in other words, producer's prefix supplied when application was launched), stream name and thread to fetch. For video streams, one may store received raw ARGB frames into a file, specified by `sink`. Alternatively, raw frames can be dumped into a file pipe or nanomsg socket by specifying `sink_type` parameter.

<details>
  <summary>Expand to see example consumer configuration</summary>
  
    consume = {
      basic = {
         audio = {
           interest_lifetime = 2000; // in millisecond
           jitter_size = 150; // minimal jitter buffer size in milliseconds
         };
         video = {
           interest_lifetime = 2000;
           jitter_size = 150;
         };
         // statistics to gather per stream
         // allowed statistics keywords can be found in statistics.h
         stat_gathering = ({
           name="buffer";  // file name prefix (complete filename will be 
                           // <name>-<producer_name>-<stream_name>.stat)
           statistics= ("jitterPlay", "jitterTar", "dArr"); 
         },
         {
           name="playback";
           statistics= ("framesAcq","lambdaD","drdPrime");
         },
         {
           name="play";
           statistics= ("lambdaD","drdPrime","jitterTar","dArr");
         });
      };
      streams = ({
        type = "video";             // [video | audio] 
        base_prefix = "/ndn/edu/ucla/remap/clientB";
        name = "camera";            // video stream name
        thread_to_fetch = "low";    // exact stream thread to fetch from
                                    // should be the name of one thread in this stream
        sink = "clientB-camera";    // file path where raw decoded frames 
                                    // will be stored (without extension)
                                    // full filename will be:
                                    //      "<sink>-<idx>-<W>x<H>.argb"
                                    // idx is required, as during fetching, 
                                    // consumer may receive different frame 
                                    // resolutions (due to ARC switching between
                                    // differen threads)
        sink_type = "file";         // "file", "pipe", "nano". if ommited - "file" by default
      },
      {
        type = "video";
        base_prefix = "/ndn/edu/ucla/remap/clientC";
        name = "camera";
        thread_to_fetch = "mid";
        sink = "clientC-camera";
        sink_type = "file";
      },
      {
        type = "audio";
        base_prefix = "/ndn/edu/ucla/remap/clientB";
        name = "sound";
        thread_to_fetch = "pcmu";
      },
      {
        type = "audio";
        base_prefix = "/ndn/edu/ucla/remap/clientC";
        name = "sound";
        thread_to_fetch = "pcmu";
      });
    };
</details>
