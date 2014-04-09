//
//  ndnrtc-debug.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 2/14/14.
//

#ifndef __ndnrtc__ndnrtc_debug__
#define __ndnrtc__ndnrtc_debug__

#include <stdlib.h>
#include <ios>
#include <sstream>
#include <iostream>
#include <string.h>
#include <iomanip>

namespace ndnrtc {
    namespace test {
        /**
         * Debugging helper functions
         */
        
        /**
         * Dumps the bytes of data into a string
         * @param nBytes The number of bytes to be dumped starting from 0 byte
         * @param data Data that need to be dumped
         */
        template <int nBytes, std::ios_base::fmtflags base = std::ios_base::hex>
        static std::string dump(void *data)
        {
            std::stringstream dumpStream;
            for (int i = 0; i < nBytes; i++)
            {
                unsigned char byte1 = ((unsigned char*)data)[i];
                
                dumpStream << std::hex << std::setfill('0');
                dumpStream << std::uppercase << std::setw(2)
                << static_cast<int>(byte1) << " ";
            }
            
            return dumpStream.str();
        }
    }
}

#endif /* defined(__ndnrtc__ndnrtc_debug__) */
