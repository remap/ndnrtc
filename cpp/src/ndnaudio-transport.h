//
//  ndnaudio-transport.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 10/21/13
//

#ifndef __ndnrtc__ndnaudio_transport__
#define __ndnrtc__ndnaudio_transport__

#include "ndnrtc-common.h"
#include "ndnrtc-object.h"

namespace ndnrtc {
    class NdnAudioTransport : public webrtc::Transport, public NdnRtcObject
    {
    public:
        
        NdnAudioTransport();
        ~NdnAudioTransport();
        
        int init(webrtc::VoENetwork *voiceEngine);
        int start(); // starts accepting data (in/out)
        int stop(); // stops accepting data (in/out)
        
        // Transport interface
        int SendPacket(int channel, const void *data, int len);
        int SendRTCPPacket(int channel, const void *data, int len);
        
    private:
        bool started_;
        webrtc::VoENetwork *voiceEngine_;
    };
}

#endif /* defined(__ndnrtc__ndnaudio_transport__) */
