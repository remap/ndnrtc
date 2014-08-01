Ndnrtc-demo
===
Description
---
Ndnrtc-demo is a simple console application which loads NDN-RTC library and provides functionality for establishing many-to-many audiovisual conferences over NDN networks. It also provides extensive logging system and statistics output which can be used for testing and anlysis. 

Demo app configures NDN-RTC library using parameters loaded from configuration file (ndnrtc.cfg by default). Configuration file is a simple text file (as defined by [libconfig](http://www.hyperrealm.com/libconfig/libconfig_manual.html#Configuration-Files)) with several sections. [Default configuration file](../resources/ndnrtc.cfg) shipped with this code has plenty of comments and is self-explanatory.

Build prerequisites
---

Currently, Ndnrtc-demo is the **MacOS X only** application

**Required**
* [NDN-RTC](../INSTALL.md)
* [libconfig](http://www.hyperrealm.com/libconfig/)
* [ncurses](http://www.gnu.org/software/ncurses/)
* pthread

How to build
---
1. [Build](../INSTALL.md) NDN-RTC library
2. In **cpp** folder run:
<pre>
$ make ndnrtc-demo
</pre>

How to use
--

NOTE: *In case of using ndnd-tlv remember to [configure](http://redmine.named-data.net/projects/ndnd-tlv/wiki/Configuring_ndnd-tlv) it beforehand.*

Demo application can take two command-line arguments - a path to the NDN-RTC library binary and a path to configuration file. If nothing is given, the app assumes it was given *./libndnrtc.dylib* for the library path and *./ndnrtc.cfg* for the configuration file.

Demo app folder provides convenient script to run demo application. In order to start demo application:
<pre>
$ cd cpp/demoapp
$ ./rundemo.sh
</pre>

Script will start Ndnrtc-demo application with pathes to the built library and sample configuration file in **resources** folder.

Some useful tips for using demo app
---

In order to:
* **start publishing** choose option *"1 start media publishing"* or press *1*. After that, the app will ask for **user name** and **prefix** under which you would like to publish media. NOTE: *if you would like to run separate consumer later on, make sure you chose prefix which is routable from your machine*.

>Depending on the configuration file demo app may start publishing media streams under the following prefixes:
>    - **&lt;prefix>**/ndnrtc/user/**&lt;username>**/streams/audio0/**&lt;bitrate>**/frames/delta - for audio samples
>    - **&lt;prefix>**/ndnrtc/user/**&lt;username>**/streams/video0/**&lt;bitrate>**/frames/delta - for video DELTA frames
>    - **&lt;prefix>**/ndnrtc/user/**&lt;username>**/streams/video0/**&lt;bitrate>**/frames/key - for video KEY frames


* **fetch media stream** choose option *"3 fetch stream"* or press *3*. After that, specify username and prefix. Depending on configuration, media playback should start shortly.
* **stop fetching** choose option *"4 stop fetching stream"* or press *4*. After that, specify username from which you want to stop fetching media.
* **run loopback test** choose option *"6 loopback mode"* or press "*6*". App will start publishing media under username "loopback" and default prefix. Also, app will start fetching this data.
* **see runtime statistics** choose option *"7 show statistics"* or press "*7*". App will open screen with statistics for current media publisher/consumers. To navigate between several consumers use arrows.
* **bring all current windows to front** chose option *8 arrange all windows*. All current rendering windows will be brought to the front. 

Feedback
--

While running, demo app is logging events in several log files (depending on use case). For publishing, logs are stored in *"producer-<username>.log"* files, for fetching - in *"consumer-<username>.log"*. Please, attach these log files if you would like to contribute or report a bug and send them via e-mail to <peter@remap.ucla.edu>. 
