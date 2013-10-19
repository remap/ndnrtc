//
//  video-sender.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/21/13
//

#ifndef __ndnrtc__video_sender__
#define __ndnrtc__video_sender__

#include "ndnrtc-common.h"
#include "ndnrtc-namespace.h"
#include "video-coder.h"

#define NDNRTC_FRAMEHDR_MRKR 0xf4d4
#define NDNRTC_FRAMEBODY_MRKR 0xfb0d

namespace ndnrtc
{
    class INdnVideoSenderDelegate;
    
    class VideoSenderParams : public NdnParams {
    public:
        // static
        static VideoSenderParams* defaultParams()
        {
            VideoSenderParams * p = new VideoSenderParams();
            
            char *hub = (char*)"ndn/ucla.edu/apps";
            char *user = (char*)"testuser";
            char *stream = (char*)"video0";

            p->setStringParam(ParamNameNdnHub, hub);
            p->setStringParam(ParamNameStreamName, stream);
            p->setStringParam(ParamNameProducerId, user);
            p->setIntParam(ParamNameSegmentSize, 3800);
            p->setIntParam(ParamNameFrameFreshnessInterval, 5);
//            p->setStringParam(ParamNameStreamPrefix, *NdnRtcNamespace::getStreamPath(hub, user, stream).get());
            
            return p;
        }
        
        // public methods go here
        int getHub(char **hub) const { return getParamAsString(ParamNameNdnHub, hub); }
        int getProducerId(char **producerId) const { return getParamAsString(ParamNameProducerId, producerId); }
        int getStreamName(char **streamName) const { return getParamAsString(ParamNameStreamName, streamName); }
        int getSegmentSize(unsigned int *segmentSize) const { return getParamAsInt(ParamNameSegmentSize, (int*)segmentSize); }
        int getFreshnessInterval(unsigned int *freshness) const {return getParamAsInt(ParamNameFrameFreshnessInterval, (int*)freshness); }
        
        int getSegmentSize() const { return getParamAsInt(ParamNameSegmentSize); }
        std::string getUserPrefix() const;
        std::string getStreamPrefix() const;
        std::string getStreamKeyPrefix() const;
        std::string getStreamFramePrefix() const;
    };

    /**
     * Class is used for packaging encoded frame metadata and actual data in a buffer.
     * It has also methods for unarchiving this data into an encoded frame.
     */
    class NdnFrameData
    {
    public:
        NdnFrameData(webrtc::EncodedImage &frame);
        ~NdnFrameData();
        
        static int unpackFrame(unsigned int length_, const unsigned char *data, webrtc::EncodedImage **frame);
        int getLength() { return length_; }
        unsigned char* getData() { return data_; }
        
    private:
        struct FrameDataHeader {
            uint32_t                    headerMarker_ = NDNRTC_FRAMEHDR_MRKR;
            uint32_t                    encodedWidth_;
            uint32_t                    encodedHeight_;
            uint32_t                    timeStamp_;
            int64_t                     capture_time_ms_;
            webrtc::VideoFrameType      frameType_;
//            uint8_t*                    _buffer;
//            uint32_t                    _length;
//            uint32_t                    _size;
            bool                        completeFrame_;
            uint32_t                    bodyMarker_ = NDNRTC_FRAMEBODY_MRKR;
        };
        
        unsigned int length_;
        unsigned char *data_;
    };
    
    /**
     *
     */
    class NdnVideoSender : public NdnRtcObject, public IEncodedFrameConsumer {
    public:
        // construction/desctruction
        NdnVideoSender(const NdnParams * params):NdnRtcObject(params), frameNo_(0) {}
        ~NdnVideoSender(){ TRACE("sender is dead"); };
        
        // public methods go here
        int init(const shared_ptr<Transport> transport);
//        void registerConference(VideoSenderParams &params);
        unsigned long int getFrameNo() { return frameNo_; };
        
        // interface conformance
        void onEncodedFrameDelivered(webrtc::EncodedImage &encodedImage);
        
    private:        
        // private attributes go here
        shared_ptr<Transport> ndnTransport_;
        shared_ptr<KeyChain> ndnKeyChain_;
        shared_ptr<Name> framePrefix_;
        shared_ptr<Name> certificateName_;
        
        unsigned long int frameNo_; // sequential frame number
        int segmentSize_, freshnessInterval_;
        
        // private methods go here        
    };
    
    class INdnVideoSenderDelegate : public INdnRtcObjectObserver {
    public:
    };
}

#endif /* defined(__ndnrtc__video_sender__) */
