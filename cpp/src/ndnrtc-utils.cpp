//
//  ndnrtc-utils.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

//#undef NDN_LOGGING

#include <boost/chrono.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/asio.hpp>

#include "ndnrtc-utils.h"
#include "ndnrtc-endian.h"
#include "ndnrtc-namespace.h"

using namespace std;
using namespace ndnrtc;
using namespace webrtc;

using namespace boost::chrono;

typedef struct _FrequencyMeter {
    double cycleDuration_;
    unsigned int nCyclesPerSec_;
    double callsPerSecond_;
    int64_t lastCheckTime_;
    unsigned int meanEstimatorId_;
} FrequencyMeter;

typedef struct _DataRateMeter {
    double cycleDuration_;
    unsigned int bytesPerCycle_;
    double nBytesPerSec_;
    int64_t lastCheckTime_;
    unsigned int meanEstimatorId_;
} DataRateMeter;

typedef struct _MeanEstimator {
    unsigned int sampleSize_;
    int nValues_;
    double startValue_;
    double prevValue_;
    double valueMean_;
    double valueMeanSq_;
    double valueVariance_;
    double currentMean_;
    double currentDeviation_;
} MeanEstimator;

typedef struct _Filter {
    double coeff_;
    double filteredValue_;
} Filter;

typedef struct _InclineEstimator {
    unsigned int avgEstimatorId_;
    unsigned int nValues_;
    double lastValue_;
    unsigned int sampleSize_;
    unsigned int skipCounter_;
} InclineEstimator;

typedef struct _SlidingAverage {
    unsigned int sampleSize_;
    int nValues_;
    double accumulatedSum_;
    double currentAverage_;
    double currentDeviation_;
    double* sample_;
} SlidingAverage;

//********************************************************************************
#pragma mark - all static
std::string ndnrtc::LIB_LOG = "ndnrtc-startup.log";
static boost::asio::io_service* NdnRtcIoService;
static boost::shared_ptr<FaceProcessor> LibraryFace;

static std::vector<FrequencyMeter> freqMeters_;
static std::vector<DataRateMeter> dataMeters_;
static std::vector<MeanEstimator> meanEstimators_;
static std::vector<Filter> filters_;
static std::vector<InclineEstimator> inclineEstimators_;
static std::vector<SlidingAverage> slidingAverageEstimators_;

static VoiceEngine *VoiceEngineInstance = NULL;

static boost::thread backgroundThread;
static boost::shared_ptr<boost::asio::io_service::work> backgroundWork;

//******************************************************************************
void NdnRtcUtils::setIoService(boost::asio::io_service& ioService)
{
    NdnRtcIoService = &ioService;
}

boost::asio::io_service& NdnRtcUtils::getIoService()
{
    return *NdnRtcIoService;
}

void NdnRtcUtils::startBackgroundThread()
{
    if (!NdnRtcIoService)
        return;
    
    if (!backgroundWork.get() &&
        backgroundThread.get_id() == boost::thread::id())
    {
        backgroundWork.reset(new boost::asio::io_service::work(*NdnRtcIoService));
        backgroundThread = boost::thread([](){
            (*NdnRtcIoService).run();
        });
    }
}

void NdnRtcUtils::stopBackgroundThread()
{
    if (backgroundWork.get())
    {
        backgroundWork.reset();
        (*NdnRtcIoService).stop();
        backgroundThread.try_join_for(boost::chrono::milliseconds(500));
    }
}

bool NdnRtcUtils::isBackgroundThread()
{
    return (boost::this_thread::get_id() == backgroundThread.get_id());
}

void NdnRtcUtils::dispatchOnBackgroundThread(boost::function<void(void)> dispatchBlock,
                                        boost::function<void(void)> onCompletion)
{
    (*NdnRtcIoService).dispatch([=]{
        dispatchBlock();
        if (onCompletion)
            onCompletion();
    });
}

