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
#include "interfaces.h"

namespace ndnrtc {
    
    /**
     * This class provides interface to work with NDN-RTC library.
     * It provides calls to allow publish audio/video streams and fetch them.
     * User should create an instance of this class using static methods 
     * provided. All further communications with NDN-RTC library should be 
     * performed using this instance.
     * No upper boundary exists for the number of simultaneously fetched 
     * streams. Library is configured using ParamsStruct structure.
     */
    class NdnRtcLibrary {
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
         * Sets library observer
         * @param observer Refernce to observer object
         */
        virtual void setObserver(INdnRtcLibraryObserver *observer) DEPRECATED;
        
        /**
         * Returns statistics of the producer queried
         * @param producerId Name of the user, which stream statistics are being
         * queried
         * @param streamName The name of producer's stream
         * @param threadName The name of the producer's stream thread
         * @param stat Upon return, this structure contains statistics for the
         * user queried
         */
        virtual int getStatistics(const char *producerId,
                                  const char* streamName,
                                  const char* threadName,
                                  NdnLibStatistics &stat) const;
        
        //******************************************************************************
        //******************************************************************************
        //******************************************************************************
        
        /**
         * Starts NDN-RTC session with username and prefix provided
         * @param username NDN-RTC user name
         * @return Session prefix in the following form:
         *      <prefix>/<ndnrtc_component>/<username>
         *      where ndnrtc_component is "ndnrtc/user", but may be changed in
         *      future releases
         */
        virtual std::string startSession(const std::string& username,
                                         const new_api::GeneralParams& generalParams,
                                         ISessionObserver *sessionObserver);
        
        virtual int stopSession(const std::string& sessionPrefix);
        
        /**
         * Sets remote user session observer
         * @param username
         * @param prefix
         * @return Remote user's prefix
         */
        virtual std::string
        setRemoteSessionObserver(const std::string& username,
                                 const std::string& prefix,
                                 const new_api::GeneralParams& generalParams,
                                 IRemoteSessionObserver* sessionObserver);
        
        virtual int
        removeRemoteSessionObserver(const std::string& sessionPrefix);
        
        /**
         * Adds local stream to the session identified by userPrefix parameter
         * @param userPrefix User prefix obtained by previous startSession call
         * @param params
         * @param capturer
         * @return Stream prefix in th following form:
         *      <userPrefix>/<ndnrtc_streams>/<stream_name>
         *      where ndnrtc_streams is "streams", but may be changed in future 
         *      releases
         * @see startSession
         */
        virtual std::string addLocalStream(const std::string& sessionPrefix,
                                           const new_api::MediaStreamParams& params,
                                           IExternalCapturer** const capturer);
        
        /**
         * Removes local stream identified by streamPrefix
         * @param streamPrefix Stream prefix obtained by previous addLocalStream
         * call
         * @see addLocalStream
         */
        virtual int removeLocalStream(const std::string& sessionPrefix,
                                      const std::string& streamPrefix);
        
        /**
         * Adds local thread to the existing media stream identified by 
         * streamPrefix
         * @param streamPrefix Stream prefix obtained by previous addLocalStream
         * call
         * @param params Media thread parameters
         * @return Thread prefix in the following form:
         *      <streamPrefix>/<thread_name>
         *      where thread_name is taken from thread parameters provided
         * @see addLocalStream
         */
//        virtual std::string addLocalThread(const std::string& streamPrefix,
//                                           const new_api::MediaThreadParams& params);
        
        /**
         * Removes local thread identified by threadPrefix provided
         * @param threadPrefix Thread prefix obtained by previous call to 
         * addLocalThread
         * @see addLocalThread
         */
//        virtual int removeLocalThread(const std::string& threadPrefix);
        
        /**
         * Adds new remote stream and starts fetching from its' first thread
         */
        virtual std::string addRemoteStream(const std::string& remoteSessionPrefix,
                                            const new_api::MediaStreamParams& params,
                                            IExternalRenderer* const renderer);
        
        virtual int removeRemoteStream(const std::string& streamPrefix);
        
        //******************************************************************************
        //******************************************************************************
        //******************************************************************************
        
        /**
         * Starts publishing media streams under provided username configured
         * according to provided parameters. If video is enabled, rendering is
         * performed in separate cocoa window, managed by the library
         * @param params Parameters used for configuring local producer
         */
        virtual int startLocalProducer(const new_api::AppParams& params);
        
        /**
         * Starts publishing media streams under provided username configured
         * according to provided parameters. If video is enabled, rendering is
         * delegated to the external renderer object which should conform to the
         * IExternalRenderer interface.
         * @param params Parameters used for configuring local producer
         * @param renderer Pointer to external rendering class which conforms to
         * IExternalRenderer interface.
         */
        virtual int startLocalProducer(const new_api::AppParams& params,
                                       IExternalRenderer* const renderer);
        
        /**
         * Stops local producer. If local producer was not started, does nothing.
         */
        virtual int stopLocalProducer();
        
        /**
         * Returns current local producer's parameters, if it's started.
         * Otherwise - returns zeroed structures.
         * @param params Parameters, used to configure local producer
         */
        virtual void getLocalProducerParams(new_api::AppParams& params);
        
        /**
         * Initializes local publisher. Publishing starts as soon as user starts
         * capturing new video frames and delivers them using IExternalCapturer
         * interface.
         * @param username Which will be used for publishing media
         * @param capturer Pointer to an object conforming to IExternalCapturer
         * @see IExternalCapturer
         */
        virtual int initPublishing(const char* username,
                                   IExternalCapturer** const capturer);

