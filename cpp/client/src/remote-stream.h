// 
// remote-stream.h
//
//  Created by Peter Gusev on 07 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __remote_stream_h__
#define __remote_stream_h__

#include <stdlib.h>

class RemoteStream {
public:
	RemoteStream(const std::string& prefix):streamPrefix_(prefix) {}

	std::string getPrefix() { return streamPrefix_; }

private:
	std::string streamPrefix_;
};

#endif