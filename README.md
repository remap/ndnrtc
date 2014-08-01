NDN-RTC
----

NDN Real Time Communication Conferencing Project

For license information see LICENSE.

http://named-data.net/

Description
----

NDN-based video conferencing library.

Library provides functions for establishing connections to NDN-hubs and transmitting/receiving encoded media (audio/video) between two or more instances.
Library is written in C++ and is a dynamic library. There is a binary for console demo application (ndnrtc-demo) included in the project which can demonstrate the use of the library.

Library requires local or remote NDN daemon and has been tested with:
* [ndnd](https://github.com/named-data/ndnx) from NDNx
* [ndnd-tlv](https://github.com/named-data/ndnd-tlv) (which uses NDNx)
* [NFD forwarder](https://github.com/named-data/NFD)

Structure
----

The directory structure is as follows:

* **/root**
    * **ccp/**
        * **src/** *-- source code*
        * **include/** *-- public headers*
        * **demoapp/** *-- demo application code (MacOS X only)*
        * **test/** *-- unit tests code*
        * **resources/** *-- unit tests' resources, scripts, etc.*  
    * **docs/** *-- documentation*
    * **LICENSE**
    * **README.md**

Building instructions
----
Please, see the [cpp/INSTALL.md](cpp/INSTALL.md) for build and install instructions.

Demo app (MacOS only)
----
This code is shipped with a demo application which loads NDN-RTC library at runtime and provides functionality for establishing many-to-many audiovisual conferences over NDN networks. Please see [cpp/demoapp/README.md](cpp/demoapp/README.md) for more information.

License
---
This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version, with the additional exemption that compiling, linking, and/or using OpenSSL is allowed.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. If not, see http://www.gnu.org/licenses/. A copy of the GNU General Public License is in the file [cpp/COPYING](cpp/COPYING).
