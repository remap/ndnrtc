# NDN-RTC

## Description
----
NDN-RTC C++ library allows one to perform real-time low latency video streaming over [NDN](http://named-data.net/). Library provides functions for creating video streams for publishing and/or fetching over NDN.

For more information about NDN and NDN-RTC design, refer to the original paper [NDN-RTC: Real-Time Videoconferencing over Named Data Networking](https://dl.acm.org/citation.cfm?id=2810176). Current library version might evolve from the original design. Please check this repository for the up-to-date state on the library code and design.

Library requires local [NFD forwarder](https://github.com/named-data/NFD) to be installed and configured.

## Structure

The directory structure is as follows:

* **cpp/**
    * **src/** *-- source code*
    * **include/** *-- public headers*
    * **test/** *-- unit tests code*
    * **resources/** *-- unit tests' resources, helper scripts, etc.*  
* **docker/** *-- Docker image files*
* **docs/** *-- some documentation*
* **py/** *-- Python tools*
* **LICENSE**
* **README.md**

## Build instructions

Please, see the [cpp/INSTALL.md](cpp/INSTALL.md) for build and install instructions.

## Tools

The library comes with a set of [tools](cpp/tools/README.md) that may be instrumental for learning about NDN-RTC and inspecting avilable NDN-RTC data over NDN.

## License

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version, with the additional exemption that compiling, linking, and/or using OpenSSL is allowed.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. If not, see http://www.gnu.org/licenses/. A copy of the GNU General Public License is in the file [cpp/COPYING](cpp/COPYING).
