//
//  name-components.cpp
//  libndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <string>
#include <sstream>
#include <algorithm>
#include <bitset>
#include <iterator>
#include <boost/assign.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string.hpp>
#include <ndn-cpp/interest-filter.hpp>

#include "name-components.hpp"
#include "clock.hpp"

using namespace std;
using namespace ndnrtc;
using namespace ndn;
using namespace boost::assign;

const string NameComponents::NameComponentApp = "ndnrtc";
const string NameComponents::NameComponentAudio = "audio";
const string NameComponents::NameComponentVideo = "video";
const string NameComponents::NameComponentMeta = "_meta";
const string NameComponents::NameComponentDelta = "d";
const string NameComponents::NameComponentKey = "k";
const string NameComponents::NameComponentParity = "_parity";
const string NameComponents::NameComponentManifest = "_manifest";
const string NameComponents::NameComponentRdrLatest = "_latest";

const string NameComponents::App = "ndnrtc";
const string NameComponents::Meta = "_meta";
const string NameComponents::Latest = "_latest";
const string NameComponents::Live = "_live";
const string NameComponents::Gop = "_gop";
const string NameComponents::GopEnd = "end";
const string NameComponents::GopStart = "start";
const string NameComponents::Manifest = "_manifest";
const string NameComponents::Parity = "_parity";

static map<NameComponents::Filter, InterestFilter> NameFilters =
map_list_of
(NameComponents::Filter::Stream,    InterestFilter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)<>{,3}"))
(NameComponents::Filter::Meta,      InterestFilter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)<_meta>$"))
(NameComponents::Filter::Live,      InterestFilter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)<_live>(<%FD[\\w%\+\-\.]+>)?$"))
(NameComponents::Filter::Latest,    InterestFilter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)<_latest>(<%FD[\\w%\+\-\.]+>)?$"))
(NameComponents::Filter::GopStart,  InterestFilter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)<_gop>(<%FE[\\w%\+\-\.]+>)<start>$"))
(NameComponents::Filter::GopEnd,    InterestFilter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)<_gop>(<%FE[\\w%\+\-\.]+>)<end>$"))
(NameComponents::Filter::Frame,    InterestFilter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)(<%FE[\\w%\+\-\.]+>)<>{,2}"))
(NameComponents::Filter::FrameMeta, InterestFilter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)(<%FE[\\w%\+\-\.]+>)<_meta>$"))
(NameComponents::Filter::Manifest,  InterestFilter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)(<%FE[\\w%\+\-\.]+>)<_manifest>$"))
(NameComponents::Filter::Payload,   InterestFilter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)(<%FE[\\w%\+\-\.]+>)(<%00[\\w%\+\-\.]+>)$"))
(NameComponents::Filter::Parity,    InterestFilter("/", "(<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)(<%FE[\\w%\+\-\.]+>)<_parity>(<%00[\\w%\+\-\.]+>)?$"));

void
NamespaceInfo::reset()
{
    basePrefix_ = Name();
    apiVersion_ = 0;
    streamName_ = "";
    streamTimestamp_ = 0;
    hasSeqNo_ = hasSegNo_ = isGopStart_ =
        isGopEnd_ = hasVersion_ = isLiveMeta_ = isLatestPtr_ = false;
    segmentClass_ = SegmentClass::Unknown;
    sampleNo_ = 0;
    segNo_ = 0;
    version_ = 0;
}

bool
NamespaceInfo::isValidInterestPrefix() const
{
    return false;
}

bool
NamespaceInfo::isValidDataPrefix() const
{
    return false;
}

