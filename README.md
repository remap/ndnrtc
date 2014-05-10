ndnrtc
======

NDN WebRTC Conferencing Project

For license information see LICENSE.

http://named-data.net/

Description
----

NDN-based video conferencing library.

It provides all basic operations for establishing NDN-connections to NDN-hub and transmitting/receiving encoded media (audio/video).
Library is written in C++ and is a dynamic library which can be loaded at runtime. There is a binary for console demo application (ndnrtc-app) included in the project which can demonstrate the use of the library.

Structure
----

The directory structure is as follows:

* **/root**
    * **ccp/**
        * **src** *-- source code*
        * **test** *-- unit tests code*
    * **docs/** *-- documentation*
    * **res/** *-- helpful resources, scripts, etc.* 
    * **test_app/**
        * **app/** *-- compiled binaries for library and demo application*
        * **utils** *-- any helpful apps for testing*
    * **LICENSE**
    * **README.md**

Build prerequisites
----
TBD

How to build
----
TBD - currently only binaries for MacOS X 10.7, 10.8 and 10.9 are provided.

How to use
----
1. Open Terminal
2. Run `cd` to the folder where demo app resides:
<pre>
$ cd test_app/app
</pre>
3. Make sure, that ndnrtc library (libndnrtc-sa.dylib) and configuration file (ndnrtc.cfg) are in the same directory as demo app:
<pre>
$ ls
libndnrtc-sa.dylib*
ndnrtc-app*		
ndnrtc.cfg		
refine_log.sh*
</pre>
4. Verify that configuration file contains all the parameters you want. App loads configuration file at start-up time, but also allows run-time loading (option 5 in main menu). 
<pre>
$ nano ndnrtc.cfg
</pre>
5. Run demo app:
<pre>
$ ./ndnrtc-app
</pre>

Demo app
----
Demo application is a simple console application which loads ndnrtc library (NOTE: *the library should reside in the same folder as the demo app*) and provides functionality to publish media streams (audio, video or both, depending on configuration file). Demo app configures ndnrtc library using parameters loaded from configuration file (ndnrtc.cfg by default). Configuration file is a simple text file (as defined by libconfig) with several sections. Default configuration file has plenty of comments and is self-explanatory.

**Some usful tips for using demo app:**

In order to:
* **start publishing** choose option *"1 start media publishing"* or press *1*. After that, the app will ask for user name and prefix under which you would like to publish media. NOTE: *if you would like to run separate consumer later on, make sure you chose prefix which is routable from your machine*.
* **fetch media stream** choose option *"3 fetch stream"* or press *3*. After that, specify username and prefix. Depending on configuration, media playback should start shortly.
* **stop fetching** choose option *"4 stop fetching stream"* or press *4*. After that, specify username from which you want to stop fetching media.
* **run loopback test** choose option *"6 loopback mode"* or press "*6*". App will start publishing media under username "loopback" and default prefix. Also, app will start fetching this data.
* **see runtime statistics** choose option *"7 show statistics"* or press "*7*". App will open screen with statistics for current media publisher/consumers. To navigate between several consumers use arrows.

**Feedback**

While running, demo app is logging events in several log files (depending on use case). For publishing, logs are stored in *"producer-<username>.log"* files, for fetching - in *"consumer-<username>.log"*. Please, attach these log files if you would like to contribute or report a bug and send them via e-mail to <peter@remap.ucla.edu>. Also you can use provided script *refine_log.sh* which will gather all log files into one time-stamped folder and zip it.
