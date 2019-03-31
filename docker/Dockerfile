FROM ubuntu:16.04
LABEL maintainer "Peter Gusev <peter@remap.ucla.edu>"

RUN  apt-get update \
     && apt-get install -y libssl-dev libboost-all-dev libsqlite3-dev sudo \
     wget autoconf automake libtool cmake git build-essential \
     lsb-release gawk nasm

# OpenFEC
RUN wget http://openfec.org/files/openfec_v1_4_2.tgz \
    && tar -xvf openfec_v1_4_2.tgz && rm openfec_v1_4_2.tgz \
    && mkdir -p openfec_v1.4.2/build && cd openfec_v1.4.2/ \
    && wget https://raw.githubusercontent.com/remap/ndnrtc/master/cpp/resources/ndnrtc-openfec.patch \
    && patch src/CMakeLists.txt ndnrtc-openfec.patch \
    && cd build && cmake .. -DDEBUG:STRING=OFF \
    && make

# protobuf
RUN git clone https://github.com/protocolbuffers/protobuf.git \
    && cd protobuf \
    && git submodule update --init --recursive \
    && ./autogen.sh \
    && ./configure \
    && make && make install \
    && ldconfig

# nanomsg
RUN git clone https://github.com/nanomsg/nanomsg.git \
    && cd /nanomsg \
    && ./configure \
    && make && make install

# rocksdb
RUN git clone https://github.com/facebook/rocksdb.git \
    && cd /rocksdb \
    && make static_lib && make install

# libvpx
RUN git clone https://github.com/webmproject/libvpx.git \
    && cd libvpx/build \
    && ../configure --disable-vp8 --enable-shared --enable-better-hw-compatibility --enable-realtime-only --enable-vp9-highbitdepth --enable-runtime-cpu-detect \
    && make && make install

# NDN-CPP
RUN git clone https://github.com/named-data/ndn-cpp \
    && cd ndn-cpp \
    && ./configure --with-std-shared-ptr=no --with-std-function=no \
    && make && make install

# CNL-CPP
RUN git clone https://github.com/named-data/cnl-cpp \
    && cd cnl-cpp \
    && ./configure \
    && make && make install

# NDN-RTC
ARG BRANCH=master
RUN git clone --recursive https://github.com/remap/ndnrtc.git \
    && cd ndnrtc/cpp \
    && git checkout $BRANCH \
    && ./configure CPPFLAGS="-I/openfec_v1.4.2/src/" LDFLAGS="-L/openfec_v1.4.2/bin/Release" \
    && make && make install