Name
NamespaceInfo::getPrefix(uint8_t filter) const
{
    // cout << bitset<8>(filter) << endl;
    Name prefix;

    if (filter & NameFilter::Base)
        prefix.append(basePrefix_);
    if (filter & (NameFilter::Library ^ NameFilter::Base))
        prefix.append(NameComponents::App).appendVersion(apiVersion_);
    if (filter & (NameFilter::Timestamp ^ NameFilter::Library))
        prefix.appendTimestamp(streamTimestamp_);
    if (filter & (NameFilter::Stream ^ NameFilter::Timestamp))
        prefix.append(streamName_);

    if (filter & (NameFilter::Sample ^ NameFilter::Stream))
    {
        if (hasSeqNo_)
        {
            if (isGopStart_ || isGopEnd_)
                prefix.append(NameComponents::Gop);

            prefix.appendSequenceNumber(sampleNo_);
        }
    }

    if (filter & (NameFilter::Segment ^ NameFilter::Sample))
    {
        if (hasSegNo_)
        {
            if (segmentClass_ == SegmentClass::Parity)
                prefix.append(NameComponents::Parity);
            prefix.appendSegment(segNo_);
        }
        else
        {
            if (segmentClass_ == SegmentClass::Meta)
                prefix.append(isLiveMeta_ ? NameComponents::Live : NameComponents::Meta);
            if (segmentClass_ == SegmentClass::Manifest)
                prefix.append(NameComponents::Manifest);
            if (isLatestPtr_)
                prefix.append(NameComponents::Latest);

            if (hasVersion_)
                prefix.appendVersion(version_);
            if (isGopStart_)
                prefix.append(NameComponents::GopStart);
            if (isGopEnd_)
                prefix.append(NameComponents::GopEnd);
        }
    }

    return prefix;
}

Name
NamespaceInfo::getPrefix(NameFilter filter) const
{
    return getPrefix(static_cast<uint8_t>(filter));
}

Name
NamespaceInfo::getSuffix(NameFilter filter) const
{
    return getPrefix((uint8_t)(~static_cast<uint8_t>(filter) >> 1));
}

Name
NamespaceInfo::getPrefix(int filter) const
{
    using namespace prefix_filter;
    Name prefix(basePrefix_);

    if (filter)
    {
        if (filter&(Library^Base))
            prefix.append(Name(NameComponents::NameComponentApp)).appendVersion(apiVersion_);
        if (filter&(Stream^Library))
            prefix.append((streamType_ == MediaStreamParams::MediaStreamType::MediaStreamTypeAudio ?
                NameComponents::NameComponentAudio : NameComponents::NameComponentVideo)).append(streamName_);
        if (filter&(StreamTS^Stream) && threadName_ != "")
            prefix.appendTimestamp(streamTimestamp_);
        if (threadName_ != "" &&
            (filter&(Thread^StreamTS)  ||
            filter&(ThreadNT^StreamTS) & streamType_ == MediaStreamParams::MediaStreamType::MediaStreamTypeVideo))
        {
            prefix.append(threadName_);
        }

        if (isMeta_)
        {
            if (filter&(Segment^Thread))
                prefix.append(NameComponents::NameComponentMeta);

            if (filter&(Segment^Sample))
            {
                prefix.appendVersion(segmentVersion_).appendSegment(segNo_);
            }
        }
        else
        {
            if (filter&(Thread^ThreadNT) && threadName_ != "" &&
                streamType_ == MediaStreamParams::MediaStreamType::MediaStreamTypeVideo)
                    prefix.append((class_ == SampleClass::Delta ? NameComponents::NameComponentDelta : NameComponents::NameComponentKey));
            if (filter&(Sample^Thread))
                prefix.appendSequenceNumber(sampleNo_);

            if (filter&(Segment^Sample))
            {
                if (isParity_)
                    prefix.append(NameComponents::NameComponentParity);
                prefix.appendSegment(segNo_);
            }
        }
    }

    return prefix;
}

Name
NamespaceInfo::getSuffix(int filter) const
{
    using namespace suffix_filter;
    Name suffix;

    if (filter)
    {
        if (filter&(Library^Stream))
            suffix.append(Name(NameComponents::NameComponentApp)).appendVersion(apiVersion_);
        if (filter&(Stream^Thread))
        {
            suffix.append((streamType_ == MediaStreamParams::MediaStreamType::MediaStreamTypeAudio ?
                NameComponents::NameComponentAudio : NameComponents::NameComponentVideo)).append(streamName_);
            if (threadName_ != "")
                suffix.appendTimestamp(streamTimestamp_);
        }
        if (filter&(Thread^Sample) && threadName_ != "")
            suffix.append(threadName_);

        if (isMeta_)
        {
            if ((filter&(Thread^Sample) && threadName_ != "") ||
                filter&(Stream^Thread))
                suffix.append(NameComponents::NameComponentMeta);
        }

        if (filter&(Thread^Sample) && threadName_ != "" &&
            streamType_ == MediaStreamParams::MediaStreamType::MediaStreamTypeVideo &&
            !isMeta_)
            suffix.append((class_ == SampleClass::Delta ? NameComponents::NameComponentDelta : NameComponents::NameComponentKey));
        if (filter&(Sample^Segment))
        {
            if (isMeta_)
                suffix.appendVersion(segmentVersion_);
            else
                suffix.appendSequenceNumber(sampleNo_);
        }
        if (filter&(Segment) && hasSegNo_)
        {
            if (isParity_)
                suffix.append(NameComponents::NameComponentParity);
            suffix.appendSegment(segNo_);
        }
    }
    return suffix;
}

