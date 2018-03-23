//
//  face-processor.hpp
//  NDNCon
//
//  Created by Peter Gusev on 2/24/17.
//  Copyright © 2017 UCLA. All rights reserved.
//

#ifndef face_processor_hpp
#define face_processor_hpp

#include <stdio.h>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/asio.hpp>

namespace ndn {
    class Face;
    class Name;
    class Interest;
    class InterestFilter;
}

namespace ndnrtc {
    namespace helpers {
        typedef boost::function<void
            (const boost::shared_ptr<const ndn::Name>& prefix,
             const boost::shared_ptr<const ndn::Interest>& interest, 
             ndn::Face& face, uint64_t interestFilterId,
             const boost::shared_ptr<const ndn::InterestFilter>& filter)> OnInterestCallback;

        typedef boost::function<void
            (const boost::shared_ptr<const ndn::Name>& prefix)> OnRegisterFailed;

        typedef boost::function<void
            (const boost::shared_ptr<const ndn::Name>& prefix, 
             uint64_t registeredPrefixId)> OnRegisterSuccess;

        class FaceProcessorImpl;

        class FaceProcessor {
        public:
            FaceProcessor(std::string host);
            ~FaceProcessor();

            void start();
            void stop();
            bool isProcessing();

            boost::asio::io_service& getIo();
            boost::shared_ptr<ndn::Face> getFace();

            // non blocking
            void dispatchSynchronized(boost::function<void(boost::shared_ptr<ndn::Face>)> dispatchBlock);
            // blocking
            void performSynchronized(boost::function<void(boost::shared_ptr<ndn::Face>)> dispatchBlock);

            // helper method for registering a prefix on the face
            void registerPrefix(const ndn::Name& prefix, 
                const OnInterestCallback& onInterest,
                const OnRegisterFailed& onRegisterFailed,
                const OnRegisterSuccess& onRegisterSuccess);

            // synchronizer prefix registration - calller will block until 
            // receiving either onRegisterFailed or onRegisterSuccess callback 
            void registerPrefixBlocking(const ndn::Name& prefix, 
                const OnInterestCallback& onInterest,
                const OnRegisterFailed& onRegisterFailed,
                const OnRegisterSuccess& onRegisterSuccess);


            static boost::shared_ptr<FaceProcessor> forLocalhost();
            static bool checkNfdConnection();

        private:
            boost::shared_ptr<FaceProcessorImpl> _pimpl;
        };
    }
}

#endif /* face_processor_hpp */
