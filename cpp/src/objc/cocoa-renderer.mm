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

static NSMutableDictionary *Windows = nil;
static NSNumber *LocalViewWindow = nil;

using namespace webrtc;

void setupLocalView(NSWindow* win);
NSRect resizeProportionally(NSRect frame, double scale);

int WebRtcCreateWindow(CocoaRenderView*& cocoaRenderer, const char *winName, int width, int height)
{
    // In Cocoa, rendering is not done directly to a window like in Windows and Linux.
    // It is rendererd to a Subclass of NSOpenGLView
    [NSApp activateIgnoringOtherApps:YES];
    
    // create cocoa container window
    NSRect outWindowFrame = NSMakeRect(0, 0, width , height );

    NSWindow* outWindow = [[NSWindow alloc] initWithContentRect:outWindowFrame
                                                      styleMask:NSTitledWindowMask|
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
    ((NSView*)cocoaRenderer).autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [outWindow display];
    [outWindow setIsVisible:YES];
    [outWindow makeMainWindow];
    [outWindow makeKeyAndOrderFront:NSApp];
    [outWindow setOrderedIndex:0];
    
    if (!Windows)
        Windows = [NSMutableDictionary dictionary];
    
    [Windows setObject:outWindow forKey:[NSNumber numberWithInt:[cocoaRenderer hash]]];
    arrangeAllWindows();    
    
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
static int counter = 0;
void destroyCocoaRenderWindow(void *window)
{
    CocoaRenderView *cocoaView = (CocoaRenderView*)window;
    
    // get view's window and destroy
    NSWindow *mainWin = cocoaView.window;
    
    [Windows removeObjectForKey:[NSNumber numberWithInt:[cocoaView hash]]];
    
    [mainWin close];
    
    arrangeAllWindows();
    counter++;
}

void setWindowTitle(const char *title, void *window)
{
    CocoaRenderView *cocoaView = (CocoaRenderView*)window;
    
    // get view's window and destroy
    NSWindow *mainWin = cocoaView.window;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    [mainWin setTitle:[NSString stringWithCString:title encoding:NSUTF8StringEncoding]];
    
    if ([mainWin.title isEqualToString:@"Local render"])
    {
        LocalViewWindow = [NSNumber numberWithInt:cocoaView.hash];
        arrangeAllWindows();
    }
    
    [pool drain];
}

void* getGLView(void *window)
{
    return (void*)[[((CocoaRenderView*)window) nsOpenGLContext] view];
}

void arrangeAllWindows()
{
    CGFloat widthLeft = [NSScreen mainScreen].frame.size.width;
    CGFloat heightLeft = [NSScreen mainScreen].frame.size.height;

    int nWindows = (LocalViewWindow)?Windows.count-1:Windows.count;
    CGFloat windowWidth = widthLeft/(CGFloat)nWindows;
    
    for (NSNumber *hash in Windows.allKeys)
    {
        NSWindow *window = [Windows objectForKey:hash];

        if ([hash isEqual:LocalViewWindow])
            setupLocalView(window);
        else
        {
            NSRect frame = [window frame];
            
            if (nWindows > 2)
                frame = resizeProportionally(frame, (double)( windowWidth/frame.size.width));
            
            frame.origin.x = [NSScreen mainScreen].frame.size.width - widthLeft;
            frame.origin.y = [NSScreen mainScreen].frame.size.height - heightLeft;
            widthLeft -= frame.size.width;
            
            [window setFrame:frame display:YES];
            [window makeKeyAndOrderFront:NSApp];
        }
    }
}


void setupLocalView(NSWindow* win)
{
    NSRect frame = win.frame;
    CGFloat ratio = frame.size.height/frame.size.width;
    
    frame.size.width = 320;
    frame.size.height = frame.size.width*ratio;
    frame.origin.x = [NSScreen mainScreen].frame.size.width-frame.size.width;
    frame.origin.y = [NSScreen mainScreen].frame.size.height-frame.size.height;
    
    [win setFrame:frame display:YES];
    [win makeKeyAndOrderFront:NSApp];
}

NSRect resizeProportionally(NSRect frame, double scale)
{
    NSRect rect;
    
    rect.size.width = frame.size.width*scale;
    rect.size.height = frame.size.height*scale;
    
    return rect;
}

