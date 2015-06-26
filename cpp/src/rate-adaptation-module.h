//
//  rate-adaptation-module.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Authors:  Peter Gusev, Takahiro Yoneda
//

#ifndef ndnrtc_rate_adaptation_module_h
#define ndnrtc_rate_adaptation_module_h

#include <string>
#include <stdint.h>

namespace ndnrtc {
    
    /**
     * Codec modes
     */
    typedef enum _CodecMode {
        CodecModeNormal, // normal codec mode
        CodecModeSVC     // scalable video coding (SVC)
    } CodecMode;
    
    typedef struct _StreamEntry {
        unsigned int id_;
        double bitrate_;
        double parityRatio_;
    } StreamEntry;
    
    /**
     * This abstract class defines an interface for a rate adaptation decision
     * module which provides several functions:
     *  - detects network congestions;
     *  - recommends video bitrate for the current network conditions;
     *  - recommends interest issuing rate for the current network conditions.
     */
    class IRateAdaptationModule {
    public:
        /**
         * Module intitialization
         * @param codecMode Codec mode used by producer
         * @param nStreams Number of video streams with different bitrates
         * @param streamArray Array of avaliable video streams
         * @return 0 If intitialization completed succesfully, negative values
         * represent error codes.
         */
        virtual int initialize(const CodecMode& codecMode,
                               uint32_t nStreams,
                               StreamEntry* streamArray) = 0;
        
        /**
         * This method should be called each time new interest is expressed
         * @param interestName Name of the expressed interest
         * @param streamId Stream id which relates to the expressed interest
         */
        //virtual void interestExpressed(const shared_ptr<Name>& interestName,
        virtual void interestExpressed(const std::string &name,
                                       unsigned int streamId) = 0;

        
        /**
         * This method should be called each time the interest is timed out
         * @param interestName Name of the timed out interest
         * @param streamId Stream id which relates to the timed out interest
         * @param timestampMs Timeout timestamp in milliseconds
         */
        virtual void interestTimeout(const std::string &name,
                                     unsigned int streamId) = 0;
        
        /**
         * This method should be called each time new data packet has arrived
         * @param dataName Name of the data packet received
         * @param streamId Stream id which relates to the received data packet
         * @param dataSize Size of the data packet in bytes
         * @param timestampMs Time in milliseconds whe the packet arrvied
         */
        //virtual void dataReceived(const shared_ptr<Name>& dataName,
        virtual void dataReceived(const std::string &name,
                                  unsigned int streamId,
                                  unsigned int dataSize,
                                  double rttMs,
                                  unsigned int nRtx) = 0;
        
        /**
         * This method should be called periodically (i.e. every 50ms) in order
         * to get updated current values for interest issuing rate.
         * @param interestRate Upon return from this method, contains interest
         * rate (interests per second) which should be applied to the stream
         * with streamId in order to avoid congestions
         * @param streamId ID of the video stream, to which new interest rate
         * should be applied
         */
        virtual void getInterestRate(int64_t timestampMs,
                                     double& interestRate,
                                     unsigned int& streamId) = 0;
    };
}

#endif
