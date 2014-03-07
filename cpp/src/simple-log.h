//
//  simple-log.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/8/13
//

#ifndef __ndnrtc__simple__
#define __ndnrtc__simple__

#include <iostream>
#include <pthread.h>
#include <string>
#include <map>

#if !defined(NDN_LOGGING)
#undef NDN_TRACE
#undef NDN_INFO
#undef NDN_WARN
#undef NDN_ERROR
#undef NDN_DEBUG
#endif

// if defined detailed logging - print whole signature of the function.
//#if defined(NDN_DETAILED)
#define __NDN_FNAME__ __PRETTY_FUNCTION__
//#else
//#define __NDN_FNAME__ __func__
//#endif

// following macros are used for NdnRtcObject logging
// each macro checks, whether a logger, associated with object has been
// initialized and use it instead of global logger
#if defined (NDN_TRACE) //&& defined(DEBUG)
#define TRACE(fmt, ...) if (this->logger_) this->logger_->log(NdnLoggerLevelTrace, fmt, ##__VA_ARGS__); \
else NdnLogger::log(__NDN_FNAME__, NdnLoggerLevelTrace, fmt, ##__VA_ARGS__)
#define LogTrace(fname, ...) ndnlog::new_api::Logger::log(fname, NdnLoggerLevelTrace, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define TRACE(fmt, ...)
#define LogTrace(fname, ...)
#endif

#if defined (NDN_DEBUG) //&& defined(DEBUG)
#define DBG(fmt, ...) if (this->logger_) this->logger_->log(NdnLoggerLevelDebug, fmt, ##__VA_ARGS__); \
else NdnLogger::log(__NDN_FNAME__, NdnLoggerLevelDebug, fmt, ##__VA_ARGS__)

#define LogDebug(fname, ...) ndnlog::new_api::Logger::log(fname, NdnLoggerLevelDebug, __FILE__, __LINE__, ##__VA_ARGS__)

#else
#define DBG(fmt, ...)
#define LogDebug(fmt, ...)
#endif

#if defined (NDN_INFO)
#define INFO(fmt, ...) if (this->logger_) this->logger_->log(NdnLoggerLevelInfo, fmt, ##__VA_ARGS__); \
else NdnLogger::log(__NDN_FNAME__, NdnLoggerLevelInfo, fmt, ##__VA_ARGS__)

#define LogInfo(fname, ...) ndnlog::new_api::Logger::log(fname, NdnLoggerLevelInfo, __FILE__, __LINE__, ##__VA_ARGS__)

#else

#define INFO(fmt, ...)
#define LogInfo(fname, ...)

#endif

#if defined (NDN_WARN)
#define WARN(fmt, ...) if (this->logger_) this->logger_->log(NdnLoggerLevelWarning, fmt, ##__VA_ARGS__); \
else NdnLogger::log(__NDN_FNAME__, NdnLoggerLevelWarning, fmt, ##__VA_ARGS__)

#define LogWarn(fname, ...) ndnlog::new_api::Logger::log(fname, NdnLoggerLevelWarning   , __FILE__, __LINE__, ##__VA_ARGS__)


#else
#define WARN(fmt, ...)
#define LogWarn(fname, ...)
#endif

#if defined (NDN_ERROR)
#define NDNERROR(fmt, ...) if (this->logger_) this->logger_->log(NdnLoggerLevelError, fmt, ##__VA_ARGS__); \
else NdnLogger::log(__NDN_FNAME__, NdnLoggerLevelError, fmt, ##__VA_ARGS__)

#define LogError(fname, ...) ndnlog::new_api::Logger::log(fname, NdnLoggerLevelInfo, __FILE__, __LINE__, ##__VA_ARGS__)

#else
#define NDNERROR(fmt, ...)
#define LogError(fname, ...)
#endif

// following macros are used for logging usign global logger
#if defined (NDN_TRACE) //&& defined(DEBUG)
#define LOG_TRACE(fmt, ...) NdnLogger::log(__NDN_FNAME__, NdnLoggerLevelTrace, fmt, ##__VA_ARGS__)
#else
#define LOG_TRACE(fmt, ...)
#endif

#if defined (NDN_DEBUG) //&& defined(DEBUG)
#define LOG_DBG(fmt, ...) NdnLogger::log(__NDN_FNAME__, NdnLoggerLevelDebug, fmt, ##__VA_ARGS__)
#else
#define LOG_DBG(fmt, ...)
#endif

#if defined (NDN_INFO)
#define LOG_INFO(fmt, ...) NdnLogger::log(__NDN_FNAME__, NdnLoggerLevelInfo, fmt, ##__VA_ARGS__)
#else
#define LOG_INFO(fmt, ...)
#endif

#if defined (NDN_WARN)
#define LOG_WARN(fmt, ...) NdnLogger::log(__NDN_FNAME__, NdnLoggerLevelWarning, fmt, ##__VA_ARGS__)
#else
#define LOG_WARN(fmt, ...)
#endif

#if defined (NDN_ERROR)
#define LOG_NDNERROR(fmt, ...) NdnLogger::log(__NDN_FNAME__, NdnLoggerLevelError, fmt, ##__VA_ARGS__)
#else
#define LOG_NDNERROR(fmt, ...)
#endif

namespace ndnlog {
    typedef enum _NdnLoggerLevel {
        NdnLoggerLevelTrace = 0,
        NdnLoggerLevelDebug = 1,
        NdnLoggerLevelInfo = 2,
        NdnLoggerLevelWarning = 3,
        NdnLoggerLevelError = 4
    } NdnLoggerLevel;
    
    typedef NdnLoggerLevel NdnLogType;
    
    typedef enum _NdnLoggerDetailLevel {
        NdnLoggerDetailLevelNone = NdnLoggerLevelError+1,
        NdnLoggerDetailLevelDefault = NdnLoggerLevelInfo,
        NdnLoggerDetailLevelDebug = NdnLoggerLevelDebug,
        NdnLoggerDetailLevelAll = NdnLoggerLevelTrace
    } NdnLoggerDetailLevel;
    
    /**
     * Thread-safe logger class to stdout or file
     */
    class NdnLogger
    {
    public:
        // construction/desctruction
        NdnLogger(NdnLoggerDetailLevel logDetailLevel, const char *logFileFmt, ...);
        NdnLogger(const char *logFile = NULL, NdnLoggerDetailLevel logDetailLevel = NdnLoggerDetailLevelDefault);
        ~NdnLogger();
        
        // public static attributes go here
        
        // public static methods go here
        static void initialize(const char *logFile, NdnLoggerDetailLevel logDetailLevel);
        static void log(const char *fName, NdnLoggerLevel level, const char *format, ...);
        static std::string currentLogFile(); // returns "" if log to stdout
        static NdnLoggerDetailLevel currentLogLevel();
        static NdnLogger *sharedInstance() { return getInstance(); }

        // public attributes go here
        
        // public methods go here 
        void logString(const char *str);
        void log(NdnLoggerLevel level, const char *format, ...);

    private:
        // private static attributes go here
        
        // private static methods go here
        static NdnLogger* getInstance();
        static const char* stingify(NdnLoggerLevel lvl);
        
        // private attributes go here
        FILE *outLogStream_;
        char *buf_;
        NdnLoggerDetailLevel loggingDetailLevel_;
        std::string logFile_;
        int64_t lastFileFlush_;
        
        static pthread_mutex_t logMutex_;
        pthread_mutex_t instanceMutex_;
        
        // private methods go here
        void log(const char *str);
        void flushBuffer(char *buf);
        char* getBuffer(){ return buf_; }
        NdnLoggerDetailLevel getLoggingDetailLevel() { return loggingDetailLevel_; }
        int64_t millisecondTimestamp();
    };
    
    /**
     * Abstract class for an object which can print logs into its' logger if 
     * it is initialized
     */
    class LoggerObject
    {
    public:
        
        /**
         * Default constructor
         */
        LoggerObject(){}
        
        /**
         * Initializes object and creates a logger to file using file name 
         * provided. Logger will be deleted in destruction process.
         *
         * @param logFile Log file name. File could exist or not, depending on
         * case, logger will overwrite existing or create a new file
         */
        LoggerObject(const char *logFile) : isLoggerCreated_(true) {
            logger_ = new NdnLogger(logFile);
        }
        
        /**
         * Initializes object with existing logger. Logger will not be deleted 
         * in destruction process.
         * 
         * @param logger Logger which will be used for logging of the object
         * being created
         */
        LoggerObject(NdnLogger *logger) : logger_(logger) {}
        
        virtual ~LoggerObject() {
            if (isLoggerCreated_)
                delete logger_;
        }
        
        /**
         * Returns current logger of the object
         * @return Current logger
         */
        NdnLogger* getLogger() { return logger_; }
        
        /**
         * Sets a logger only if it was not previously created. This logger 
         * will not be deleted upon object destruction. Otherwise, deletes 
         * previous logger and sets a new one provided.
         * @param logger Logger which will be used for logging by the object
         */
        virtual void setLogger(NdnLogger *logger) {
            if (isLoggerCreated_)
                delete logger_;

            isLoggerCreated_ = false;
            logger_ = logger;
        }
        
    protected:
        bool isLoggerCreated_ = false;
        ndnlog::NdnLogger *logger_ = nullptr;
        
        /**
         * Initializes logger with filename composed by parameters passed. 
         * Resulting filename should not exceed 256 symbols.
         */
//        void initializeLogger(const char *format, ...);
    };
    
    namespace new_api
    {
        // interval at which logger is flushing data on disk (if file logging
        // was chosen)
        const unsigned int FlushIntervalMs = 100;
        
        class ILoggingObject;
        
        /**
         * Logger object. Performs thread-safe logging into a file or standard 
         * output.
         */
        class Logger
        {
        private:
            using endl_type = std::ostream&(std::ostream&);
            
        public:
            /**
             * Creates an instance of logger with specified logging level and 
             * file name. By default, creates logger wich logs everything into 
             * the console.
             * @param logLevel Minimal logging level for current instance. Eny 
             * log wich has level less than specified will br ignored.
             * @param logFileFmt Format for the logging file name. If NULL - 
             * logging is performed into standard output.
             */
            Logger(const NdnLoggerDetailLevel& logLevel = NdnLoggerDetailLevelAll,
                   const std::string& logFile = "");
            /**
             * Releases current instance and all associated resources
             */
            ~Logger();
            
            /**
             * Starts new logging entry with the header. Logging entry
             * can be completed with the "endl" function or by another new call
             * to this method (i.e. calling "<<" will append to the existing
             * logging entry initiated by the last "log" call).
             * @param logType Current logging type
             * @param loggingInstance Pointer to the instance of ILoggingObject
             * interface which performs logging
             * @param locationFile Name of the source file from which logging is
             * performed
             * @param locationLine Line number from the source file from which
             * logging is performed
             * @return A refernce to the current Logger instance
             */
            Logger&
            log(const NdnLogType& logType,
                const ILoggingObject* loggingInstance = nullptr,
                const std::string& locationFile = "",
                const int& locationLine = -1);
            
            /**
             * Stream operator << implementation
             * Any consequent call appends data to the previously created log 
             * entry. Specific log entry can be created using "log" call,
             * otherwise - if no previous log call was made, calling this 
             * operator creates a new deafult log entry with INFO log level and 
             * with no logging instance (set to nullptr) which results in poor 
             * undetailed logging. However can be usfeul for system-wide logging.
             */
            template<typename T>
            Logger& operator<< (const T& data)
            {
                if (isWritingLogEntry_ &&
                    currentEntryLogType_ >= (NdnLogType)logLevel_)
                {
                    getOutStream() << data;
                }
                
                return *this;
            }
            
            /**
             * Overload for std::endl explicitly
             * Closes current logging entry
             */
            Logger& operator<< (endl_type endl)
            {
                if (isWritingLogEntry_ &&
                    currentEntryLogType_ >= (NdnLogType)logLevel_)
                {
                    isWritingLogEntry_ = false;
                    currentEntryLogType_ = NdnLoggerLevelTrace;
                    
                    getOutStream() << endl;
                    unlockStream();
                }
                
                return *this;
            }
            
            void
            setLogLevel(const NdnLoggerDetailLevel& logLevel)
            { logLevel_ = logLevel; }
            
            static Logger&
            getLogger(const std::string &logFile);
            
            static void
            destroyLogger(const std::string &logFile);
            
            static Logger& log(const std::string &logFile,
                               const NdnLogType& logType,
                               const std::string& locationFile = "",
                               const int& locationLine = -1,
                               const ILoggingObject* loggingInstance = nullptr)
            {
                return getLogger(logFile).log(logType, loggingInstance,
                                       locationFile, locationLine);
            }
            
        private:
            NdnLoggerDetailLevel logLevel_;
            std::string logFile_;
            std::ostream* outStream_;
            int64_t lastFlushTimestampMs_;
            
            bool isWritingLogEntry_;
            NdnLogType currentEntryLogType_;
            pthread_t current_;
            
            pthread_mutex_t logMutex_;
            static pthread_mutex_t stdOutMutex_;
            
            static std::map<std::string, Logger*> loggers_;

            std::ostream&
            getOutStream() { return *outStream_; }
            
            std::ofstream&
            getOutFileStream()
            {
                std::ofstream *fstream = reinterpret_cast<std::ofstream*>(outStream_);
                return *fstream;
            }
            
            int64_t
            getMillisecondTimestamp();
            
            static std::string
            stringify(NdnLoggerLevel lvl);
            
            void
            lockStreamExclusively()
            {
                if (getOutStream() == std::cout)
                    pthread_mutex_lock(&stdOutMutex_);
                else
                    pthread_mutex_lock(&logMutex_);
            }
            
            void unlockStream()
            {
                if (getOutStream() == std::cout)
                    pthread_mutex_unlock(&stdOutMutex_);
                else
                    pthread_mutex_unlock(&logMutex_);
            }
        };
        
        /**
         * Interface for logging objects - instances, that can do logging 
         * should implement this interface
         */
        class ILoggingObject
        {
        public:
            /**
             * Instance short description
             * @return Description of the current instance - should contain 
             * short string representing just enough information about the 
             * instance, which preforms logging
             */
            virtual std::string getDescription() const = 0;
            
            virtual bool isLoggingEnabled() const
            {
                return true;
            }
        };
        
        /**
         * Loggin function
         * @param
         */

    }
}

#endif /* defined(__ndnrtc__simple__) */
