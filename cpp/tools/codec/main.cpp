//
// main.cpp
//
//  Created by Peter Gusev on 23 February 2019.
//  Copyright 2013-2019 Regents of the University of California
//

#include <iostream>
#include <iomanip>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <execinfo.h>
#include <thread>

#include <boost/asio.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <mutex>
//

#include "../../contrib/docopt/docopt.h"
#include "../../include/simple-log.hpp"
#include "../../src/video-codec.hpp"
#include "tools.hpp"

// **** tools
#include <vpx/vpx_encoder.h>
// ****

#define TOOL_NAME "NdnRtc Codec"

static const char USAGE[] =
R"(NdnRtc Codec.
    This app is for testing encoder/decoder of NDN-RTC.
    In encoder mode, it takes raw video (yuv420 as default) as an input and
    encodes it according to the settings. Outputs encoded video in a simple
    format: frame by frame, where first 4 bytes tell the size of the compressed
    data that follows. In decoder mode, takes encoded video as an
    input and outputs decoded raw frames.

    Usage:
      ndnrtc-codec encode <in_file> ( - | <out_file> ) --size=<WxH> --bitrate=<bitrate> [--gop=<gop>] [--fps=<fps>] [--no-drop] [--i420] [--csv=<csv_file>] [--verbose]
      ndnrtc-codec decode (<in_file> | - ) ( - | <out_file>) [--csv=<csv_file>] [--verbose]

    Arguments:
      <in_file>     For "encode" mode: input file of raw video or stdin.
                    For "decode" mode: IVF encoded video file.
      <out_file>    For "encode" mode: output file for encded IVF video.
                    For "decode" mode: output file for raw video.

    Options:
      --size=<WxH>              Size of incoming video frame; must be in "WIDTHxHEIGHT" format
      --bitrate=<bitrate>       Target encoding bitrate in kbps
      --gop=<gop>               Target group of picture size inframes [default: 30]
      --fps=<fps>               Target FPS [default: 30]
      --no-drop                 Tells encoder not to drop frames
      --i420                    I420 raw frame format
      --csv=<csv_file>          CSV file to save statistics to
      -v --verbose              Verbose (debug) output
)";

using namespace std;
using namespace ndnrtc;

static bool mustExit = false;

void handler(int sig)
{
    void *array[10];
    size_t size;

    if (sig == SIGABRT || sig == SIGSEGV)
    {
        fprintf(stderr, "Received signal %d:\n", sig);
        // get void*'s for all entries on the stack
        size = backtrace(array, 10);
        // print out all the frames to stderr
        backtrace_symbols_fd(array, size, STDERR_FILENO);
        exit(1);
    }
    else
        mustExit = true;
}

int getSize(string sizeStr, uint16_t& w, uint16_t& h);
void runEncoder(VideoCodec& codec, string inFile, string outFile, string statFile);
void runDecoder(VideoCodec& codec, string inFile, string outFile, string statFile);

int main(int argc, char **argv)
{
    signal(SIGABRT, handler);
    signal(SIGSEGV, handler);
    signal(SIGINT, handler);
    signal(SIGUSR1, handler);

    ndnlog::new_api::Logger::initAsyncLogging();
    std::map<std::string, docopt::value> args =docopt::docopt(USAGE,
                         { argv + 1, argv + argc },
                         true,               // show help if requested
                         (string(TOOL_NAME)+string(PACKAGE_VERSION)).c_str());  // version string

    // for(auto const& arg : args) {
    //     std::cout << arg.first << " " <<  arg.second << std::endl;
    // }

    ndnlog::new_api::Logger::getLogger("").setLogLevel(args["--verbose"].asBool() ? ndnlog::NdnLoggerDetailLevelAll : ndnlog::NdnLoggerDetailLevelDefault);

    int err = 0;
    boost::asio::io_service io;
    shared_ptr<boost::asio::io_service::work> work(make_shared<boost::asio::io_service::work>(io));
    thread t([&io, &err]() {
        try
        {
            io.run();
        }
        catch (std::exception &e)
        {
            LogError("") << "Caught exception while running: " << e.what() << std::endl;
            err = 1;
        }
    });

    try
    {
        VideoCodec codec;
        CodecSettings codecSettings;
        codecSettings.numCores_ = thread::hardware_concurrency();
        codecSettings.rowMt_ = true;

        if (args["encode"].asBool())
        {
            LogDebug("") << "initializing encoder" << endl;

            if (getSize(args["--size"].asString(),
                        codecSettings.spec_.encoder_.width_,
                        codecSettings.spec_.encoder_.height_))
            {
                codecSettings.spec_.encoder_.bitrate_ = args["--bitrate"].asLong();
                codecSettings.spec_.encoder_.gop_ = args["--gop"].asLong();
                codecSettings.spec_.encoder_.fps_ = args["--fps"].asLong();
                codecSettings.spec_.encoder_.dropFrames_ = !args["--no-drop"].asBool();

                LogDebug("") << "encoder settings:"
                             << "\n\twidth " << codecSettings.spec_.encoder_.width_
                             << " height " << codecSettings.spec_.encoder_.height_
                             << "\n\tbitrate " << codecSettings.spec_.encoder_.bitrate_
                             << "\n\tgop " << codecSettings.spec_.encoder_.gop_
                             << " fps " << codecSettings.spec_.encoder_.fps_
                             << " drop frames " << codecSettings.spec_.encoder_.dropFrames_
                             << endl;

                codec.initEncoder(codecSettings);
                runEncoder(codec, args["<in_file>"].asString(), args["-"].asLong() ? "-" : args["<out_file>"].asString(),
                           args["--csv"] ? args["--csv"].asString() : "" );
            }
            else
            {
                LogError("") << "bad frame size specified: " << args["--size"] << endl;
                err = 1;
            }

        }
        else if (args["decode"].asBool())
        {
            LogDebug("") << "initializing decoder" << endl;

            codec.initDecoder(codecSettings);
            runDecoder(codec, args["<in_file>"].asString(), args["-"].asLong() ? "-" : args["<out_file>"].asString(),
                       args["--csv"] ? args["--csv"].asString() : "");
        }
    }
    catch (std::exception &e)
    {
        LogError("") << "caught exception: " << e.what() << endl;
    }

    LogInfo("") << "Shutting down gracefully..." << endl;

    work.reset();
    t.join();
    io.stop();

    LogInfo("") << "done" << endl;
}