void NdnRtcUtils::performOnBackgroundThread(boost::function<void(void)> dispatchBlock,
                                             boost::function<void(void)> onCompletion)
{
    if (boost::this_thread::get_id() == backgroundThread.get_id())
    {
        dispatchBlock();
        if (onCompletion)
            onCompletion();
    }
    else
    {
        boost::mutex m;
        boost::unique_lock<boost::mutex> lock(m);
        boost::condition_variable isDone;
        
        (*NdnRtcIoService).post([dispatchBlock, onCompletion, &isDone]{
            dispatchBlock();
            if (onCompletion)
                onCompletion();
            isDone.notify_one();
        });
        
        isDone.wait(lock);
    }
}

void NdnRtcUtils::createLibFace(const new_api::GeneralParams& generalParams)
{
    if (!LibraryFace.get())
    {
        LogInfo(LIB_LOG) << "Creating library Face..." << std::endl;
        
        LibraryFace = FaceProcessor::createFaceProcessor(generalParams.host_, generalParams.portNum_, NdnRtcNamespace::defaultKeyChain());
        LibraryFace->startProcessing();
        
        LogInfo(LIB_LOG) << "Library Face created" << std::endl;
    }
}

boost::shared_ptr<FaceProcessor> NdnRtcUtils::getLibFace()
{
    return LibraryFace;
}

void NdnRtcUtils::destroyLibFace()
{
    if (LibraryFace.get())
    {
        LogInfo(LIB_LOG) << "Stopping library Face..." << std::endl;
        LibraryFace->stopProcessing();
        LibraryFace.reset();
        LogInfo(LIB_LOG) << "Library face stopped" << std::endl;
    }
}

//******************************************************************************

unsigned int NdnRtcUtils::getSegmentsNumber(unsigned int segmentSize, unsigned int dataSize)
{
    return (unsigned int)ceil((float)dataSize/(float)segmentSize);
}

int NdnRtcUtils::segmentNumber(const Name::Component &segmentNoComponent)
{
    std::vector<unsigned char> bytes = *segmentNoComponent.getValue();
    int bytesLength = segmentNoComponent.getValue().size();
    int result = 0;
    unsigned int i;
    
    for (i = 0; i < bytesLength; ++i) {
        result *= 256.0;
        result += (int)bytes[i];
    }
    
    return result;
}

int NdnRtcUtils::frameNumber(const Name::Component &frameNoComponent)
{
    return NdnRtcUtils::intFromComponent(frameNoComponent);
}

int NdnRtcUtils::intFromComponent(const Name::Component &comp)
{
    std::vector<unsigned char> bytes = *comp.getValue();
    int valueLength = comp.getValue().size();
    int result = 0;
    unsigned int i;
    
    for (i = 0; i < valueLength; ++i) {
        unsigned char digit = bytes[i];
        if (!(digit >= '0' && digit <= '9'))
            return -1;
        
        result *= 10;
        result += (unsigned int)(digit - '0');
    }
    
    return result;    
}

Name::Component NdnRtcUtils::componentFromInt(unsigned int number)
{
    stringstream ss;
    
    ss << number;
    std::string frameNoStr = ss.str();
    
    return Name::Component((const unsigned char*)frameNoStr.c_str(),
                           frameNoStr.size());
}

// monotonic clock
int64_t NdnRtcUtils::millisecondTimestamp()
{
    milliseconds msec = duration_cast<milliseconds>(steady_clock::now().time_since_epoch());
    return msec.count();
};

// monotonic clock
int64_t NdnRtcUtils::microsecondTimestamp()
{
    microseconds usec = duration_cast<microseconds>(steady_clock::now().time_since_epoch());
    return usec.count();
};

// monotonic clock
int64_t NdnRtcUtils::nanosecondTimestamp()
{
    boost::chrono::nanoseconds nsec = boost::chrono::steady_clock::now().time_since_epoch();
    return nsec.count();
};

