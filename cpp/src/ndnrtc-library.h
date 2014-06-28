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

#include "params.h"
#include "statistics.h"
#include "ndnrtc-observer.h"
#include "external-renderer.h"

namespace ndnrtc {
    
    /**
     * This abstract class declares interface for the library's observer - an
     * instance which can receive status updates from the library.
     */
    class INdnRtcLibraryObserver {
    public:
        /**
         * This method is called whenever library encouteres errors or state
         * transistions (for instance, fetching has started). Arguments provided
         * work only as a source of additional information about what has 
         * happened inside the library.
         * @param state Indicates which state library has encountered (i.e. 
         * "error" or "info"). Can be used by observer for filtering important 
         * events. Currently, following states are provided:
         *      "error"
         *      "info"
         * @param args Any additional info that accompany new state (human-
         * readable text information).
         */
        virtual void onStateChanged(const char *state, const char *args) = 0;
    };
    
    /**
     * This class provides interface to work with NDN-RTC library.
     * It provides calls to allow publish audio/video streams and fetch them.
     * User should create an instance of this class using static methods 
     * provided. All further communications with NDN-RTC library should be 
     * performed using this instance.
     * No upper boundary exists for the number of simultaneously fetched 
     * streams. Library is configured using ParamsStruct structure.
     */
    class NdnRtcLibrary : public INdnRtcObjectObserver {
    public:
        
        /**
         * This method loads library from the path provided and creates a
         * library object.
         * @param libPath A path to the library's binary
         */
        static NdnRtcLibrary *instantiateLibraryObject(const char *libPath)
        {
            void *libHandle = dlopen(libPath, RTLD_LAZY);
            
            if (libHandle == NULL)
            {
                LogError("")
                << "error while loading NdnRTC library: " << dlerror() << std::endl;
                
                return NULL;
            }
            
            NdnRtcLibrary* (*create_ndnrtc)(void *);
            create_ndnrtc = (NdnRtcLibrary* (*)(void*))
            dlsym(libHandle, "create_ndnrtc");
            
            NdnRtcLibrary *libObject = create_ndnrtc(libHandle);
            
            return libObject;
        }
        
        /**
         * Properly destroys library object
         * @param libObject Library object to destroy
         */
        static void destroyLibraryObject(NdnRtcLibrary *libObject)
        {
            void (*destroy_ndnrtc)(NdnRtcLibrary*);
            destroy_ndnrtc = (void (*)(NdnRtcLibrary*))
            dlsym(libObject->getLibraryHandle(), "destroy_ndnrtc");
            
            destroy_ndnrtc(libObject);
        }
        
        /**
         * Creates instance of library object using library handle provided.
         * NOTE: do not use constructor directly, use instantiateLibraryObject 
         * static method instead, as it performs all necessary initializations.
         */
        NdnRtcLibrary(void *libHandle);
        ~NdnRtcLibrary();
        
        /**
         * Configures library object with provided parameters
         * @param params Video parameters
         * @param audioParams Audio parameters
         */
        virtual void configure(const ParamsStruct &params,
                               const ParamsStruct &audioParams);
        /**
         * Returns current library parameters
         * @param params Video parameters
         * @param audioParams Audio parameters
         */
        virtual void currentParams(ParamsStruct &params,
                                   ParamsStruct &audioParams);
        
        /**
         * Sets library observer
         * @param observer Refernce to observer object
         */
        virtual void setObserver(INdnRtcLibraryObserver *observer) {
            observer_ = observer;
        }
        /**
         * Returns default parameters for audio and video. This method can be 
         * used as a preparation step before configuring library - after calling 
         * this method, necessary parameters can be altered and library 
         * configured using these structures.
         * @param videoParams Default video parameters will be written into this
         * structure
         * @param audioParams Default audio parameters will be written into this
         * structure
         */
        virtual void getDefaultParams(ParamsStruct &videoParams,
                                      ParamsStruct &audioParams) const;
        
        /**
         * Returns statistics of the producer queried
         * @param producerId Name of the user, which stream statistics are being
         * queried
         * @param stat Upon return, this structure contains statistics for the 
         * user queried
         */
        virtual int getStatistics(const char *producerId,
                                  NdnLibStatistics &stat) const;
        
        /**
         * Starts publishing media streams under provided username. If video is 
         * enabled, rendering is performed in separate cocoa window, managed by
         * the library
         * @param username Which will be used for publishing media
         */
        virtual int startPublishing(const char* username);
        /**
         * Starts publishing media streams under provided username. If video is 
         * enabled, rendering is delegated to the external renderer object which
         * should conform to the IExternalRenderer interface.
         * @param username Which will be used for publishing media
         */
        virtual int startPublishing(const char* username,
                                    IExternalRenderer* const renderer);
        /**
         * Stops publishing. If publishing was not started, does nothing.
         */
        virtual int stopPublishing();

        /**
         * Returns full NDN prefix under which publishing is performed
         * @param userPrefix Upon return, contains NDN prefix
         */
        virtual void getPublisherPrefix(const char** userPrefix);
        
        /**
         * Starts fetching from the remote user. If video is enabled, rendering
         * is performed in separate cocoa window, managed by the library.
         * @param producerId Name of the user which streams will be fetched
         */
        virtual int startFetching(const char* producerId);
        /**
         * Stops fetching from the remote user. If fetching was not intitated, 
         * does nothing.
         * @param producerId Name of the user which streams fetching should be 
         * stopped
         */
        virtual int stopFetching(const char* producerId);
        /**
         * Returns full NDN prefix for the remote producer whose streams are 
         * currently fetched.
         * @param producerId Remote producer name
         * @param prducerPrefix Upon return, contains full NDN prefix for media 
         * fetching
         */
        virtual void getProducerPrefix(const char* producerId,
                                       const char** producerPrefx);
        
        /**
         * Returns dynamic library handle
         * @return Library handle which was used for instantiating library 
         * object
         */
        virtual void* getLibraryHandle(){ return libraryHandle_; };
        
        /**
         * Returns current build number
         * @return Current build number
         */
        virtual int getBuildNumber();
        
        /**
         * Arranges all app windows on the screen
         */
        virtual void arrangeWindows();
        
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
        virtual void onErrorOccurred(const char *errorMessage);
    };
}


#endif /* defined(__ndnrtc__ndnrtc_library__) */