void printStats(const VideoCodec::Stats& codecStats, string statFile);

void runEncoder(VideoCodec& codec, string inFile, string outFile, string statFile)
{
    // open file
    FILE *f, *fOut;

    if (!(f = fopen(inFile.c_str(), "rb")))
        throw runtime_error("failed to open video file for reading");

    bool noStats = false;
    if (outFile == "-")
    {
        ndnlog::new_api::Logger::getLoggerPtr("")->
            setLogLevel((ndnlog::NdnLoggerDetailLevel)ndnlog::NdnLoggerLevelError);
        noStats = true;
        fOut = stdout;
    }
    else
        if (!(fOut = fopen(outFile.c_str(), "wb")))
            throw runtime_error("failed to open output video file");

    VideoCodec::Image raw(codec.getSettings().spec_.encoder_.width_,
                          codec.getSettings().spec_.encoder_.height_,
                          ImageFormat::I420);

    while (!mustExit && raw.read(f))
    {
        int res = codec.encode(raw, false,
            [fOut](const EncodedFrame& frame){
                if (!writeFrame(fOut, frame))
                    throw runtime_error("error writing frame to the output file ("+to_string(errno)+"): "+strerror(errno));
            },
            [](const VideoCodec::Image&){
                // LogWarn("") << "dropped frame " << nFrames << endl;
            });

        if (res)
            LogError("") << "failed encoding for frame "
                         << codec.getStats().nFrames_ << endl;

        if (!noStats) printStats(codec.getStats(), statFile);
        usleep(5);
    }

    fclose(fOut);
    fclose(f);
}

void runDecoder(VideoCodec& codec, string inFile, string outFile, string statFile)
{
    // open file
    FILE *f, *fOut;

    bool noStats = false;

    if (inFile == "-")
        f = stdin;
    else if (!(f = fopen(inFile.c_str(), "rb")))
            throw runtime_error("failed to open video file for reading");

    if (outFile == "-")
    {
        if (inFile == "-")
            throw runtime_error("can't have STDIN input and STDOUT output at the same time");

        ndnlog::new_api::Logger::getLoggerPtr("")->
            setLogLevel((ndnlog::NdnLoggerDetailLevel)ndnlog::NdnLoggerLevelError);
        noStats = true;
        fOut = stdout;
    }
    else if (!(fOut = fopen(outFile.c_str(), "wb")))
            throw runtime_error("failed to open output video file");

    EncodedFrame frame;
    frame.data_ = nullptr;

    while (!mustExit && readFrame(frame, f))
    {
        LogDebug("") << "read frame of size " << frame.length_ << " " << frame.data_ << endl;

        int res = codec.decode(frame, [fOut](const VideoCodec::Image &image){
            image.write(fOut);
        });

        if (res)
            LogError("") << "error decoding frame (" << res << "): " << codec.getStats().nFrames_ << endl;

        if (!noStats) printStats(codec.getStats(), statFile);
        usleep(5);
    }

    fclose(fOut);
    fclose(f);
}

void printStats(const VideoCodec::Stats& codecStats, string statFile)
{
    static FILE *statF = nullptr;

    if (statFile != "")
    {
        if (!statF)
        {
            statF = fopen(statFile.c_str(), "w+");
            if (!statF)
                throw runtime_error("couldn't open stat file "+statFile);
            fprintf(statF, "captured\tprocessed\tdropped\tkeynum\tbytesIn\tbytesOut\n");
        }

        fprintf(statF, "%d\t%d\t%d\t%d\t%llu\t%llu\n",
                codecStats.nFrames_, codecStats.nProcessed_,
                codecStats.nDropped_, codecStats.nKey_,
                codecStats.bytesIn_, codecStats.bytesOut_);
        fflush(statF);
    }

    cout << "\r"
         << "[ captured " << codecStats.nFrames_
         << " processed: " << codecStats.nProcessed_
         << "/" << codecStats.nDropped_
         << " ( " << setprecision(3) << ((float)codecStats.nProcessed_/(float)(codecStats.nProcessed_+codecStats.nDropped_))*100  << "%) "
         << codecStats.nKey_ << "k"
         << " bytes in " << codecStats.bytesIn_
         << " bytes out " << codecStats.bytesOut_
         << setprecision(4)
         << " (compression x" << (float)codecStats.bytesIn_/(float)codecStats.bytesOut_ << ")"
         << " (effective " << 0 << "Kbps)"
         << " ]" << flush;
}

int getSize(string sizeStr, uint16_t& w, uint16_t& h)
{
    istringstream ss(sizeStr);
    string k;
    int idx = 0;

    while(getline(ss, k, 'x') && idx < 2)
    {
        int val;

        try {
            val = stoi(k);
            if (val > 0)
            {
                if (idx == 0)
                    w = val;
                else
                    h = val;
                idx++;
            }
        }
        catch(std::exception&)
        {
            break;
        }
    }

    return (idx == 2);
}
