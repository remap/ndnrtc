//
//  ndnrtc-library.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__ndnrtc_library__
#define __ndnrtc__ndnrtc_library__

#include <dlfcn.h>

//#include "ndnrtc-common.h"
//#include "ndnrtc-object.h"
#include "params.h"
#include "statistics.h"
#include "ndnrtc-observer.h"

namespace ndnrtc {
    
    class INdnRtcLibraryObserver {
    public:
        virtual void onStateChanged(const char *state, const char *args) = 0;
    };
    
    class NdnRtcLibrary : public INdnRtcObjectObserver {
    public:
        // construction/desctruction
        NdnRtcLibrary(void *libHandle);
        ~NdnRtcLibrary();
        
        // public static methods go here
        static NdnRtcLibrary *instantiateLibraryObject(const char *libPath)
        {
            void *libHandle = dlopen(libPath, RTLD_LAZY);
            
            if (libHandle == NULL)
            {
                LOG_NDNERROR("error while loading NdnRTC library: %s", dlerror());
                return NULL;
            }
            
            NdnRtcLibrary* (*create_ndnrtc)(void *);
            create_ndnrtc = (NdnRtcLibrary* (*)(void*))
            dlsym(libHandle, "create_ndnrtc");
            
            NdnRtcLibrary *libObject = create_ndnrtc(libHandle);
            
            return libObject;
        }
        
        static void destroyLibraryObject(NdnRtcLibrary *libObject)
        {
            void (*destroy_ndnrtc)(NdnRtcLibrary*);
            destroy_ndnrtc = (void (*)(NdnRtcLibrary*))
            dlsym(libObject->getLibraryHandle(), "destroy_ndnrtc");
            
            destroy_ndnrtc(libObject);
        }
        
        static ParamsStruct createParamsStruct();
        static void releaseParamsStruct(ParamsStruct &params);
        
        // public methods go here
        virtual void configure(const ParamsStruct &params,
                               const ParamsStruct &audioParams);
        virtual void currentParams(ParamsStruct &params,
                                   ParamsStruct &audioParams);
        
        virtual void setObserver(INdnRtcLibraryObserver *observer) {
            observer_ = observer;
        }
        virtual void getDefaultParams(ParamsStruct &videoParams,
                                      ParamsStruct &audioParams) const;
        virtual int getStatistics(const char *producerId,
                                  NdnLibStatistics &stat) const;
        
        virtual int startPublishing(const char* username);
        virtual int stopPublishing();
        virtual void getPublisherPrefix(const char** userPrefix);
        
        virtual int startFetching(const char* producerId);
        virtual int stopFetching(const char* producerId);
        virtual void getProducerPrefix(const char* producerId,
                                       const char** producerPrefx);
        
        virtual void onErrorOccurred(const char *errorMessage);
        virtual void* getLibraryHandle(){ return libraryHandle_; };
    private:
        void *libraryHandle_;
        char *publisherId_ = 0;
        ParamsStruct libParams_, libAudioParams_;
        INdnRtcLibraryObserver *observer_;
        
        // private methods go here
        int notifyObserverWithError(const char *format, ...) const;
        int notifyObserverWithState(const char *stateName,
                                    const char *format, ...) const;
        void notifyObserver(const char *state, const char *args) const;
    };
}


#endif /* defined(__ndnrtc__ndnrtc_library__) */
