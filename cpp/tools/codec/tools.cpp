//
// tools.cpp
//
//  Created by Peter Gusev on 29 October 2018.
//  Copyright 2013-2019 Regents of the University of California
//

#include "tools.hpp"
#include <vpx/vpx_encoder.h>

using namespace std;

namespace ndnrtc
{

int writeFrame(FILE* file, const EncodedFrame& frame)
{
    // write frames size -- 4 bytes
    uint32_t frameSize = frame.length_;
    int res = 0;

    if ((res = fwrite(&frameSize, 1, sizeof(frameSize), file))!= sizeof(frameSize))
    {
        printf("WROTE %d\n", res);
        return 0;
    }

    // write frame data
    if ((res = fwrite((void*)frame.data_, 1, frame.length_, file)) != frameSize)
    {
        printf("WROTE %d\n", res);
        return 0;
    }

    return 1;
}

int readFrame(EncodedFrame& frame, FILE* file)
{
    uint32_t frameSize;
    int res = 0;

    if ((res = fread(&frameSize, 1, sizeof(frameSize), file)) != sizeof(frameSize))
        return 0;

    if (!frame.data_)
        frame.data_ = (uint8_t*)malloc(frameSize*2);
    else if (frame.length_ < frameSize)
        frame.data_ = (uint8_t*)realloc((void*)frame.data_, frameSize);

    frame.length_ = frameSize;
    uint8_t *buf = frame.data_;
    size_t c = frameSize;
    while (c > 0 && (res = fread(buf, 1, c, file)))
    {
        if (res != c && errno != EAGAIN)
            return 0;

        buf += res;
        c -= res;
    }

    return 1;
}

}