        /**
         * Initializes local publisher. Publishing starts as soon as user starts
         * capturing new video frames and delivers them using IExternalCapturer
         * interface. Rendering is delegated to the external renderer object
         * which should conform to the IExternalRenderer interface.
         * @param username Which will be used for publishing media
         * @param capturer Pointer to an object conforming to IExternalCapturer
         * @param renderer Pointer to external rendering class which conforms to
         * @see IExternalRenderer, IExternalCapturer
         */
        virtual int initPublishing(const char* username,
                                   IExternalCapturer** const capturer,
                                   IExternalRenderer* const renderer);

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
         * Starts fetching from the remote user. If video is enabled, rendering
         * is performed in delegated to the external object which hsould conform
         * to the IEXternalRenderer interface.
         * @param producerId Name of the user which streams will be fetched
         * @param renderer Pointer to external rendering class which conforms to
         * IExternalRenderer interface.
         */
        virtual int startFetching(const char* producerId,
                                  IExternalRenderer* const renderer);
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
         * @deprecated Use getRemoteProducerParams instead
         */
        virtual void getProducerPrefix(const char* producerId,
                                       const char** producerPrefx) DEPRECATED;

        /**
         * Returns current remote producer's parameters in ParamsStruct 
         * structure. This structure has updated parameters, actually fetched 
         * from remote producer.
         * @param producerId Remote producer user name
         * @param remoteProducerParams A pointer to the structure which upon 
         * return contains actual remote producer's parameters
         */
        virtual void getRemoteProducerParams(const char* producerId,
                                             const ParamsStruct* videoParams,
                                             const ParamsStruct* audioParams);
        
        /**
         * Returns dynamic library handle
         * @return Library handle which was used for instantiating library 
         * object
         */
        virtual void* getLibraryHandle(){ return libraryHandle_; };
        
        /**
         * Returns current library version
         * @param versionString A pointer to the string where the library
         * version will be stored.
         */
        virtual void getVersionString(char **versionString);
        
        /**
         * Arranges all app windows on the screen
         */
        virtual void arrangeWindows();
        
        //**********************************************************************
        // Deprecated methods
        //**********************************************************************
        
        /**
         * Returns default parameters for audio and video. This method can be
         * used as a preparation step before configuring library - after calling
         * this method, necessary parameters can be altered and library
         * configured using these structures.
         * @param videoParams Default video parameters will be written into this
         * structure
         * @param audioParams Default audio parameters will be written into this
         * structure
         * @deprecated This method is not supported anymore
         */
        virtual void getDefaultParams(ParamsStruct &videoParams,
                                      ParamsStruct &audioParams) const DEPRECATED;
        
        /**
         * Returns statistics of the producer queried
         * @param producerId Name of the user, which stream statistics are being
         * queried
         * @param stat Upon return, this structure contains statistics for the
         * user queried
         * @deprecated Use getStatistics(const char*, unsigned int, NdnLibStatistics)
         * instead
         */
        virtual int getStatistics(const char *producerId,
                                  NdnLibStatistics &stat) const DEPRECATED;
        
        /**
         * Configures library object with provided parameters
         * @param params Video parameters
         * @param audioParams Audio parameters
         * @deprecated Now publishing parameters should be passed into
         * designated calls (startLocalProducer)
         * @see startLocalProducer
         */
        virtual void configure(const ParamsStruct &params,
                               const ParamsStruct &audioParams) DEPRECATED;
        
        /**
         * Returns current library parameters
         * @param params Video parameters
         * @param audioParams Audio parameters
         * @deprecated Use getLocalProducerParams instead
         */
        virtual void currentParams(ParamsStruct &params,
                                   ParamsStruct &audioParams) DEPRECATED;
        
        /**
         * Starts publishing media streams under provided username. If video is
         * enabled, rendering is performed in separate cocoa window, managed by
         * the library
         * @param username Which will be used for publishing media
         * @deprecated Use startLocalProducer calls instead
         */
        virtual int startPublishing(const char* username) DEPRECATED;
        /**
         * Starts publishing media streams under provided username. If video is
         * enabled, rendering is delegated to the external renderer object which
         * should conform to the IExternalRenderer interface.
         * @param username Which will be used for publishing media
         * @param renderer Pointer to external rendering class which conforms to
         * IExternalRenderer interface.
         * @deprecated Use startLocalProducer calls instead
         */
        virtual int startPublishing(const char* username,
                                    IExternalRenderer* const renderer) DEPRECATED;
        
        /**
         * Stops publishing. If publishing was not started, does nothing.
         * @deprecated Use stopLocalProducer instead
         */
        virtual int stopPublishing() DEPRECATED;
        
    private:
        void *libraryHandle_;
        char *publisherId_ = 0;
        ParamsStruct libParams_, libAudioParams_;
        
        // private methods go here
        int notifyObserverWithError(const char *format, ...) const;
        int notifyObserverWithState(const char *stateName,
                                    const char *format, ...) const;
        void notifyObserver(const char *state, const char *args) const;
        
        int preparePublishing(const char* username,
                              bool useExternalCapturer,
                              IExternalRenderer* const renderer);
    };
}


#endif /* defined(__ndnrtc__ndnrtc_library__) */
