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
}

namespace ndnrtc {
    namespace helpers {
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

            static boost::shared_ptr<FaceProcessor> forLocalhost();
            static bool checkNfdConnection();

        private:
            boost::shared_ptr<FaceProcessorImpl> _pimpl;
        };
    }
}

#endif /* face_processor_hpp */
