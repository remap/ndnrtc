//
//  interest-queue.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__interest_queue__
#define __ndnrtc__interest_queue__

#include "ndnrtc-common.h"
#include "ndn-assembler.h"
#include "ndnrtc-utils.h"
#include "face-wrapper.h"
#include "statistics.h"

namespace ndnrtc {
    namespace new_api {
        class Consumer;
        
        class InterestQueue : public ndnlog::new_api::ILoggingObject
        {
        public:
            
            class IPriority
            {
                class QueueEntry;
                friend QueueEntry;
                friend InterestQueue;
            public:
                virtual int64_t getValue() const = 0;
                
                virtual int64_t getEnqueueTimestamp() const = 0;
                virtual int64_t getExpressionTimestamp() const  = 0;

                class Comparator
                {
                public:
                    Comparator(bool inverted):inverted_(inverted){}
                    
                    bool operator() (const IPriority& p1,
                                     const IPriority& p2) const
                    {
                        return inverted_^(p1.getValue() < p2.getValue());
                    }
                    
                private:
                    bool inverted_;
                };
                
            protected:
                virtual void setEnqueueTimestamp(int64_t timestamp) = 0;
                virtual void setExpressionTimestamp(int64_t timestamp) = 0;
            };

            InterestQueue(const ndn::ptr_lib::shared_ptr<FaceWrapper> &face);
            ~InterestQueue();
            
            void enqueueInterest(const ndn::Interest& interest,
                                 const ndn::ptr_lib::shared_ptr<IPriority>& priority,
                                 const ndn::OnData& onData = Assembler::defaultOnDataHandler(),
                                 const ndn::OnTimeout& onTimeout = Assembler::defaultOnTimeoutHandler());
            
            void
            getStatistics(ReceiverChannelPerformance& stat);
            
        private:
            class QueueEntry : public IPriority
            {
                friend InterestQueue;
            public:
                QueueEntry(){}
                QueueEntry(const ndn::Interest& interest,
                           const ndn::ptr_lib::shared_ptr<IPriority>& priority,
                           const ndn::OnData& onData,
                           const ndn::OnTimeout& onTimeout);

                int64_t
                getValue() const { return priority_->getValue(); }
                
                int64_t
                getEnqueueTimestamp() const
                { return priority_->getEnqueueTimestamp(); }
                
                int64_t
                getExpressionTimestamp() const
                { return priority_->getExpressionTimestamp(); }
                
                IPriority
                getPriority() { return *priority_; }
                
                QueueEntry& operator=(const QueueEntry& entry)
                {
                    interest_.reset(new ndn::Interest(*entry.interest_));
                    priority_  = entry.priority_;
                    onDataCallback_ = entry.onDataCallback_;
                    onTimeoutCallback_ = entry.onTimeoutCallback_;
                    
                    return *this;
                }
                
            private:
                ndn::ptr_lib::shared_ptr<const ndn::Interest> interest_;
                ndn::ptr_lib::shared_ptr<IPriority> priority_;
                ndn::OnData onDataCallback_;
                ndn::OnTimeout onTimeoutCallback_;
                
                void
                setEnqueueTimestamp(int64_t timestamp) {
                    priority_->setEnqueueTimestamp(timestamp);
                }
                void
                setExpressionTimestamp(int64_t timestamp) {
                    priority_->setExpressionTimestamp(timestamp);
                }
            };
            
            typedef std::priority_queue<QueueEntry, std::vector<QueueEntry>, IPriority::Comparator>
            PriorityQueue;
            
            unsigned int freqMeterId_;
            ndn::ptr_lib::shared_ptr<FaceWrapper> face_;
            webrtc::RWLockWrapper &queueAccess_;
            webrtc::EventWrapper &queueEvent_;
            webrtc::ThreadWrapper &queueWatchingThread_;
            PriorityQueue queue_;
            
            bool isWatchingQueue_ = false;
            
            static bool
            watchThreadRoutine(void* interestQueue)
            {
                return ((InterestQueue*)interestQueue)->watchQueue();
            }
            
            bool
            watchQueue();
            
            void
            startQueueWatching();
            
            void
            stopQueueWatching();
            
            void
            processEntry(const QueueEntry &entry);
        };
        
        class Priority : public InterestQueue::IPriority
        {
        public:
            Priority(const Priority& p)
            {
                createdMs_ = NdnRtcUtils::millisecondTimestamp();
                arrivalDelayMs_ = p.getArrivalDelay();
            }
            
            int64_t getValue() const
            {
                return getArrivalDeadlineFromEnqueue() -
                        NdnRtcUtils::millisecondTimestamp();
            }
            
            int64_t getArrivalDelay() const { return arrivalDelayMs_; }
            int64_t getCreationTimestamp() const { return createdMs_; }
            
            int64_t getEnqueueTimestamp() const { return enqueuedMs_; }
            int64_t getExpressionTimestamp() const { return expressedMs_; }
            
            static ndn::ptr_lib::shared_ptr<Priority>
            fromArrivalDelay(int64_t millisecondsFromNow)
            {
                return ndn::ptr_lib::shared_ptr<Priority>(new Priority(millisecondsFromNow));
            }
            
        private:
            int64_t createdMs_ = -1, enqueuedMs_ = -1,
                    expressedMs_ = -1, arrivalDelayMs_ = -1;
            
            Priority(int64_t arrivalDelay):arrivalDelayMs_(arrivalDelay) {
                createdMs_ = NdnRtcUtils::millisecondTimestamp();
            }
            
            void setEnqueueTimestamp(int64_t timestamp)
            { enqueuedMs_ = timestamp; }
            
            void setExpressionTimestamp(int64_t timestamp)
            { expressedMs_ = timestamp; }
            
            int64_t getArrivalDeadlineFromEnqueue() const
            {
                if (enqueuedMs_ < 0)
                    return -1;
                return enqueuedMs_+arrivalDelayMs_;
            }
            
            int64_t getArrivalDedalineFromExpression() const
            {
                if (expressedMs_ < 0)
                    return -1;
                return expressedMs_+arrivalDelayMs_;
            }
            
            int64_t getArrivalDelayFromNow() const
            {
                return NdnRtcUtils::millisecondTimestamp()-
                getArrivalDeadlineFromEnqueue();
            }
        };
        
    }
}

#endif /* defined(__ndnrtc__interest_queue__) */
