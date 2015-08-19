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
         * Returns a pointer to the singleton instance of NdnRtcLirary class
         */
        static NdnRtcLibrary& getSharedInstance();
        
        /**
         * Sets library observer
         * @param observer Pointer to the observer object
         * @see INdnRtcLibraryObserver
         */
        virtual void
        setObserver(INdnRtcLibraryObserver *observer);

        /**
         * Starts NDN-RTC session with username and parameters provided
         * @param username NDN-RTC user name
         * @param generalParams General NDN-RTC parameters object
         * @return Session prefix in the following form:
         *      <prefix>/<ndnrtc_component>/<username>
         *      where ndnrtc_component is "ndnrtc/user", but may be changed in
         *      future releases
         *      prefix is taken from generalParams object
         *      If failed - returns empty string and error message is sent to 
         *      library observer
         * @see setObserver
         */
        virtual std::string
        startSession(const std::string& username,
                     const new_api::GeneralParams& generalParams,
                     ISessionObserver *sessionObserver);
        
        /**
         * Stops session started previously
         * @param sessionPrefix Session prefix returned from startSession call
         * @return RESULT_OK on success, RESULT_ERR on failure
         * @see startSession
         */
        virtual int
        stopSession(const std::string& sessionPrefix);
        
        /**
         * Adds publishing stream to the session
         * @param sessionPrefix Session prefix returned by previous startSession
         *                      call
         * @param params Media stream parameters
         * @param capturer Upon success, pointer contains a pointer to the object, 
         *                 which is used as an external video capturing system 
         *                 (video only). In this case, stream publishing is driven 
         *                 by this object - whenever new frame is fed to this 
         *                 object, it gets published
         * @return Stream prefix in th following form:
         *         <session_prefix>/<stream_name>
         *         If failed, returns empty string
         * @see startSession, IExternalCapturer
         */
        virtual std::string
        addLocalStream(const std::string& sessionPrefix,
                       const new_api::MediaStreamParams& params,
                       IExternalCapturer** const capturer);
        
        /**
         * Removes publishing stream
         * @param sessionPrefix Session prefix returned by previous startSession 
         *                      call
         * @param streamPrefix Stream prefix returned by previous addLocalStream 
         *                     call
         * @return RESULT_OK on success, RESULT_ERR on failure
         * @see startSession, addLocalStream
         */
        virtual int
        removeLocalStream(const std::string& sessionPrefix,
                          const std::string& streamPrefix);
        
        /**
         * Adds new remote stream and starts fetching from its' first thread
         * @param remoteSessionPrefix Remote producer's session prefix returned
         *                            by previsous setRemoteSessionObserver call
         * @param threadName Thread name to fetch from
         * @param params Media stream parameters
         * @param generalParams General NDN-RTC parameters
         * @param consumerParams General consumer parameters
         * @param renderer Pointer to the object which conforms to the 
         *                 IExternalRenderer interface (video only)
         * @return Remote stream prefix on success and empty string on failure
         * @see setRemoteSessionObserver
         */
        virtual std::string
        addRemoteStream(const std::string& remoteSessionPrefix,
                        const std::string& threadName,
                        const new_api::MediaStreamParams& params,
                        const new_api::GeneralParams& generalParams,
                        const new_api::GeneralConsumerParams& consumerParams,
                        IExternalRenderer* const renderer);
        
        /**
         * Removes remote stream and stops fetching from it
         * @param streamPrefix Stream prefix returned by previous addRemoteStream 
         *                     call
         * @return Full log file path on success, empty string on failure
         */
        virtual std::string
        removeRemoteStream(const std::string& streamPrefix);
        
        /**
         * Sets remote stream obsever and starts sending fetching updates to it
         * @param streamPrefix Stream prefix returned by previous addRemoteStream
         *                     call
         * @param observer Pointer to an object which conforms to the 
         *                 IConsumerObserver interface
         * @return RESULT_OK on success, RESULT_ERR on failure
         */
        virtual int
        setStreamObserver(const std::string& streamPrefix,
                          IConsumerObserver* const observer);

        /**
         * Removes remote stream observer and stops sending updates to it
         * @param streamPrefix Stream prefix returned by previous addRemoteStream
         *                     call
         * @return RESULT_OK on success, RESULT_ERR on failure
         * @see setStreamObserver
         */
        virtual int
        removeStreamObserver(const std::string& streamPrefix);
        
        /**
         * Returns currently fetched media thread name
         * @param streamPrefix Stream prefix returned by previous addRemoteStream
         *                     call
         * @see addRemoteStream
         */
        virtual std::string
        getStreamThread(const std::string& streamPrefix);
        
        /**
         * Forces specified fetching stream to switch active media thread
         * @param streamPrefix Stream prefix returned by previous addRemoteStream
         *                     call
         * @param threadName Name of the thread which should be used for 
         *                   fetching media. All threads are returned in SessionInfo
         *                   object to remote session observer
         * @return RESULT_OK on success, RESULT_ERR on failure
         * @see setRemoteSessionObserver
         */
        virtual int
        switchThread(const std::string& streamPrefix,
                     const std::string& threadName);
        
        /**
         * Returns statistics of remote fetching stream
         * @param streamPrefix Stream prefix returned by previous addRemoteStream
         *                     call
         * @param stat Reference to a receiver statistics object
         * @return RESULT_OK on success, RESULT_ERR on failure
         */
        virtual new_api::statistics::StatisticsStorage
        getRemoteStreamStatistics(const std::string& streamPrefix);

        /**
         * Returns current library version
         * @param versionString A pointer to the string where the library
         * version will be stored.
         */
        virtual void getVersionString(char **versionString);
        
        /**
         * Serializes session info structure into an array of bytes.
         * @param sessionInfo Session info structure to serialize
         * @param length Upon succesfull serialization, contains lenght of the
         *   byte array, otherwise - 0
         * @param bytes Contains serialized bytes array for session info structure
         * @note Bytes array is allocated inside this function. One should check
         * length parameter - if it's greater than 0, deallocation of bytes array 
         * is the responsibility of the caller.
         */
        virtual void serializeSessionInfo(const new_api::SessionInfo &sessionInfo,
                                          unsigned int &length,
                                          unsigned char **bytes);
        
        /**
         * Deserializes bytes array into a session info structure
         * @param length Length of the bytes array
         * @param btyes Bytes array containing session info strcture bytes
         * @param sessionInfo Deserialized session info structure
         * @return true if deserialization was successeful, false otherwise
         */
        virtual bool deserializeSessionInfo(const unsigned int length,
                                            const unsigned char *bytes,
                                            new_api::SessionInfo &sessionInfo);

        ~NdnRtcLibrary();
        
    private:
        NdnRtcLibrary();
        NdnRtcLibrary(NdnRtcLibrary const&) = delete;
        void operator=(NdnRtcLibrary const&) = delete;

        int notifyObserverWithError(const char *format, ...) const;
        int notifyObserverWithState(const char *stateName,
                                    const char *format, ...) const;
        void notifyObserver(const char *state, const char *args) const;
        
        
    };
}


#endif /* defined(__ndnrtc__ndnrtc_library__) */
