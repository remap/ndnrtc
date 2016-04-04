// 
// frame-io.cpp
//
//  Created by Peter Gusev on 17 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdexcept>
#include <sstream>
#include "frame-io.h"

using namespace std;

RawFrame::RawFrame(unsigned int width, unsigned int height):
bufferSize_(0), width_(width), height_(height){}

//******************************************************************************
ArgbFrame::ArgbFrame(unsigned int width, unsigned int height):
RawFrame(width, height)
{
	unsigned long bufSize = getFrameSizeInBytes();
	setBuffer(bufSize, boost::shared_ptr<uint8_t>(new uint8_t[bufSize]));
}

void ArgbFrame::getFrameResolution(unsigned int& width, unsigned int& height) const
{
	width = width_;
	height = height_;
}

unsigned long ArgbFrame::getFrameSizeInBytes() const
{
	return width_*height_*4;
}

//******************************************************************************
void FileFrameStorage::openFile()
{
    if (path_ == "")
        throw runtime_error("invalid file path provided");

    file_ = openFile_impl(path_);

    if (!file_)
        throw runtime_error("couldn't create sink file at path "+path_);

    fseek(file_, 0, SEEK_END);
    fileSize_ = ftell(file_);
    rewind(file_);
}

void FileFrameStorage::closeFile()
{
    if (file_)
    {
        fclose(file_);
        file_ = nullptr;
    }
}

//******************************************************************************
IFrameSink& FileSink::operator<<(const RawFrame& frame)
{
	fwrite(frame.getBuffer().get(), sizeof(uint8_t), frame.getFrameSizeInBytes(), file_);
}

FILE* FileSink::openFile_impl(string path)
{
    return fopen(path.c_str(), "wb");
}

//******************************************************************************
FileFrameSource::FileFrameSource(const string& path):FileFrameStorage(path),
current_(0), readError_(false)
{ 
    openFile(); 
}

IFrameSource& FileFrameSource::operator>>(RawFrame& frame)
{
    uint8_t *buf = frame.getBuffer().get();
    size_t readBytes = fread(buf, sizeof(uint8_t), frame.getFrameSizeInBytes(), file_);
    current_ = ftell(file_);
    readError_ = (readBytes != frame.getFrameSizeInBytes());

    // {
    //     stringstream msg;
    //     msg << "error trying to read frame of " << frame.getFrameSizeInBytes()
    //         << " bytes from file (read " << readBytes << " bytes): error " 
    //         << ferror(file_) << " eof: " << feof(file_) 
    //         << " current " << current_ << " size " << fileSize_ 
    //         << " ftell " << ftell(file_);
    //     throw runtime_error(msg.str());
    // }
}

bool FileFrameSource::checkSourceForFrame(const std::string& path, 
    const RawFrame& frame)
{
    FILE *f = fopen(path.c_str(), "rb");

    if (!f) 
        return false;
    else
        fseek (f , 0 , SEEK_END);

    long lSize = ftell(f);
    int nFrames = lSize%frame.getFrameSizeInBytes();
    fclose(f);

    return (nFrames == 0);
}

FILE* FileFrameSource::openFile_impl(string path)
{
    return fopen(path.c_str(), "rb");
}
