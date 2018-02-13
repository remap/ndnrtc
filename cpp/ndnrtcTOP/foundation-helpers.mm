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

const char* get_resources_path()
{
    NSBundle *bundle;
    
    bundle = [NSBundle bundleWithIdentifier: @"edu.ucla.remap.ndnrtcTOP"];
    if (bundle == NULL) {
        return NULL;
    }
    
    NSString *respath = [bundle resourcePath];
    if (respath == NULL) {
        return NULL;
    }
    
    return [respath UTF8String];
}
