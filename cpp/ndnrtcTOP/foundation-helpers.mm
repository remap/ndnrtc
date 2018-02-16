//
//  foundation-helpers.m
//  ndnrtcTOP
//
//  Created by Peter Gusev on 2/13/18.
//  Copyright © 2018 UCLA REMAP. All rights reserved.
//

#include "foundation-helpers.h"

#include <Foundation/Foundation.h>
#include <string>

// this dummy class is used only in order to identify plugin bundle
@interface Dummy : NSObject
@end

@implementation Dummy
@end

const char* get_resources_path()
{
    NSBundle *bundle = [NSBundle bundleForClass:[Dummy class]];

    if (bundle == NULL) {
        return NULL;
    }
    
    NSString *respath = [bundle resourcePath];
    if (respath == NULL) {
        return NULL;
    }
    
    return [respath UTF8String];
}
