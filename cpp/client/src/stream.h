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
#include "video-source.h"

class Stream {
public:
	Stream(std::string prefix):streamPrefix_(prefix){}
	virtual ~Stream(){}

	std::string getPrefix() const { return streamPrefix_; }

protected:
	std::string streamPrefix_;
};

class RemoteStream : public Stream {
public:
	// RemoteStream(RemoteStream&& rs){}
	RemoteStream(std::string& prefix, boost::shared_ptr<RendererInternal>&& renderer):
		Stream(prefix), renderer_(boost::move(renderer)) {}
	~RemoteStream(){}

	RendererInternal* getRenderer() const { return renderer_.get(); }

private:
	boost::shared_ptr<RendererInternal> renderer_;
	std::string streamPrefix_;
};

class LocalStream : public Stream {
public:
	LocalStream(const LocalStream& ls):Stream(ls.getPrefix()), 
		vsource_(boost::move(ls.getVideoSource())){}
	LocalStream(std::string& prefix, boost::shared_ptr<VideoSource>& vsource):
		Stream(prefix), vsource_(boost::move(vsource)){}
	~LocalStream(){}

	boost::shared_ptr<VideoSource> getVideoSource() const { return vsource_; }
	void stopSource() { if (vsource_.get()) vsource_->stop(); }

private:
	boost::shared_ptr<VideoSource> vsource_;
};

#endif