//******************************************************************************
#if 0
void initFilters()
{
    // (<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)
    string base =  "(<>+)<"+
                    NameComponents::App + "><" +
                    Name::Component::fromVersion(NameComponents::nameApiVersion()) + ">" +
                    "(<%FC[%\\w\+\-\.]+>)(<>)";
    // (<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)<>{,3}
    NameFilters[NameComponents::Filter::Stream] = InterestFilter("/", base+"<>{,3}");
    // (<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)<_meta>$
    NameFilters[NameComponents::Filter::Meta] = InterestFilter("/", base+"<_meta>$");
    // (<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)<_live>(<%FD[\\w%\+\-\.]+>)?$
    NameFilters[NameComponents::Filter::Live] = InterestFilter("/", base+"<_live>(<%FD[\\w%\+\-\.]+>)?$");
    // (<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)<_latest>(<%FD[\\w%\+\-\.]+>)?$
    NameFilters[NameComponents::Filter::Latest] = InterestFilter("/", base+"<_latest>(<%FD[\\w%\+\-\.]+>)?$");
    // (<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)<_gop>(<%FE[\\w%\+\-\.]+>)<start>$
    NameFilters[NameComponents::Filter::GopStart] = InterestFilter("/", base+"<_gop>(<%FE[\\w%\+\-\.]+>)<start>$");
    // (<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)<_gop>(<%FE[\\w%\+\-\.]+>)<end>$
    NameFilters[NameComponents::Filter::GopEnd] = InterestFilter("/", base+"<_gop>(<%FE[\\w%\+\-\.]+>)<end>$");
    // (<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)(<%FE[\\w%\+\-\.]+>)<>{,2}
    NameFilters[NameComponents::Filter::Frame] = InterestFilter("/", base+"<>{,2}");
    // (<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)(<%FE[\\w%\+\-\.]+>)<_meta>$
    NameFilters[NameComponents::Filter::FrameMeta] = InterestFilter("/", base+"<_meta>$");
    // (<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)(<%FE[\\w%\+\-\.]+>)<_manifest>$
    NameFilters[NameComponents::Filter::Manifest] = InterestFilter("/", base+"<_manifest>$");
    // (<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)(<%FE[\\w%\+\-\.]+>)(<%00[\\w%\+\-\.]+>)$
    NameFilters[NameComponents::Filter::Payload] = InterestFilter("/", base+"(<%00[\\w%\+\-\.]+>)$");
    // (<>+)<ndnrtc><%FD%04>(<%FC[%\\w\+\-\.]+>)(<>)(<%FE[\\w%\+\-\.]+>)<_parity>(<%00[\\w%\+\-\.]+>)?$
    NameFilters[NameComponents::Filter::Parity] = InterestFilter("/", base+"<_parity>(<%00[\\w%\+\-\.]+>)?$");
}
#endif

vector<string> ndnrtcVersionComponents()
{
    string version = NameComponents::fullVersion();
    std::vector<string> components;

    boost::erase_all(version, "v");
    boost::split(components, version, boost::is_any_of("."), boost::token_compress_on);
    return components;
}

std::string
NameComponents::fullVersion()
{
    return std::string(PACKAGE_VERSION);
}

unsigned int
NameComponents::nameApiVersion()
{
    return (unsigned int)atoi(ndnrtcVersionComponents().front().c_str());
}

Name
NameComponents::ndnrtcSuffix()
{
    return Name(NameComponentApp).appendVersion(nameApiVersion());
}

Name
NameComponents::streamPrefix(string basePrefix, string streamName)
{
    return Name(basePrefix)
            .append(ndnrtcSuffix())
            .appendTimestamp(clock::millisecondTimestamp())
            .append(streamName);
}