// system clock
double NdnRtcUtils::unixTimestamp()
{
    auto now = boost::chrono::system_clock::now().time_since_epoch();
    boost::chrono::duration<double> sec = now;
    return sec.count();
}

//******************************************************************************
unsigned int NdnRtcUtils::setupFrequencyMeter(unsigned int granularity)
{
    FrequencyMeter meter = {1000./(double)granularity, 0, 0., 0, 0};
    meter.meanEstimatorId_ = setupMeanEstimator();
    
    freqMeters_.push_back(meter);
    
    return freqMeters_.size()-1;
}

void NdnRtcUtils::frequencyMeterTick(unsigned int meterId)
{
    if (meterId >= freqMeters_.size())
        return;
    
    FrequencyMeter &meter = freqMeters_[meterId];
    int64_t now = millisecondTimestamp();
    int64_t delta = now - meter.lastCheckTime_;
    
    meter.nCyclesPerSec_++;
    
    if (delta >= meter.cycleDuration_)
        if (meter.lastCheckTime_ >= 0)
        {
            if (meter.lastCheckTime_ > 0)
            {
                meter.callsPerSecond_ = 1000.*(double)meter.nCyclesPerSec_/(double)(delta);
                meter.nCyclesPerSec_ = 0;
                meanEstimatorNewValue(meter.meanEstimatorId_, meter.callsPerSecond_);
            }
            
            meter.lastCheckTime_ = now;
        }
}

double NdnRtcUtils::currentFrequencyMeterValue(unsigned int meterId)
{
    if (meterId >= freqMeters_.size())
        return 0.;
    
    FrequencyMeter &meter = freqMeters_[meterId];
    
    return currentMeanEstimation(meter.meanEstimatorId_);
}

void NdnRtcUtils::releaseFrequencyMeter(unsigned int meterId)
{
    if (meterId >= freqMeters_.size())
        return;
    
    // do nothing
}

//******************************************************************************
unsigned int NdnRtcUtils::setupDataRateMeter(unsigned int granularity)
{
    DataRateMeter meter = {1000./(double)granularity, 0, 0., 0, 0};
    meter.meanEstimatorId_ = NdnRtcUtils::setupMeanEstimator();
    
    dataMeters_.push_back(meter);
    
    return dataMeters_.size()-1;
}

void NdnRtcUtils::dataRateMeterMoreData(unsigned int meterId,
                                        unsigned int dataSize)
{
    if (meterId >= dataMeters_.size())
        return ;
    
    DataRateMeter &meter = dataMeters_[meterId];
    int64_t now = millisecondTimestamp();
    int64_t delta = now - meter.lastCheckTime_;
    
    meter.bytesPerCycle_ += dataSize;
    
    if (delta >= meter.cycleDuration_)
        if (meter.lastCheckTime_ >= 0)
        {
            if (meter.lastCheckTime_ > 0)
            {
                meter.nBytesPerSec_ = 1000.*meter.bytesPerCycle_/delta;
                meter.bytesPerCycle_ = 0;
                meanEstimatorNewValue(meter.meanEstimatorId_, meter.nBytesPerSec_);
            }
            
            meter.lastCheckTime_ = now;
        }
}

double NdnRtcUtils::currentDataRateMeterValue(unsigned int meterId)
{
    if (meterId >= dataMeters_.size())
        return 0.;
    
    DataRateMeter &meter = dataMeters_[meterId];
    return currentMeanEstimation(meter.meanEstimatorId_);
}

void NdnRtcUtils::releaseDataRateMeter(unsigned int meterId)
{
    if (meterId >- dataMeters_.size())
        return;
    
    // nothing here yet
}

//******************************************************************************
unsigned int NdnRtcUtils::setupMeanEstimator(unsigned int sampleSize,
                                             double startValue)
{
    MeanEstimator meanEstimator = {sampleSize, 0, startValue, startValue, startValue, 0., 0., startValue, 0.};
    
    meanEstimators_.push_back(meanEstimator);
    
    return meanEstimators_.size()-1;
}

