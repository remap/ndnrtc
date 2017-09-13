// 
// frame-io.h
//
//  Created by Peter Gusev on 17 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __frame_io_h__
#define __frame_io_h__

#include <stdlib.h>
#include <boost/shared_ptr.hpp>

//******************************************************************************
class RawFrame {
public:
	RawFrame(unsigned int width, unsigned int height);
	virtual ~RawFrame(){}

	virtual unsigned long getFrameSizeInBytes() const = 0;
	unsigned int getWidth() const { return width_; }
	unsigned int getHeight() const { return height_; }
	virtual void getFrameResolution(unsigned int& width, unsigned int& height) const = 0;
	boost::shared_ptr<uint8_t> getBuffer() const { return buffer_; }

protected:
	unsigned int width_, height_;

	virtual void setBuffer(const long& bufSize, boost::shared_ptr<uint8_t> buf) 
	{ bufferSize_ = bufSize; buffer_ = buf; }

private:
	long bufferSize_;
	boost::shared_ptr<uint8_t> buffer_;
};

//******************************************************************************
class ArgbFrame : public RawFrame {
public:
	ArgbFrame(unsigned int width, unsigned int height);

	virtual unsigned long getFrameSizeInBytes() const;
	void getFrameResolution(unsigned int& width, unsigned int& height) const;
};

//******************************************************************************
class FileFrameStorage {
public:
	FileFrameStorage(std::string path):file_(nullptr), path_(path), fileSize_(0) { }
	virtual ~FileFrameStorage(){ closeFile(); }

	std::string getPath() { return path_; }
	unsigned long getSize() { return fileSize_; }
protected:
	FILE *file_;
	std::string path_;
	unsigned long fileSize_;

	void openFile();
	virtual FILE* openFile_impl(std::string path) = 0;
private:
	void closeFile();
};

//******************************************************************************
class IFrameSink {
public:
	virtual IFrameSink& operator<<(const RawFrame& frame) = 0;
	virtual std::string getName() = 0;
	virtual bool isBusy() = 0;
};

/**
 * File-based frame sink
 */
class FileSink : public IFrameSink, public FileFrameStorage {
public:
	FileSink(const std::string& path):FileFrameStorage(path){ openFile(); }
	IFrameSink& operator<<(const RawFrame& frame);
	std::string getName() { return path_; }

	// TODO: whether file writing can be busy, probably, need to be tested
	bool isBusy() { return false; }

private:
    FILE* openFile_impl(std::string path);
};

/**
 * Non-blocking pipe-based frame sink
 * - will create pipe if it does not exist
 * - in order for pipe to be opened for writing, it should be opened for reading 
 *		by another process; until then, sink will skip writing frames. 
 */
class PipeSink : public IFrameSink {
public:
	PipeSink(const std::string& path);
	~PipeSink();

	IFrameSink& operator<<(const RawFrame& frame);
	std::string getName() { return pipePath_; }

	bool isLastWriteSuccessful() { return isLastWriteSuccessful_; }
	bool isBusy() { return isWriting_; }

private:
	std::string pipePath_;
	int pipe_;
	std::atomic<bool> isLastWriteSuccessful_, isWriting_;

	void createPipe(const std::string& path);
	void openPipe(const std::string& path);
};

//******************************************************************************
class IFrameSource {
public:
	virtual IFrameSource& operator>>(RawFrame& frame) noexcept = 0;
};

class FileFrameSource : public IFrameSource, public FileFrameStorage {
public:
	FileFrameSource(const std::string& path);

	IFrameSource& operator>>(RawFrame& frame) noexcept;
	/**
	 * NOTE: will always return 'true' before any read call.
	 * proper use to read frames in a loop:
	 * do {
	 *		if (source.isEof()) source.rewind();
	 *		source >> frame;
	 * } while(source.isEof() && !source.isError());
	 */
	bool isEof(){ return (feof(file_) != 0); }
	bool isError() { return readError_; }
	void rewind(){ ::rewind(file_); current_ = 0; }

	static bool checkSourceForFrame(const std::string& path, const RawFrame& frame);
private:
	FILE* openFile_impl(std::string path);
	unsigned long current_;
	bool readError_;
};

#endif