Name
NameComponents::streamPrefix(MediaStreamParams::MediaStreamType type, std::string basePrefix)
{
    Name n = Name(basePrefix);
    n.append(ndnrtcSuffix());
    return ((type == MediaStreamParams::MediaStreamType::MediaStreamTypeAudio) ?
        n.append(NameComponentAudio) : n.append(NameComponentVideo));
}

Name
NameComponents::audioStreamPrefix(string basePrefix)
{
    return streamPrefix(MediaStreamParams::MediaStreamType::MediaStreamTypeAudio, basePrefix);
}

Name
NameComponents::videoStreamPrefix(string basePrefix)
{
    return streamPrefix(MediaStreamParams::MediaStreamType::MediaStreamTypeVideo, basePrefix);
}

//******************************************************************************
bool
NameComponents::extractInfo(const ndn::Name& name, NamespaceInfo& info)
{
    bool goodName = false;
    set<NameComponents::Filter> matches;

    for (auto &it : NameFilters)
        if (it.second.doesMatch(name))
            matches.insert(it.first);

    if (matches.size())
    {
        int idx = name.size()-1;
        while (name[idx].toEscapedString() != App && idx > 0)
            idx --;

        info.basePrefix_ = name.getPrefix(idx);
        info.apiVersion_ = name[idx+1].toVersion();
        info.streamTimestamp_ = name[idx+2].toTimestamp();
        info.streamName_ = name[idx+3].toEscapedString();

        info.hasSeqNo_ = info.hasSegNo_ = info.isLiveMeta_ = info.isGopStart_ =
            info.isGopEnd_ = info.hasVersion_ = info.isLatestPtr_ = false;
        info.segmentClass_ = SegmentClass::Unknown;
        info.sampleNo_ = 0;
        info.segNo_ = 0;
        info.version_ = 0;

        Name suffix;
        if (idx + 4 < name.size())
            suffix = name.getSubName(idx+4);

        for (auto &m:matches)
        {
            switch (m) {
                case Filter::Stream : {
                    // nothing
                }break;

                case Filter::Meta : {
                    info.segmentClass_ = SegmentClass::Meta;
                } break;

                case Filter::Live : {
                    info.isLiveMeta_ = true;
                    info.segmentClass_ = SegmentClass::Meta;
                    if (suffix[-1].isVersion())
                    {
                        info.version_ = suffix[-1].toVersion();
                        info.hasVersion_ = true;
                    }
                } break;

                case Filter::Latest : {
                    info.isLatestPtr_ = true;
                    info.segmentClass_ = SegmentClass::Pointer;
                    if (suffix[-1].isVersion())
                    {
                        info.version_ = suffix[-1].toVersion();
                        info.hasVersion_ = true;
                    }
                } break;

                case Filter::GopStart : {
                    info.segmentClass_ = SegmentClass::Pointer;
                    info.isGopStart_ = true;
                    info.hasSeqNo_ = true;
                    info.sampleNo_ = suffix[-2].toSequenceNumber();
                } break;

                case Filter::GopEnd : {
                    info.segmentClass_ = SegmentClass::Pointer;
                    info.isGopEnd_ = true;
                    info.hasSeqNo_ = true;
                    info.sampleNo_ = suffix[-2].toSequenceNumber();
                } break;

                case Filter::Frame : {
                    info.hasSeqNo_ = true;
                    info.sampleNo_ = suffix[0].toSequenceNumber();
                } break;

                case Filter::FrameMeta : {
                    info.segmentClass_ = SegmentClass::Meta;
                } break;

                case Filter::Manifest : {
                    info.segmentClass_ = SegmentClass::Manifest;
                } break;

                case Filter::Payload : {
                    info.segmentClass_ = SegmentClass::Data;
                    if (suffix[-1].isSegment())
                    {
                        info.hasSegNo_ = true;
                        info.segNo_ = suffix[-1].toSegment();
                    }
                } break;

                case Filter::Parity : {
                    info.segmentClass_ = SegmentClass::Parity;
                    if (suffix[-1].isSegment())
                    {
                        info.hasSegNo_ = true;
                        info.segNo_ = suffix[-1].toSegment();
                    }
                } break;

                default : {
                    return false;
                }
            }
        }
        return true;
    }

    return false;
}
