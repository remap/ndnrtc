ndnrtc
======

NDN WebRTC Conferencing Project

Build prerequisites
----
1. XULRunner SDK (or Gecko SDK). Check the latest version [here](https://developer.mozilla.org/en-US/docs/Gecko_SDK). Current code was compiled with SDK version 25.0a1 (Nightly).

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