void NdnRtcUtils::meanEstimatorNewValue(unsigned int estimatorId, double value)
{
    if (estimatorId >= meanEstimators_.size())
        return ;
    
    MeanEstimator &estimator = meanEstimators_[estimatorId];
    
    estimator.nValues_++;
    
    if (estimator.nValues_ > 1)
    {
        double delta = (value - estimator.valueMean_);
        estimator.valueMean_ += (delta/(double)estimator.nValues_);
        estimator.valueMeanSq_ += (delta*delta);
        
        if (estimator.sampleSize_ == 0 ||
            estimator.nValues_ % estimator.sampleSize_ == 0)
        {
            estimator.currentMean_ = estimator.valueMean_;
            
            if (estimator.nValues_ >= 2)
            {
                double variance = estimator.valueMeanSq_/(double)(estimator.nValues_-1);
                estimator.currentDeviation_ = sqrt(variance);
            }
            
            // flush nValues if sample size specified
            if (estimator.sampleSize_)
                estimator.nValues_ = 0;
        }
    }
    else
    {
        estimator.currentMean_ = value;
        estimator.valueMean_ = value;
        estimator.valueMeanSq_ = 0;
        estimator.valueVariance_ = 0;
    }
    
    estimator.prevValue_ = value;
}

double NdnRtcUtils::currentMeanEstimation(unsigned int estimatorId)
{
    if (estimatorId >= meanEstimators_.size())
        return 0;
    
    MeanEstimator& estimator = meanEstimators_[estimatorId];
    
    return estimator.currentMean_;
}

double NdnRtcUtils::currentDeviationEstimation(unsigned int estimatorId)
{
    if (estimatorId >= meanEstimators_.size())
        return 0;
    
    MeanEstimator& estimator = meanEstimators_[estimatorId];
    
    return estimator.currentDeviation_;
}

void NdnRtcUtils::releaseMeanEstimator(unsigned int estimatorId)
{
    if (estimatorId >= meanEstimators_.size())
        return ;
    
    // nothing
}

void NdnRtcUtils::resetMeanEstimator(unsigned int estimatorId)
{
    if (estimatorId >= meanEstimators_.size())
        return ;
    
    MeanEstimator& estimator = meanEstimators_[estimatorId];

    estimator = (MeanEstimator){estimator.sampleSize_, 0, estimator.startValue_,
        estimator.startValue_, estimator.startValue_, 0., 0.,
        estimator.startValue_, 0.};
}

//******************************************************************************
unsigned int NdnRtcUtils::setupSlidingAverageEstimator(unsigned int sampleSize)
{
    SlidingAverage slidingAverage;
    
    slidingAverage.sampleSize_ = (sampleSize == 0)?2:sampleSize;
    slidingAverage.nValues_ = 0;
    slidingAverage.accumulatedSum_ = 0.;
    slidingAverage.currentAverage_ = 0.;
    slidingAverage.currentDeviation_ = 0.;
    
    slidingAverage.sample_ = (double*)malloc(sizeof(double)*sampleSize);
    memset(slidingAverage.sample_, 0, sizeof(double)*sampleSize);
    
    slidingAverageEstimators_.push_back(slidingAverage);
    
    return slidingAverageEstimators_.size()-1;
}

void NdnRtcUtils::slidingAverageEstimatorNewValue(unsigned int estimatorId, double value)
{
    if (estimatorId >= slidingAverageEstimators_.size())
        return ;
    
    SlidingAverage& estimator = slidingAverageEstimators_[estimatorId];
    
    estimator.sample_[estimator.nValues_%estimator.sampleSize_] = value;
    estimator.nValues_++;
    
    if (estimator.nValues_ >= estimator.sampleSize_)
    {
        estimator.currentAverage_ = (estimator.accumulatedSum_+value)/estimator.sampleSize_;
        estimator.accumulatedSum_ += value - estimator.sample_[estimator.nValues_%estimator.sampleSize_];
        
        estimator.currentDeviation_ = 0.;
        for (int i = 0; i < estimator.sampleSize_; i++)
            estimator.currentDeviation_ += (estimator.sample_[i]-estimator.currentAverage_)*(estimator.sample_[i]-estimator.currentAverage_);
        estimator.currentDeviation_ = sqrt(estimator.currentDeviation_/(double)estimator.nValues_);
    }
    else
        estimator.accumulatedSum_ += value;
}

