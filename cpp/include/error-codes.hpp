//
//  error-codes.hpp
//  libndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef libndnrtc_error_codes_h
#define libndnrtc_error_codes_h

// generated when NDN-CPP library throws an exception
#define NRTC_ERR_LIBERROR           (-1000)

// when library receives SIGPIPE signal
#define NRTC_ERR_SIGPIPE            (-1100)

// record was not found
#define NRTC_ERR_NOT_FOUND          (-2000)
// record already exists
#define NRTC_ERR_ALREADY_EXISTS     (-2001)
// malformed data received
#define NRTC_ERR_MALFORMED          (-2002)

// parameters validation error
#define NRTC_ERR_VALIDATION         (-2003)

// general fatal error
#define NRTC_ERR_FATAL         (-9999)

#endif
