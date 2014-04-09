//
//  segmentizer.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "segmentizer.h"
#include "ndnrtc-namespace.h"

using namespace ndnrtc;

int
Segmentizer::segmentize(const ndnrtc::PacketData &packetData,
                        SegmentList &segments, int segmentSize)
{
    if (packetData.isValid())
    {
        segments.clear();
        
        int sliceSize = segmentSize;
        int nSegments = getSegmentsNum(packetData.getLength(), sliceSize);
        int lastSegmentSize = (packetData.getLength()/sliceSize != nSegments)?
        packetData.getLength()-(packetData.getLength()/sliceSize)*sliceSize:
        sliceSize;        
        unsigned char* segmentPointer = packetData.getData();
        
        for (int i = 0; i < nSegments; i++)
        {
            ndnrtc::new_api::FrameBuffer::Slot::Segment segment;
            segment.setPayloadSize((i == nSegments-1)? lastSegmentSize: sliceSize);
            segment.setDataPtr(segmentPointer);
            segmentPointer += sliceSize;
            segments.push_back(segment);
        }
        
        return RESULT_OK;
    }
    
    return RESULT_ERR;
}

int
Segmentizer::desegmentize(const SegmentList &segments, ndnrtc::PacketData **packetData)
{
    if (segments.size())
    {
        unsigned int rawDataLength = 0;
        unsigned char* packetRawData = segments[0].getDataPtr();
        
        for (SegmentList::const_iterator it = segments.begin();
             it != segments.end(); ++it)
            rawDataLength += it->getPayloadSize();
        
        return PacketData::packetFromRaw(rawDataLength, packetRawData, packetData);
    }
    
    return RESULT_ERR;
}


int
Segmentizer::getSegmentsNum(unsigned int dataSize, unsigned int segmentSize)
{
    return ceil((double)dataSize/(double)segmentSize);
}