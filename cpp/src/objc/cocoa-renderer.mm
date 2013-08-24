//
//  cocoa-renderer.mm
//  ndnrtc
//
//  Created by Peter Gusev on 8/20/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#import "cocoa-renderer.h"
#include "../webrtc.h"

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>
#import <QTKit/QTKit.h>
#include <sys/time.h>
#import <modules/video_render/mac/cocoa_render_view.h>

//#include "common_types.h"
//#import "webrtc/modules/video_render//mac/cocoa_render_view.h"
//#include "module_common_types.h"
//#include "process_thread.h"
//#include "tick_util.h"
//#include "trace.h"
//#include "video_render_defines.h"

//#include "video_render.h"

using namespace webrtc;

int WebRtcCreateWindow(CocoaRenderView*& cocoaRenderer, int winNum, int width, int height)
{
    // In Cocoa, rendering is not done directly to a window like in Windows and Linux.
    // It is rendererd to a Subclass of NSOpenGLView
    
    // create cocoa container window
    NSRect outWindowFrame = NSMakeRect(200, 800, width + 20, height + 20);
    NSWindow* outWindow = [[NSWindow alloc] initWithContentRect:outWindowFrame
                                                      styleMask:NSTitledWindowMask
                                                        backing:NSBackingStoreBuffered
                                                          defer:NO];
    [outWindow orderOut:nil];
    [outWindow setTitle:@"Cocoa Renderer"];
    [outWindow setBackgroundColor:[NSColor blueColor]];
    
    // create renderer and attach to window
    NSRect cocoaRendererFrame = NSMakeRect(10, 10, width, height);
    cocoaRenderer = [[CocoaRenderView alloc] initWithFrame:cocoaRendererFrame];
    [[outWindow contentView] addSubview:(NSView*)cocoaRenderer];
    
    [outWindow makeKeyAndOrderFront:NSApp];
    
    return 0;
}

void *createCocoaRenderWindow(int winNum, int width, int height)
{
    CocoaRenderView* window;
    WebRtcCreateWindow(window, winNum, width, height);
    
    return (void*)window;
}