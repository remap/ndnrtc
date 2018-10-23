//
// client.h
//
//  Created by Peter Gusev on 07 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __client_h__
#define __client_h__

#include <stdlib.h>
#include <boost/shared_ptr.hpp>
#include <ndnrtc/interfaces.hpp>

#include "config.hpp"
#include "stream.hpp"
#include "stat-collector.hpp"

namespace ndn
{
class KeyChain;
}

class Client
{
  public:
    Client(boost::asio::io_service &io, boost::asio::io_service& rendererIo,
           const boost::shared_ptr<ndn::Face> &face,
           const boost::shared_ptr<ndn::KeyChain> &keyChain) : io_(io), rendererIo_(rendererIo),
                                                               face_(face), keyChain_(keyChain) {}
    ~Client() {}

    // blocking call. will return after runTimeSec seconds
    void run(unsigned int runTimeSec, unsigned int statSamplePeriodMs,
             const ClientParams &params, const std::string &instanceName);

  private:
    boost::asio::io_service &io_, &rendererIo_;
    unsigned int runTimeSec_, statSampleIntervalMs_;
    ClientParams params_;

    boost::shared_ptr<StatCollector> statCollector_;
    boost::shared_ptr<ndn::Face> face_;
    boost::shared_ptr<ndn::KeyChain> keyChain_;

    std::vector<RemoteStream> remoteStreams_;
    std::vector<LocalStream> localStreams_;
    std::string instanceName_;

    Client(Client const &) = delete;
    void operator=(Client const &) = delete;

    bool setupConsumer();
    bool setupProducer();
    void setupStatGathering();
    void runProcessLoop();
    void tearDownStatGathering();
    void tearDownProducer();
    void tearDownConsumer();

    RendererInternal *setupRenderer(const ConsumerStreamParams &p);

    RemoteStream initRemoteStream(const ConsumerStreamParams &p,
                                  const ndnrtc::GeneralConsumerParams &generalParams);
    LocalStream initLocalStream(const ProducerStreamParams &p);
    boost::shared_ptr<RawFrame> sampleFrameForStream(const ProducerStreamParams &p);

    boost::shared_ptr<ndnlog::new_api::Logger> producerLogger(std::string streamName);
    boost::shared_ptr<ndnlog::new_api::Logger> consumerLogger(std::string prefix,
                                                              std::string streamName);
};

#endif