double NdnRtcUtils::currentSlidingAverageValue(unsigned int estimatorId)
{
    if (estimatorId >= slidingAverageEstimators_.size())
        return 0;
    
    SlidingAverage& estimator = slidingAverageEstimators_[estimatorId];
    return estimator.currentAverage_;
}

double NdnRtcUtils::currentSlidingDeviationValue(unsigned int estimatorId)
{
    if (estimatorId >= slidingAverageEstimators_.size())
        return 0;
    
    SlidingAverage& estimator = slidingAverageEstimators_[estimatorId];
    return estimator.currentDeviation_;
}

void NdnRtcUtils::releaseAverageEstimator(unsigned int estimatorID)
{
    // nothing
}

//******************************************************************************
unsigned int
NdnRtcUtils::setupFilter(double coeff)
{
    Filter filter = {coeff, 0.};
    filters_.push_back(filter);
    return filters_.size()-1;
}

void
NdnRtcUtils::filterNewValue(unsigned int filterId, double value)
{
    if (filterId < filters_.size())
    {
        Filter &filter = filters_[filterId];
        
        if (filter.filteredValue_ == 0)
            filter.filteredValue_ = value;
        else
            filter.filteredValue_ += (value-filter.filteredValue_)*filter.coeff_;
    }
}

double
NdnRtcUtils::currentFilteredValue(unsigned int filterId)
{
    if (filterId < filters_.size())
        return filters_[filterId].filteredValue_;
    
    return 0.;
}

void
NdnRtcUtils::releaseFilter(unsigned int filterId)
{
    // do nothing
}

//******************************************************************************
unsigned int
NdnRtcUtils::setupInclineEstimator(unsigned int sampleSize)
{
    InclineEstimator ie;
    ie.sampleSize_ = sampleSize;
    ie.nValues_ = 0;
    ie.avgEstimatorId_ = NdnRtcUtils::setupSlidingAverageEstimator(sampleSize);
    ie.lastValue_ = 0.;
    ie.skipCounter_ = 0;
    inclineEstimators_.push_back(ie);
    
    return inclineEstimators_.size()-1;
}

void
NdnRtcUtils::inclineEstimatorNewValue(unsigned int estimatorId, double value)
{
    if (estimatorId < inclineEstimators_.size())
    {
        InclineEstimator& ie = inclineEstimators_[estimatorId];
        
        if (ie.nValues_ == 0)
            ie.lastValue_ = value;
        else
        {
            double incline = (value-ie.lastValue_);
            
            slidingAverageEstimatorNewValue(ie.avgEstimatorId_, incline);
            ie.lastValue_ = value;
        }
        
        ie.nValues_++;
    }
}

double
NdnRtcUtils::currentIncline(unsigned int estimatorId)
{
    if (estimatorId < inclineEstimators_.size())
    {
        InclineEstimator& ie = inclineEstimators_[estimatorId];
        return currentSlidingAverageValue(ie.avgEstimatorId_);
    }
    
    return 0.;
}

void NdnRtcUtils::releaseInclineEstimator(unsigned int estimatorId)
{
    // tbd
}

//******************************************************************************
webrtc::VoiceEngine *NdnRtcUtils::sharedVoiceEngine()
{
    if (!VoiceEngineInstance)
    {
        NdnRtcUtils::initVoiceEngine();
    }
    
    return VoiceEngineInstance;
}

