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
            StreamTS = 1<<3|Stream,     // stream with timestamp component
            ThreadNT = 1<<4|StreamTS,   // thread - no frame type component
            Thread = 1<<5|ThreadNT,
            Sample = 1<<6|Thread,
            Segment = 1<<7|Sample
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
        Meta,
        Pointer
    };

    /**
     * NamespaceInfo represents information that can be extracted from
     * legitimate NDN-RTC name (interest or data).
     */
    class NamespaceInfo {
    public:
        NamespaceInfo() : apiVersion_(0), isMeta_(false), isParity_(false),
            isDelta_(false), hasSeqNo_(false), class_(SampleClass::Unknown),
            segmentClass_(SegmentClass::Unknown), sampleNo_(0), segNo_(0), segmentVersion_(0){}

        ndn::Name basePrefix_;
        unsigned int apiVersion_;
        std::string streamName_;
        uint64_t streamTimestamp_;

        bool hasSeqNo_, hasSegNo_, isGopStart_, isGopEnd_, hasVersion_;
        SegmentClass segmentClass_;
        PacketNumber sampleNo_;
        unsigned int segNo_;
        unsigned int version_;

        void reset();

        ndn::Name getPrefix(int filter = (prefix_filter::Segment)) const;
        ndn::Name getSuffix(int filter = (suffix_filter::Segment)) const;

        bool isValidInterestPrefix() const;
        bool isValidDataPrefix() const;

        // BELOW ARE DEPRECATED MEMBERS
        bool isMeta_ DEPRECATED;
        bool isParity_ DEPRECATED;
        MediaStreamParams::MediaStreamType streamType_ DEPRECATED;
        std::string threadName_ DEPRECATED;
        bool isDelta_ DEPRECATED;
        SampleClass class_ DEPRECATED;
        unsigned int segmentVersion_ DEPRECATED;
    };

    class NameComponents {
    public:
        enum class Filter {
            Stream,
            Meta,
            Live,
            Latest,
            GopStart,
            GopEnd,
            Frame,
            FrameMeta,
            Manifest,
            Payload,
            Parity
        };

        static const std::string App;
        static const std::string Meta;
        static const std::string Latest;
        static const std::string Live;
        static const std::string Gop;
        static const std::string GopEnd;
        static const std::string GopStart;
        static const std::string Manifest;
        static const std::string Parity;

        static std::string
        fullVersion();

        static unsigned int
        nameApiVersion();

        static ndn::Name
        ndnrtcSuffix();

        static ndn::Name
        streamPrefix(std::string basePrefix, std::string streamName);

        static bool extractInfo(const ndn::Name& name, NamespaceInfo& info);

        // BELOW ARE DEPRECATED MEMBERS
        static const std::string NameComponentApp DEPRECATED;
        static const std::string NameComponentAudio DEPRECATED;
        static const std::string NameComponentVideo DEPRECATED;
        static const std::string NameComponentMeta DEPRECATED;
        static const std::string NameComponentDelta DEPRECATED;
        static const std::string NameComponentKey DEPRECATED;
        static const std::string NameComponentParity DEPRECATED;
        static const std::string NameComponentManifest DEPRECATED;
        static const std::string NameComponentRdrLatest DEPRECATED;

        static ndn::Name
        streamPrefix(MediaStreamParams::MediaStreamType type, std::string basePrefix) DEPRECATED;

        static ndn::Name
        audioStreamPrefix(std::string basePrefix) DEPRECATED;

        static ndn::Name
        videoStreamPrefix(std::string basePrefix) DEPRECATED;

    };
}

#endif
