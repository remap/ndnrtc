// 
// remote-stream.h
//
//  Created by Peter Gusev on 07 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __remote_stream_h__
#define __remote_stream_h__

#include <stdlib.h>
#include "renderer.h"

class RemoteStream {
public:
	// RemoteStream(RemoteStream&& rs){}
	RemoteStream(std::string& prefix, boost::shared_ptr<RendererInternal>&& renderer):
		streamPrefix_(boost::move(prefix)), renderer_(boost::move(renderer)) {}
	~RemoteStream(){}

	std::string getPrefix() { return streamPrefix_; }
	RendererInternal* getRenderer() { return renderer_.get(); }

private:
	boost::shared_ptr<RendererInternal> renderer_;
	std::string streamPrefix_;
};

#endif