//
//  name-components.h
//  libndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef libndnrtc_ndnrtc_name_components_h
#define libndnrtc_ndnrtc_name_components_h

#include <string>
#include <ndn-cpp/name.hpp>

#include "params.h"
#include "ndnrtc-common.h"

namespace ndnrtc {
    namespace prefix_filter {
        typedef enum _PrefixFilter {
            Base = 1<<0,
            Library = 1<<1|Base,
            Stream = 1<<2|Library,
            Thread = 1<<3|Stream,
            Sample = 1<<4|Thread,
            Segment = 1<<5|Sample
        } PrefixFilter;
    }

    namespace suffix_filter {
        typedef enum _SuffixFilter {
            Segment = 1<<0,
            Sample = 1<<1|Segment,
            Thread = 1<<2|Sample,
            Stream = 1<<3|Thread,
            Library = 1<<4|Stream
        } SuffixFilter;
    }

    enum class SampleClass {
        Unknown = -1,
        Key = 0,
        Delta = 1
    };

    enum class SegmentClass {
        Unknow = -1,
        Data = 0,
        Parity = 1
    };

    /**
     * NamespaceInfo represents information that can be extracted from 
     * legitimate NDN-RTC name (interest or data).
     */
    class NamespaceInfo {
    public:
        NamespaceInfo():apiVersion_(0), isMeta_(false), isParity_(false), 
            isDelta_(false), hasSeqNo_(false), class_(SampleClass::Unknown),
            sampleNo_(0), segNo_(0), metaVersion_(0){}

        ndn::Name basePrefix_;
        unsigned int apiVersion_;
        MediaStreamParams::MediaStreamType streamType_;
        std::string streamName_, threadName_;
        bool isMeta_, isParity_, isDelta_, hasSeqNo_;
        SampleClass class_;
        PacketNumber sampleNo_;
        unsigned int segNo_;
        unsigned int metaVersion_;

        ndn::Name getPrefix(int filter = (prefix_filter::Segment)) const;
        ndn::Name getSuffix(int filter = (suffix_filter::Segment)) const;
    };

    class NameComponents {
    public:
        #if 0
        static const std::string NameComponentApp;
        static const std::string NameComponentUser;
        static const std::string NameComponentBroadcast;
        static const std::string NameComponentDiscovery;
        static const std::string NameComponentUserStreams;
        static const std::string NameComponentSession;
        static const std::string NameComponentStreamAccess;
        static const std::string NameComponentStreamKey;
        static const std::string NameComponentStreamFramesDelta;
        static const std::string NameComponentStreamFramesKey;
        static const std::string NameComponentStreamInfo;
        static const std::string NameComponentFrameSegmentData;
        static const std::string NameComponentFrameSegmentParity;
        static const std::string KeyComponent;
        static const std::string CertificateComponent;
        
        static std::string
        getUserPrefix(const std::string& username,
                      const std::string& prefix);
        
        static std::string
        getStreamPrefix(const std::string& streamName,
                        const std::string& username,
                        const std::string& prefix);
        
        static std::string
        getThreadPrefix(const std::string& threadName,
                        const std::string& streamName,
                        const std::string& username,
                        const std::string& prefix);

        static std::string 
        getUserName(const std::string& prefix);

        static std::string 
        getStreamName(const std::string& prefix);

        static std::string 
        getThreadName(const std::string& prefix);
        #endif

        //****************************************************************************
        static const std::string NameComponentApp;
        static const std::string NameComponentAudio;
        static const std::string NameComponentVideo;
        static const std::string NameComponentMeta;
        static const std::string NameComponentDelta;
        static const std::string NameComponentKey;
        static const std::string NameComponentParity;

        static unsigned int 
        nameApiVersion();

        static ndn::Name
        ndnrtcSuffix();

        static ndn::Name
        streamPrefix(MediaStreamParams::MediaStreamType type, std::string basePrefix);

        static ndn::Name
        audioStreamPrefix(std::string basePrefix);

        static ndn::Name
        videoStreamPrefix(std::string basePrefix);

        static bool extractInfo(const ndn::Name& name, NamespaceInfo& info);
    };
}

#endif
