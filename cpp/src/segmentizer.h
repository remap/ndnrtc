//
//  segmentizer.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__segmentizer__
#define __ndnrtc__segmentizer__

#include "ndnrtc-common.h"
#include "ndnrtc-object.h"
#include "ndnrtc-utils.h"
#include "frame-data.h"
#include "frame-buffer.h"

namespace ndnrtc
{
    class Segmentizer : public NdnRtcObject
    {
    public:
        typedef std::vector<ndnrtc::new_api::FrameBuffer::Slot::Segment> SegmentList;
        
        /**
         * Slices packet data into segments objects. Each segment object points
         * to the original packetData buffer (continuous memory allocation), 
         * i.e. once packetData will be released, segment objects will be invalid
         */
        static int
        segmentize(const PacketData &packetData,
                   SegmentList &segments, int segmentSize);
        
        /**
         * Assembles appropriate (i.e. audio or video) packet data from the 
         * segment objects. NOTE: Segment objects' buffers should point to the 
         * one continuous memory allocation.
         */
        static int
        desegmentize(const SegmentList &segments, PacketData **packetData);
        
        static int
        getSegmentsNum(unsigned int dataSize, unsigned int segmentSize);
    };
}

#endif /* defined(__ndnrtc__segmentizer__) */
