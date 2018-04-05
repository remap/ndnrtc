# NDN-RTC

## Description
----
NDN-RTC C++ library allows one perform real-time low latency media streaming over [NDN](http://named-data.net/) networks. Library provides functions for creating audio and video streams for publishing over NDN, as well as fetching audio and video streams from remote publishers over NDN. More information about namespace and protocol design can be found in [NDN-RTC: Real-Time Videoconferencing over Named Data Networking](https://dl.acm.org/citation.cfm?id=2810176) paper.

Library depends on [WebRTC](https://webrtc.org) library and utilizes several classes from it:   
  - VP9 video encoder
        _NDN-RTC employs VP9 encoder for encoding/decoding individual video frames as they arrive._
  - Audio processing pipeline
        _NDN-RTC uses whole WebRTC audio processing pipeline which includes features like echo cancellation, gain control, noise reduction, etc. Since the whole pipeline has to be used, NDN-RTC operates as a transport for RTP audio-packets generated and processed by the pipeline. NDN-RTC, however, still provides buffering mechanism for these packets._

Library requires local [NFD forwarder](https://github.com/named-data/NFD) to be installed and configured.

## Structure

The directory structure is as follows:

* **/root**
    * **ccp/**
        * **src/** *-- source code*
        * **include/** *-- public headers*
        * **client/** *-- headless client application code*
        * **test/** *-- unit tests code*
        * **resources/** *-- unit tests' resources, helper scripts, etc.*  
    * **docs/** *-- relevant documentation*
    * **LICENSE**
    * **README.md**

## Building instructions

Please, see the [cpp/INSTALL.md](cpp/INSTALL.md) for build and install instructions.

## Headless client

As an example application, one can use headless client provided with the codebase. It is a console C++ application, which employs NDN-RTC library and can demonstrate publishing audio and video streams (video streams can be captured from files or file pipes only) as well as fetching those streams from NDN network. Please, refer to the [cpp/INSTALL.md](cpp/INSTALL.md#NDN-RTC) file for details on how to build it and [client/README.md](cpp/client/README.md) on how to use it.

## Headless client Docker image

Please, see the [docker/README.md](docker) for more information on `ndnrtc-client` Docker image.

## License

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version, with the additional exemption that compiling, linking, and/or using OpenSSL is allowed.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. If not, see http://www.gnu.org/licenses/. A copy of the GNU General Public License is in the file [cpp/COPYING](cpp/COPYING).