void NdnRtcUtils::initVoiceEngine()
{
    boost::mutex m;
    boost::unique_lock<boost::mutex> lock(m);
    boost::condition_variable initialized;
    
    NdnRtcUtils::dispatchOnVoiceThread(
                                       [&initialized](){
                                           LogInfo(LIB_LOG) << "Iinitializing voice engine..." << std::endl;
                                           VoiceEngineInstance = VoiceEngine::Create();
                                           
                                           int res = 0;
                                           
                                           {// init engine
                                               VoEBase *voe_base = VoEBase::GetInterface(VoiceEngineInstance);
                                               
                                               res = voe_base->Init();
                                               
                                               voe_base->Release();
                                           }
                                           {// configure
                                               VoEAudioProcessing *voe_proc = VoEAudioProcessing::GetInterface(VoiceEngineInstance);
                                               
                                               voe_proc->SetEcStatus(true, kEcConference);
                                               voe_proc->Release();
                                           }
                                           
                                           LogInfo(LIB_LOG) << "Voice engine initialized" << std::endl;
                                           initialized.notify_one();
                                       },
                                       boost::function<void()>());
    initialized.wait(lock);
}

void NdnRtcUtils::releaseVoiceEngine()
{
    NdnRtcUtils::dispatchOnVoiceThread([](){
                                           VoiceEngine::Delete(VoiceEngineInstance);
                                       },
                                       boost::function<void()>());
}

void NdnRtcUtils::startVoiceThread()
{
    startBackgroundThread();
}

void NdnRtcUtils::stopVoiceThread()
{
    stopBackgroundThread();
}

void NdnRtcUtils::dispatchOnVoiceThread(boost::function<void(void)> dispatchBlock,
                           boost::function<void(void)> onCompletion)
{
    dispatchOnBackgroundThread(dispatchBlock, onCompletion);
}

string NdnRtcUtils::stringFromFrameType(const WebRtcVideoFrameType &frameType)
{
    switch (frameType) {
        case webrtc::kDeltaFrame:
            return "DELTA";
        case webrtc::kKeyFrame:
            return "KEY";
        case webrtc::kAltRefFrame:
            return "ALT-REF";
        case webrtc::kGoldenFrame:
            return "GOLDEN";
        case webrtc::kSkipFrame:
            return "SKIP";
        default:
            return "UNKNOWN";
    }
}

unsigned int NdnRtcUtils::toFrames(unsigned int intervalMs,
                                   double fps)
{
    return (unsigned int)ceil(fps*(double)intervalMs/1000.);
}

unsigned int NdnRtcUtils::toTimeMs(unsigned int frames,
                                   double fps)
{
    return (unsigned int)ceil((double)frames/fps*1000.);
}

uint32_t NdnRtcUtils::generateNonceValue()
{
    uint32_t nonce = (uint32_t)std::rand();
    
    return nonce;
}

Blob NdnRtcUtils::nonceToBlob(const uint32_t nonceValue)
{
    uint32_t beValue = htobe32(nonceValue);
    Blob b((uint8_t *)&beValue, sizeof(uint32_t));
    return b;
}

uint32_t NdnRtcUtils::blobToNonce(const ndn::Blob &blob)
{
    if (blob.size() < sizeof(uint32_t))
        return 0;
    
    uint32_t beValue = *(uint32_t *)blob.buf();
    return be32toh(beValue);
}

std::string NdnRtcUtils::getFullLogPath(const new_api::GeneralParams& generalParams,
                                  const std::string& fileName)
{
    static char logPath[PATH_MAX];
    return ((generalParams.logPath_ == "")?std::string(getwd(logPath)):generalParams.logPath_) + "/" + fileName;
}


std::string NdnRtcUtils::toString(const char *format, ...)
{
    std::string str = "";
    
    if (format)
    {
        char *stringBuf = (char*)malloc(256);
        memset((void*)stringBuf, 0, 256);
        
        va_list args;
        
        va_start(args, format);
        vsprintf(stringBuf, format, args);
        va_end(args);
        
        str = string(stringBuf);
        free(stringBuf);
    }
    
    return str;
}
