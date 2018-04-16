//
//  name-components.hpp
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

#include "params.hpp"
#include "ndnrtc-common.hpp"

#define SAMPLE_SUFFIX(n)(n.getSubName(-3,3))
#define PARITY_SUFFIX(n)(n.getSubName(-4,4))

namespace ndnrtc {
    namespace prefix_filter {
        typedef enum _PrefixFilter {
            Base = 1<<0,
            Library = 1<<1|Base,
            Stream = 1<<2|Library,
            ThreadNT = 1<<3|Stream,
            Thread = 1<<4|ThreadNT,
            Sample = 1<<5|Thread,
            Segment = 1<<6|Sample
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
        Unknown,
        Key,
        Delta
    };

    enum class SegmentClass {
        Unknown,
        Data,
        Parity,
        Manifest,
        Meta
    };

    /**
     * NamespaceInfo represents information that can be extracted from 
     * legitimate NDN-RTC name (interest or data).
     */
    class NamespaceInfo {
    public:
        NamespaceInfo():apiVersion_(0), isMeta_(false), isParity_(false), 
            isDelta_(false), hasSeqNo_(false), class_(SampleClass::Unknown),
            segmentClass_(SegmentClass::Unknown), sampleNo_(0), segNo_(0), metaVersion_(0){}

        ndn::Name basePrefix_;
        unsigned int apiVersion_;
        MediaStreamParams::MediaStreamType streamType_;
        std::string streamName_, threadName_;
        bool isMeta_, isParity_, isDelta_, hasSeqNo_, hasSegNo_;
        SampleClass class_;
        SegmentClass segmentClass_;
        PacketNumber sampleNo_;
        unsigned int segNo_;
        unsigned int metaVersion_;

        ndn::Name getPrefix(int filter = (prefix_filter::Segment)) const;
        ndn::Name getSuffix(int filter = (suffix_filter::Segment)) const;
    };

    class NameComponents {
    public:
        static const std::string NameComponentApp;
        static const std::string NameComponentAudio;
        static const std::string NameComponentVideo;
        static const std::string NameComponentMeta;
        static const std::string NameComponentDelta;
        static const std::string NameComponentKey;
        static const std::string NameComponentParity;
        static const std::string NameComponentManifest;

        static std::string 
        fullVersion();

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
