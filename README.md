ndnrtc
======

NDN WebRTC Conferencing Project

For license information see LICENSE.

http://named-data.net/

Description
----

Early stages of an NDN-based videoconferencing tool based on the WebRTC implementation in Firefox.

Structure
----
App code is divided into to main parts - C++ code of add-on and Javascript code of web app.
C++ code provides all basic operations for establishing NDN-connections to other peers and transmitting/receiving encoded media. It also exposes it's interface to the web-page Javascript through Javascript wrapper object attached to DOM's window object of a page.

Javascript code of web app provides all th UI and peer discovery/authentication logic of the NDN-RTC application.

The directory structure is as follows:

* **/root**
    * **ccp/** *-- c++ code*
        * **addon/** *-- Firefox-specific files for add-on installation*
            * **components/** *-- Javascript wrapper code*
        * **bin/** *-- compiled files, dynamic library of c++ add-on, .xpt component interface file for Firefox*
            * **ndnrtc/** *-- upacked add-on*
            * **ndnrtc.xpi** *-- compiled add-on package*
        * **content/** *-- add-on resources and helper Javascript files*
        * **idl/** *-- add-on's idl interface files*
        * **src/** *-- .h and .cpp files of c++ component*
    * **html/** *-- web app directory*
        * **js/** *-- Javascript for web app*
        * **index.html** *-- main page for web app*
    * **LICENSE**
    * **README.md**

Build prerequisites
----
1. XULRunner SDK (or Gecko SDK). Check the latest version [here](http://ftp.mozilla.org/pub/mozilla.org/xulrunner/nightly/latest-trunk/). Current code was compiled with SDK version 25.0a1 (Nightly).

How to build
----
1. Setup `XULSDK` environment path:
<pre>
    $ export XULSDK=~/mozilla/xulrunner/dist
</pre>
    
2. Setup `PYTHONPATH` environment path:
<pre>
    $ export PYTHONPATH=$(PYTHONPATH):$(XULSDK)/sdk/bin
</pre>

3. Perform make:
<pre>
    $ cd cpp
    $ make
</pre>

How to use
----
1. Check **bin** directory for **ndnrtc.xpi** file, open it in Firefox and restart.
2. Open **html/index.html** file.
