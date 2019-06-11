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
#include <iterator>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string.hpp>

#include "name-components.hpp"

using namespace std;
using namespace ndnrtc;
using namespace ndn;

const string NameComponents::NameComponentApp = "ndnrtc";
const string NameComponents::NameComponentAudio = "audio";
const string NameComponents::NameComponentVideo = "video";
const string NameComponents::NameComponentMeta = "_meta";
const string NameComponents::NameComponentDelta = "d";
const string NameComponents::NameComponentKey = "k";
const string NameComponents::NameComponentParity = "_parity";
const string NameComponents::NameComponentManifest = "_manifest";

#include <bitset>

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
                prefix.appendVersion(metaVersion_).appendSegment(segNo_);
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
                suffix.appendVersion(metaVersion_);
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
bool extractMeta(const ndn::Name& name, NamespaceInfo& info)
{
    // example: name == %FD%05/%00%00
    if (name.size() >= 1 && name[0].isVersion())
    {
        info.metaVersion_ = name[0].toVersion();
        if (name.size() >= 2)
        {
            info.segNo_ = name[1].toSegment();
            info.hasSegNo_ = true;
        }
        else
            info.hasSegNo_ = false;
        return true;
    }

    return false;
}

bool extractVideoStreamInfo(const ndn::Name& name, NamespaceInfo& info)
{
    if (name.size() == 1)
    {
        info.streamName_ = name[0].toEscapedString();
        return true;
    }

    if (name.size() == 2 && name[1].isTimestamp())
    {
        info.streamName_ = name[0].toEscapedString();
        info.streamTimestamp_ = name[1].toTimestamp();
        return true;
    }

    if (name.size() < 3)
        return false;

    int idx = 0;
    info.streamName_ = name[idx++].toEscapedString();
    info.isMeta_ = (name[idx++] == Name::Component(NameComponents::NameComponentMeta));

    if (info.isMeta_)
    {   // example: name == camera/_meta/%FD%05/%00%00
        info.segmentClass_ = SegmentClass::Meta;
        info.threadName_ = "";
        return extractMeta(name.getSubName(idx), info);
    }
    else
    {   // example: name == camera/%FC%00%00%01c_%27%DE%D6/hi/d/%FE%07/%00%00

        info.class_ = SampleClass::Unknown;
        info.segmentClass_ = SegmentClass::Unknown;
        info.streamTimestamp_ = name[idx-1].toTimestamp();
        info.threadName_ = name[idx++].toEscapedString();
        
        if (name.size() <= idx)
            return true;

        info.isMeta_ = (name[idx++] == Name::Component(NameComponents::NameComponentMeta));

        if (info.isMeta_)
        {   // example: camera/%FC%00%00%01c_%27%DE%D6/hi/_meta/%FD%05/%00%00
            info.segmentClass_ = SegmentClass::Meta;
            if(name.size() > idx-1 && extractMeta(name.getSubName(idx), info))
                return true;
            return true;
        }

        if (name[idx-1] == Name::Component(NameComponents::NameComponentDelta) || 
            name[idx-1] == Name::Component(NameComponents::NameComponentKey))
        {
            info.isDelta_ = (name[idx-1] == Name::Component(NameComponents::NameComponentDelta));
            info.class_ = (info.isDelta_ ? SampleClass::Delta : SampleClass::Key);

            try{
                if (name.size() > idx)
                    info.sampleNo_ = (PacketNumber)name[idx++].toSequenceNumber();
                else
                {
                    info.hasSeqNo_ = false;
                    return true;
                }
            
                info.hasSeqNo_ = true;
                if (name.size() > idx)
                {
                    info.isParity_ = (name[idx] == Name::Component(NameComponents::NameComponentParity));
                    info.hasSegNo_ = true;

                    if (info.isParity_ && name.size() > idx+1)
                    {
                        info.segmentClass_ = SegmentClass::Parity;
                        info.segNo_ = name[idx+1].toSegment();
                        return true;
                    }
                    else 
                    {
                        if (info.isParity_) 
                            return false;
                        else
                        {
                            if (name[idx] == Name::Component(NameComponents::NameComponentManifest))
                                info.segmentClass_ = SegmentClass::Manifest;
                            else
                            {
                                info.segmentClass_ = SegmentClass::Data;
                                info.segNo_ = name[idx].toSegment();
                            }
                        }
                        return true;
                    }
                }
                else
                {
                    info.segmentClass_ = SegmentClass::Unknown;
                    info.hasSegNo_ = false;
                    return true;
                }
            }
            catch (std::runtime_error& e)
            {
                return false;
            }
        }
    }

    return false;
}

