//
//  cocoa-renderer.mm
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 8/20/13
//


#import "cocoa-renderer.h"
#include "../webrtc.h"

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>
#import <QTKit/QTKit.h>
#include <sys/time.h>
#import <modules/video_render/mac/cocoa_render_view.h>

static int TotalWindows = 0;

using namespace webrtc;

int WebRtcCreateWindow(CocoaRenderView*& cocoaRenderer, const char *winName, int width, int height)
{
    // In Cocoa, rendering is not done directly to a window like in Windows and Linux.
    // It is rendererd to a Subclass of NSOpenGLView
    
    // create cocoa container window
    NSRect outWindowFrame = NSMakeRect(0/*+TotalWindows*40*/, 2000+TotalWindows*80, width , height );
    NSWindow* outWindow = [[NSWindow alloc] initWithContentRect:outWindowFrame
                                                      styleMask:NSTitledWindowMask|NSClosableWindowMask|
                           NSMiniaturizableWindowMask|NSResizableWindowMask
                                                        backing:NSBackingStoreBuffered
                                                          defer:NO];
    [outWindow setTitle:[NSString stringWithCString:winName encoding:NSUTF8StringEncoding]];
    [outWindow setBackgroundColor:[NSColor blackColor]];
    [outWindow setReleasedWhenClosed:YES];
    //    [outWindow setLevel:NSModalPanelWindowLevel];
    
    // create renderer and attach to window
    NSRect cocoaRendererFrame = NSMakeRect(0, 0, width, height);
    cocoaRenderer = [[CocoaRenderView alloc] initWithFrame:cocoaRendererFrame];
    [[outWindow contentView] addSubview:(NSView*)cocoaRenderer];
    
    [outWindow setIsVisible:YES];
    [outWindow makeMainWindow];
    [outWindow makeKeyAndOrderFront:NSApp];
    [outWindow display];
    
    TotalWindows++;
    return 0;
}

void createDefaultCocoaMenu()
{
    NSApplication* app = [NSApplication sharedApplication];
    NSMenu* appMenu = [[NSMenu alloc] initWithTitle:@"My App"];
    
    [app setMainMenu:appMenu];
    [NSApp activateIgnoringOtherApps:YES];
}

void *createCocoaRenderWindow(const char *winName, int width, int height)
{
    CocoaRenderView* window;
    
    try{
        WebRtcCreateWindow(window, winName, width, height);
    }
    catch(NSException *e)
    {
        printf("ndnrtc exception: %s - %s",
               [e.name cStringUsingEncoding:NSUTF8StringEncoding],
               [e.reason cStringUsingEncoding:NSUTF8StringEncoding]);
        return NULL;
    }
    
    return (void*)window;
}

void destroyCocoaRenderWindow(void *window)
{
    CocoaRenderView *cocoaView = (CocoaRenderView*)window;
    
    // get view's window and destroy
    NSWindow *mainWin = cocoaView.window;
    
    [mainWin close];
    TotalWindows--;
}

void setWindowTitle(const char *title, void *window)
{
    CocoaRenderView *cocoaView = (CocoaRenderView*)window;
    
    // get view's window and destroy
    NSWindow *mainWin = cocoaView.window;
    
    [mainWin setTitle:[NSString stringWithCString:title encoding:NSUTF8StringEncoding]];
}

void* getGLView(void *window)
{
    return (void*)[[((CocoaRenderView*)window) nsOpenGLContext] view];
}