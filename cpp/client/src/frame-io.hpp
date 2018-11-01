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
#include <atomic>
#include <ndnrtc/interfaces.hpp>

//******************************************************************************
class RawFrame
{
  public:
    RawFrame(unsigned int width, unsigned int height);
    virtual ~RawFrame();

    virtual unsigned long getFrameSizeInBytes() const = 0;
    unsigned int getWidth() const { return width_; }
    unsigned int getHeight() const { return height_; }
    virtual void getFrameResolution(unsigned int &width, unsigned int &height) const = 0;
    boost::shared_ptr<uint8_t> getBuffer() const { return buffer_; }

    void setFrameInfo(const ndnrtc::FrameInfo& frameInfo);
    const ndnrtc::FrameInfo& getFrameInfo() const { return frameInfo_; };

  protected:
    unsigned int width_, height_;
    ndnrtc::FrameInfo frameInfo_;

    virtual void setBuffer(const long &bufSize, boost::shared_ptr<uint8_t> buf)
    {
        bufferSize_ = bufSize;
        buffer_ = buf;
    }

  private:
    long bufferSize_;
    boost::shared_ptr<uint8_t> buffer_;
};

//******************************************************************************
class ArgbFrame : public RawFrame
{
  public:
    ArgbFrame(unsigned int width, unsigned int height);

    virtual unsigned long getFrameSizeInBytes() const;
    void getFrameResolution(unsigned int &width, unsigned int &height) const;
};

//******************************************************************************
class FileFrameStorage
{
  public:
    FileFrameStorage(std::string path) : file_(nullptr), path_(path), fileSize_(0) {}
    virtual ~FileFrameStorage() { closeFile(); }

    std::string getPath() { return path_; }
    unsigned long getSize() { return fileSize_; }

  protected:
    FILE *file_;
    std::string path_;
    unsigned long fileSize_;

    void openFile();
    virtual FILE *openFile_impl(std::string path) = 0;

  private:
    void closeFile();
};

//******************************************************************************
class IFrameSink
{
  public:
    virtual IFrameSink &operator<<(const RawFrame &frame) = 0;
    virtual std::string getName() = 0;
    virtual bool isBusy() = 0;
    virtual bool isLastWriteSuccessful() = 0;
    virtual void setWriteFrameInfo(bool) = 0;
    virtual bool isWritingFrameInfo() const = 0;
};

/**
 * File-based frame sink
 */
class FileSink : public IFrameSink, public FileFrameStorage
{
  public:
    FileSink(const std::string &path) : FileFrameStorage(path), writeFrameInfo_(false) { openFile(); }
    IFrameSink &operator<<(const RawFrame &frame);
    std::string getName() { return path_; }

    bool isLastWriteSuccessful() { return isLastWriteSuccessful_; }
    // TODO: whether file writing can be busy, probably, need to be tested
    bool isBusy() { return false; }
    void setWriteFrameInfo(bool b) { writeFrameInfo_ = b; }
    bool isWritingFrameInfo() const { return writeFrameInfo_; }

  private:
    FILE *openFile_impl(std::string path);
    bool writeFrameInfo_;
    std::atomic<bool> isLastWriteSuccessful_;
};

/**
 * Non-blocking pipe-based frame sink
 * - will create pipe if it does not exist
 * - in order for pipe to be opened for writing, it should be opened for reading 
 *		by another process; until then, sink will skip writing frames. 
 */
class PipeSink : public IFrameSink
{
  public:
    PipeSink(const std::string &path);
    ~PipeSink();

    IFrameSink &operator<<(const RawFrame &frame);
    std::string getName() { return pipePath_; }

    bool isLastWriteSuccessful() { return isLastWriteSuccessful_; }
    bool isBusy() { return isWriting_; }
    void setWriteFrameInfo(bool b) { writeFrameInfo_ = b; }
    bool isWritingFrameInfo() const { return writeFrameInfo_; }

  private:
    std::string pipePath_;
    int pipe_;
    bool writeFrameInfo_;
    std::atomic<bool> isLastWriteSuccessful_, isWriting_;

    void createPipe(const std::string &path);
    void openPipe(const std::string &path);
};

#ifdef HAVE_LIBNANOMSG
/**
 * nanomsg sink (unix socket)
 */
class NanoMsgSink : public IFrameSink
{
  public:
    NanoMsgSink(const std::string &handle);
    ~NanoMsgSink();

    virtual IFrameSink &operator<<(const RawFrame &frame);
    virtual std::string getName() { return handle_; }
    virtual bool isBusy() { return false; }
    bool isLastWriteSuccessful() { return isLastWriteSuccessful_; }
    void setWriteFrameInfo(bool b) { writeFrameInfo_ = b; }
    bool isWritingFrameInfo() const { return writeFrameInfo_; }

  private:
    std::string handle_;
    bool writeFrameInfo_, isLastWriteSuccessful_;
    int nnSocket_;
};
#endif

//******************************************************************************
class IFrameSource
{
  public:
    virtual IFrameSource &operator>>(RawFrame &frame) noexcept = 0;
    virtual std::string getName() const = 0;
    virtual bool isEof() const = 0;
    virtual void rewind() = 0;
    virtual bool isError() const = 0;
    virtual std::string getErrorMsg() const = 0;
};

class FileFrameSource : public IFrameSource, public FileFrameStorage
{
  public:
    FileFrameSource(const std::string &path);

    IFrameSource &operator>>(RawFrame &frame) noexcept;
    std::string getName() const { return path_; }

    /**
     * NOTE: will always return 'true' before any read call.
     * proper use to read frames in a loop:
     * do {
     *		if (source.isEof()) source.rewind();
     *		source >> frame;
     * } while(source.isEof() && !source.isError());
     */
    bool isEof() const { return (feof(file_) != 0); }
    bool isError() const { return readError_; }
    std::string getErrorMsg() const { return errorMsg_; }
    void rewind()
    {
        ::rewind(file_);
        current_ = 0;
    }

    static bool checkSourceForFrame(const std::string &path, const RawFrame &frame);

  private:
    FILE *openFile_impl(std::string path);
    unsigned long current_;
    bool readError_;
    std::string errorMsg_;
};

class PipeFrameSource : public IFrameSource {
  public:
    PipeFrameSource(const std::string &path);
    ~PipeFrameSource();

    IFrameSource &operator>>(RawFrame &frame) noexcept;
    std::string getName() const { return pipePath_; }
    bool isError() const { return readError_; }
    std::string getErrorMsg() const { return errorMsg_; }
    bool isEof() const { return false; }
    void rewind() { /*do nothing*/ }

  private:
    std::string pipePath_;
    int pipe_;
    bool readError_;
    std::string errorMsg_;

    int createReadPipe();
    void reopenReadPipe();
    void closePipe();
};

#endif
