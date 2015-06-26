//
//  canvas-renderer.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/19/13
//

#ifndef __ndnrtc__canvas_renderer__
#define __ndnrtc__canvas_renderer__

#include <modules/video_render/video_render_impl.h>
#include "ndnrtc-common.h"

namespace ndnrtc
{
    namespace new_api
    {
        class IRenderer
        {
        public:
            virtual ~IRenderer(){}
            virtual int init() = 0;
            virtual int startRendering(const std::string &name = "Renderer") = 0;
            virtual int stopRendering() = 0;
            
            bool
            isRendering()
            { return isRendering_; }
            
        protected:
            bool isRendering_ = false;
        };
    }
}

#endif /* defined(__ndnrtc__canvas_renderer__) */
