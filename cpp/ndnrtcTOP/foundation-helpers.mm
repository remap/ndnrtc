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

// Concatenate preprocessor tokens A and B without expanding macro definitions
// (however, if invoked from a macro, macro arguments are expanded).
#define PPCAT_NX(A, B) A ## B
// concatenate preprocessor tokens A and B after macro-expanding them.
#define PPCAT(A, B) PPCAT_NX(A, B)

#define DUMMY_CLASS PPCAT(Dummy_, PRODUCT_NAME)

// this dummy class is used only in order to identify plugin bundle
// the name of it is composed using unique PRODUCT_NAME variable - it
// prevents having identical obj-c classes when two modules (ndnrtcIn
// and ndnrtcOut) will be loaded simultaneously
@interface DUMMY_CLASS : NSObject
@end

@implementation DUMMY_CLASS
@end

const char* get_resources_path()
{
    NSBundle *bundle = [NSBundle bundleForClass:[DUMMY_CLASS class]];

    if (bundle == NULL) {
        return NULL;
    }
    
    NSString *respath = [bundle resourcePath];
    if (respath == NULL) {
        return NULL;
    }
    
    return [respath UTF8String];
}
