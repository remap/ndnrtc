# Headless client

Headless client is a C++ console application which demonstrates use of NDN-RTC library and allows one to publish and fetch audio and video streams over NDN network. Since application is console and cross-platform, it does not provide capabilities to capture video frames from connected hardware or software cameras. Instead, it can read and write raw ARGB frames from/to three types of endpoints:

- file;
- file pipe;
- [nanomsg](http://nanomsg.org/) unix socket.

 For audio, headless app acquires default audio recording device in the system and it is not configurable (in other words, if there are two audio recording devices, it'll get whatever is set as default in OS).
 
## Config file
Configuration of the app is done via config file. [Here](../tests/default.cfg) is an example of a configuration file with comments, explaining different sections and parameters.
 
 There are **3 sections** in config file: *general*, *producer* and *consumer*. Config file **MUST** have at least two sections - *general* and (*consumer* or *producer*) for consumer OR producer behaviour; but also **MAY** have all three sections for both consumer AND producer behaviour.
 
### General
This section describes general parameters such as logging level, log file name, log file folder, NDN network configuration (one may want to connect to remote NFD, for example) and others (see [example config](../tests/default.cfg) for more/latest information):

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

One can also configure real-time statistics gathering through the optional `stat_gathering` sub-subsection. Each entry in `stat_gathering` array will result in creating `.stat` CSV file for every fetched stream (specified later in `streams` section) with specified statistics. Statistics keywords and their descriptions can be found in [statistics.hpp](../include/statistics.hpp) and [statistics.cpp](../src/statistics.cpp#L180) source files.

`streams` subsection specifies which stream will application attempt to fetch from the network. Each entry describes type of stream, base prefix (in other words, producer's prefix supplied when application was launched), stream name and thread to fetch. For video streams, one may store received raw ARGB frames into a file, specified by `sink`. Alternatively, raw frames can be dumped into a file pipe or nanomsg socket by specifying `sink_type` parameter.

<details>
 <summary><i>Expand to see example consumer configuration</i></summary>
  
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

## Command-line arguments

In order to launch headless app, several command-line arguments must be provided: 

- `-c` (*config file*) -- [config file](#config-file) describing app configuration;
- `-s` (*signing identity*) -- NDN signing identity which will be used to create application certificate and sign packets; this should be something, like `/ndn/edu/ucla/remap/peter` or whatever NDN identity you have [installed](http://named-data.net/doc/NFD/current/INSTALL.html#nfd-security) in your system (unfortunately, even if app acts as a consumer-only, this parameter is mandatory);
- `-p` (*verification policy file*) -- [verification policy file](https://named-data.net/doc/ndn-cxx/current/tutorials/security-validator-config.html) for verifying incoming packets (unfortunately, even if app acts as a producer-only, this parameter is mandatory);
- `-t` (*application run time*) -- application run time in seconds;
- `-i` (*application instance name*) -- application instance name which will be appended to provided *singning identity* in order to generate application certificate;
- `-n` (*statistics sampling interval*) -- statistics sampling period in milliseconds (**optional**, default is 100ms);
- `-v` (*verbose mode*) -- verbose output for std::out (not for log file specified in config file).

## Simple example
> To run simple example with headless client, one will need to have [NFD installed and configured](http://named-data.net/doc/NFD/current/INSTALL.html).

Here, I'll explain how to run a simple producer-consumer setup (on two machines preferrrably) using headless client app. Producer will read raw video frames from file and acquire audio from default audio input device on the system. These two streams will be fetched by consumer - video saved into a file and audio played back on default audio output device.

> Following console commands should be ran statnding in `ndndrtc/cpp` folder (after running `make ndnrtc-client` command).

### Producer setup

<details>
 <summary>#1 <b>Create producer identity and store certificate into a file</b> <i>(expand for more info)</i></summary>
 
 > Producer's certificate will be needed by consumer for verifying data later.
 
</details>

```Shell
ndnsec-keygen /ndnrtc-test | ndnsec-install-cert -
ndnsec-dump-certificate -i /ndnrtc-test > tests/policy_config/signing.cert
```

<details>
 <summary>#2 <b>Run producer</b> <i>(expand for more info)</i></summary>
 
 > Last argument `-t` specifies runtime in seconds. Producer will read frames from test sequence in a loop.
 
</details>

```Shell
nfd-start
./ndnrtc-client -c tests/sample-producer.cfg -s /ndnrtc-test -p tests/policy_config/rule.conf -i instance1 -t 100
```

Producer will generate two log files:

- `/tmp/producer-instance1-camera.log` - for video stream;
- `/tmp/producer-instance1-sound.log` - for audio stream.

### Consumer setup

<details>
 <summary>#3 <b>Copy `signing.cert` saved in step #1 to consumer's machine</b></summary>
 
 > Nothing's here :neckbeard:
</details>

 ```Shell
 cp <producer-ndn-rtc-cpp-folder>/tests/policy_config/signing.cert <consumer-ndn-rtc-cpp-folder>/tests/policy_config
 ```
<details>
 <summary>#4 <b>Run consumer</b></summary>
 
 > Nothing's here :expressionless:
</details>
 
```Shell
./ndnrtc-client -c tests/sample-consumer.cfg -s /ndnrtc-test -p tests/policy_config/rule.conf -i instance2 -t 50 -v
```
 
Consumer will generate multiple files:

- `/tmp/consumer-ndnrtc-test-instance1-camera.log` - for video stream fetching;
- `/tmp/consumer-ndnrtc-test-instance1-sound.log` - for audio stream fetching;
- `/tmp/instance1-camera.320x240` - fetched ARGB frames frames from producer;
- video stream statistics (see [sample-consumer.cfg](../tests/sample-consumer.cfg#L25) for details which statistics are wirtten):
  - `/tmp/buffer-ndnrtc-test-instance1-camera.stat`
  - `/tmp/play-ndnrtc-test-instance1-camera.stat`
  - `/tmp/playback-ndnrtc-test-instance1-camera.stat`
- audio stream statistics (see [sample-consumer.cfg](../tests/sample-consumer.cfg#L25) for details which statistics are wirtten):
  - `/tmp/buffer-ndnrtc-test-instance1-sound.stat`
  - `/tmp/play-ndnrtc-test-instance1-sound.stat`
  - `/tmp/playback-ndnrtc-test-instance1-sound.stat`
  
**To convert video into viewable format, use `ffmpeg`:**
```Shell
ffmpeg -f rawvideo -vcodec rawvideo -s 320x240 -r 25 -pix_fmt argb -i /tmp/instance1-camera.320x240 -c:v libx264 -preset ultrafast -qp 0 instance1-camera.mp4
```
