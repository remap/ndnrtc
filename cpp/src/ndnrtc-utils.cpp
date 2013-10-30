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

#include "ndnrtc-utils.h"
#include <mach/mach_time.h>

using namespace ndnrtc;
using namespace webrtc;

typedef struct _FrequencyMeter {
    unsigned int nCyclesPerSec_;
    double callsPerSecond_;
    int64_t lastCheckTime_;
} FrequencyMeter;

//********************************************************************************
#pragma mark - all static
static std::vector<FrequencyMeter> freqMeters_;
static VoiceEngine *VoiceEngineInstance = NULL;
static Config AudioConfig;

unsigned int NdnRtcUtils::getSegmentsNumber(unsigned int segmentSize, unsigned int dataSize)
{
    return (unsigned int)ceil((float)dataSize/(float)segmentSize);
}

int NdnRtcUtils::segmentNumber(const Name::Component &segmentComponent)
{
    std::vector<unsigned char> bytes = *segmentComponent.getValue();
    int bytesLength = segmentComponent.getValue().size();
    double result = 0.0;
    unsigned int i;
    
    for (i = 0; i < bytesLength; ++i) {
        result *= 256.0;
        result += (double)bytes[i];
    }
    return (unsigned int)result;
}

int NdnRtcUtils::frameNumber(const Name::Component &segmentComponent)
{
    unsigned int result = 0;
    int valueLength = segmentComponent.getValue().size();
    std::vector<unsigned char> value = *segmentComponent.getValue();
    
    unsigned int i;
    for (i = 0; i < valueLength; ++i) {
        unsigned char digit = value[i];
        if (!(digit >= '0' && digit <= '9'))
            return -1;
        
        result *= 10;
        result += (unsigned int)(digit - '0');
    }
    
    return result;
}

int64_t NdnRtcUtils::millisecondTimestamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    int64_t ticks = 0;
    
#if 0
    ticks = 1000LL*static_cast<int64_t>(tv.tv_sec)+static_cast<int64_t>(tv.tv_usec)/1000LL;
#else
    static mach_timebase_info_data_t timebase;
    if (timebase.denom == 0) {
        // Get the timebase if this is the first time we run.
        // Recommended by Apple's QA1398.
        kern_return_t retval = mach_timebase_info(&timebase);
        if (retval != KERN_SUCCESS) {
            // TODO(wu): Implement CHECK similar to chrome for all the platforms.
            // Then replace this with a CHECK(retval == KERN_SUCCESS);
            asm("int3");
        }
    }
    // Use timebase to convert absolute time tick units into nanoseconds.
    ticks = mach_absolute_time() * timebase.numer / timebase.denom;
    ticks /= 1000000LL;
#endif
    
    return ticks;
};

int64_t NdnRtcUtils::microsecondTimestamp()
{
    int64_t ticks = 0;
    
#if 0
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    ticks = 1000000LL * static_cast<int64_t>(tv.tv_sec) +
        static_cast<int64_t>(tv.tv_usec);
#else
    static mach_timebase_info_data_t timebase;
    if (timebase.denom == 0) {
        // Get the timebase if this is the first time we run.
        // Recommended by Apple's QA1398.
        kern_return_t retval = mach_timebase_info(&timebase);
        if (retval != KERN_SUCCESS) {
            // TODO(wu): Implement CHECK similar to chrome for all the platforms.
            // Then replace this with a CHECK(retval == KERN_SUCCESS);
            asm("int3");
        }
    }
    // Use timebase to convert absolute time tick units into nanoseconds.
    ticks = mach_absolute_time() * timebase.numer / timebase.denom;
#endif
    
    return ticks;
};

unsigned int NdnRtcUtils::setupFrequencyMeter()
{
    FrequencyMeter meter = {0, 0., 0};
    
    freqMeters_.push_back(meter);
    
    return freqMeters_.size()-1;
}

void NdnRtcUtils::frequencyMeterTick(unsigned int meterId)
{
    if (meterId >= freqMeters_.size())
        return;
    
    FrequencyMeter &meter = freqMeters_[meterId];
    int64_t now = millisecondTimestamp();
    
    meter.nCyclesPerSec_++;
    
    if (now - meter.lastCheckTime_ >= 1000)
    {
        meter.callsPerSecond_ = (double)meter.nCyclesPerSec_/(double)(now-meter.lastCheckTime_)*1000.;
        meter.lastCheckTime_ = now;
        meter.nCyclesPerSec_ = 0;
    }
}

double NdnRtcUtils::currentFrequencyMeterValue(unsigned int meterId)
{
    if (meterId >= freqMeters_.size())
        return 0.;
    
    FrequencyMeter &meter = freqMeters_[meterId];
    
    return meter.callsPerSecond_;
}

void NdnRtcUtils::releaseFrequencyMeter(unsigned int meterId)
{
    if (meterId >= freqMeters_.size())
        return;
    
    // do nothing
}

webrtc::VoiceEngine *NdnRtcUtils::sharedVoiceEngine()
{
    if (!VoiceEngineInstance)
    {
        AudioConfig.Set<AudioCodingModuleFactory>(new NewAudioCodingModuleFactory());
        VoiceEngineInstance = VoiceEngine::Create(AudioConfig);
        
        int res = 0;
        
        {// init engine
            VoEBase *voe_base = VoEBase::GetInterface(VoiceEngineInstance);
            
            res = voe_base->Init();
            
            voe_base->Release();
        }
        {// configure
            VoEAudioProcessing *voe_proc = VoEAudioProcessing::GetInterface(VoiceEngineInstance);
            
            voe_proc->SetEcStatus(true);
            voe_proc->Release();
        }

        if (res < 0)
            return NULL;
    }
    
    return VoiceEngineInstance;
}

void NdnRtcUtils::releaseVoiceEngine()
{
    VoiceEngine::Delete(VoiceEngineInstance);
}