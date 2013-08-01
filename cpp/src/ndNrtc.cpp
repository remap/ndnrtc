//
//  ndNrtc.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 7/29/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#include "ndNrtc.h"


NS_IMPL_ISUPPORTS1(ndNrtc, INrtc)

ndNrtc::ndNrtc()
{
  /* member initializers and constructor code */
}

ndNrtc::~ndNrtc()
{
  /* destructor code */
}

/* long Add (in long a, in long b); */
NS_IMETHODIMP ndNrtc::Test(int32_t a, int32_t b, int32_t *_retval)
{
	*_retval = a+b;
    return NS_OK;
}
