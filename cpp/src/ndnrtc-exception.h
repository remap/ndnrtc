// 
// 	ndnrtc-exception.h
//  ndnrtc
//
//  Copyright 2015 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef ndnrtc_exception_h
#define ndnrtc_exception_h

#include <boost/exception/all.hpp>
#include <exception>


namespace ndnrtc {
	struct ndnrtc_exception : public boost::exception, public std::runtime_error
	{
	public:
	    ndnrtc_exception(const char* what_arg):runtime_error(what_arg){}
	    ndnrtc_exception(const std::string& what_arg):runtime_error(what_arg){}
	};
}

#endif
