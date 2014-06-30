//
//  cocoa-renderer.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/20/13
//

#ifndef __ndnrtc__cocoa_renderer__
#define __ndnrtc__cocoa_renderer__

#include "../ndnrtc-common.h"

void createDefaultCocoaMenu();
void *createCocoaRenderWindow(const char *winName, int width, int height);
void setWindowTitle(const char *title, void *window);
void destroyCocoaRenderWindow(void *window);
void arrangeAllWindows();
void* getGLView(void *window);

#endif /* defined(__ndnrtc__cocoa_renderer__) */
