//
//  media-thread.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__media_sender__
#define __ndnrtc__media_sender__

#define NLOG_COMPONENT_NAME "NdnRtcSender"

#include "ndnrtc-common.h"
#include "ndnrtc-object.h"
#include "ndnrtc-utils.h"
#include "frame-buffer.h"
#include "segmentizer.h"

namespace ndnrtc
{
    namespace new_api {
        /**
         * This class carries paramteres necessary for establishing and 
         * operating a media stream
         */
        class MediaThreadSettings
        {
        public:
            MediaThreadSettings(){}
            virtual ~MediaThreadSettings(){}
            
            bool useCache_;
            unsigned int segmentSize_, dataFreshnessMs_;
            std::string threadName_;
            
            ndn::Name prefix_;
            ndn::Name certificateName_;
            
            boost::shared_ptr<ndn::KeyChain> keyChain_;
            boost::shared_ptr<FaceProcessor> faceProcessor_;
        };
        
        /**
         * This class contains media thread statistics
         */
        class MediaThreadStatistics : public ObjectStatistics
        {
        public:
            double packetRate_, bitrate_;
        };
        
        /**
         * Media thread callback class asynchronously notifies about important 
         * events happened to media thread
         */
        class IMediaThreadCallback : public INdnRtcComponentCallback
        {
        public:
            /**
             * Called whenever thread failed to register prefix
             */
            virtual void onMediaThreadRegistrationFailed() = 0;
        };
        
        /**
         * This is a base class for media stream thread. Stream threads are used
         * for publishing media packets (video frames or audio samples) in NDN
         * network.
         */
        class MediaThread : public NdnRtcComponent
        {
        public:
            MediaThread();
            ~MediaThread();
            
            virtual int init(const MediaThreadSettings &settings);
            virtual void stop();
            
            unsigned long int getPacketNo() { return packetNo_; }
            
            void
            getStatistics(MediaThreadStatistics& statistics)
            {
                statistics.packetRate_ = NdnRtcUtils::currentFrequencyMeterValue(packetRateMeter_);
                statistics.bitrate_ = NdnRtcUtils::currentDataRateMeterValue(dataRateMeter_);
            }
            
            void
            getSettings(MediaThreadSettings& settings)
            { settings = settings_; }
            
        protected:
            typedef struct _PitEntry {
                int64_t arrivalTimestamp_;
                boost::shared_ptr<const Interest> interest_;
            } PitEntry;
            
            MediaThreadSettings settings_;
            unsigned int segSizeNoHeader_;
            
            boost::shared_ptr<FaceProcessor> faceProcessor_;
            boost::shared_ptr<MemoryContentCache> memCache_;
            
            PacketNumber packetNo_ = 0; // sequential packet number

            unsigned int packetRateMeter_;
            unsigned int dataRateMeter_;
            
            std::map<Name, PitEntry> pit_;
            webrtc::CriticalSectionWrapper &pitCs_;
            
            IMediaThreadCallback*
            getCallback()
            { return (IMediaThreadCallback*)callback_; }
            
            /**
             * Publishes specified data in the ndn network under specified prefix by
             * appending packet number to it. Big data blocks will be splitted by
             * segments and published under the
             * "<prefix>/<packetNo>/<segment>/<prefix_meta>" prefix.
             * @param packetData Pointer to the data being published
             * @param packetPrefix NDN name prefix under which data will be published
             * @param packetNo Packet number
             * @param prefixMeta Prefix metadata which will be added after main
             * prefix with packet and segment numbers
             * @return Number of segments used for publishing data or RESULT_FAIL on
             *          error
             */
            int publishPacket(PacketData &packetData,
                              const Name& packetPrefix,
                              PacketNumber packetNo,
                              PrefixMetaInfo prefixMeta,
                              double captureTimestamp);
            
            void registerPrefix(const Name& prefix);
            
            void addToPit(const boost::shared_ptr<const Interest>& interest);
            int lookupPrefixInPit(const Name &prefix,
                                  SegmentData::SegmentMetaInfo &metaInfo);
            
            // ndn-cpp callbacks
            virtual void
            onInterest(const boost::shared_ptr<const Name>& prefix,
                       const boost::shared_ptr<const Interest>& interest,
                       ndn::Transport& transport);
            
            virtual void
            onRegisterFailed(const boost::shared_ptr<const Name>& prefix);
        };
    }
}

#endif /* defined(__ndnrtc__media_sender__) */