bool extractAudioStreamInfo(const ndn::Name& name, NamespaceInfo& info)
{
    if (name.size() == 1)
    {
        info.streamName_ = name[0].toEscapedString();
        return true;
    }

    if (name.size() == 2 && name[1].isTimestamp())
    {
        info.streamName_ = name[0].toEscapedString();
        info.streamTimestamp_ = name[1].toTimestamp();
        return true;
    }

    if (name.size() < 2)
        return false;

    int idx = 0;
    info.streamName_ = name[idx++].toEscapedString();
    info.isMeta_ = (name[idx++] == Name::Component(NameComponents::NameComponentMeta));
    
    if (info.isMeta_)
    {
        info.segmentClass_ = SegmentClass::Meta;
        if (name.size() < idx+1)
            return false;

        info.threadName_ = "";
        return extractMeta(name.getSubName(idx), info);;
    }
    else
    {
        info.class_ = SampleClass::Unknown;
        info.segmentClass_ = SegmentClass::Unknown;
        info.streamTimestamp_ = name[idx-1].toTimestamp();
        info.threadName_ = name[idx++].toEscapedString();

        if (name.size() == 3)
        {
            info.hasSeqNo_ = false;
            return true;
        }

        info.isMeta_ = (name[idx] == Name::Component(NameComponents::NameComponentMeta));

        if (info.isMeta_)
        { 
            info.segmentClass_ = SegmentClass::Meta;
            if (name.size() > idx+1 && extractMeta(name.getSubName(idx+1), info))
                return true;
            return true;
        }

        info.isDelta_ = true;
        info.class_ = SampleClass::Delta;

        try
        {
            if (name.size() > 3)
                info.sampleNo_ = (PacketNumber)name[idx++].toSequenceNumber();
            
            info.hasSeqNo_ = true;
            if (name.size() > idx)
            {
                if (name[idx] == Name::Component(NameComponents::NameComponentManifest))
                    info.segmentClass_ = SegmentClass::Manifest;
                else
                {
                    info.hasSegNo_ = true;
                    info.segmentClass_ = SegmentClass::Data;
                    info.segNo_ = name[idx].toSegment();
                }
                return true;
            }
            else
            {
                info.hasSegNo_ = false;
                return true;
            }
        }
        catch (std::runtime_error& e)
        {
            return false;
        }
    }

    return false;
}

bool
NameComponents::extractInfo(const ndn::Name& name, NamespaceInfo& info)
{
    bool goodName = false;
    static Name ndnrtcSubName(NameComponents::NameComponentApp);
    Name subName;
    int i;

    for (i = name.size()-2; i > 0 && !goodName; --i)
    {
        subName = name.getSubName(i);
        goodName = ndnrtcSubName.match(subName);
    }

    if (goodName)
    {
        info.basePrefix_ = name.getSubName(0, i+1);

        if ((goodName = subName[1].isVersion()))
        {
            info.apiVersion_ = subName[1].toVersion();

            if (subName.size() > 2 &&
                (goodName = (subName[2] == Name::Component(NameComponents::NameComponentAudio) ||
                            subName[2] == Name::Component(NameComponents::NameComponentVideo)))  )
            {
                info.streamType_ = (subName[2] == Name::Component(NameComponents::NameComponentAudio) ? 
                                MediaStreamParams::MediaStreamType::MediaStreamTypeAudio : 
                                MediaStreamParams::MediaStreamType::MediaStreamTypeVideo );

                if (info.streamType_ == MediaStreamParams::MediaStreamType::MediaStreamTypeAudio)
                    return extractAudioStreamInfo(subName.getSubName(3), info);
                else
                    return extractVideoStreamInfo(subName.getSubName(3), info);
            }
        }
    }

    return false;
}
