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
#include <boost/asio.hpp>

namespace ndn {
    class Face;
}

class FaceProcessorImpl;

class FaceProcessor {
public:
    FaceProcessor(std::string host, bool createThread = true);
    ~FaceProcessor();

    void start();
    void stop();
    bool isProcessing();
    void processEvents();

    boost::asio::io_service& getIo();
    std::shared_ptr<ndn::Face> getFace();

    // non blocking
    void dispatchSynchronized(std::function<void(std::shared_ptr<ndn::Face>)> dispatchBlock);
    // blocking
    void performSynchronized(std::function<void(std::shared_ptr<ndn::Face>)> dispatchBlock);

    static std::shared_ptr<FaceProcessor> forLocalhost();
    static bool checkNfdConnection();

private:
    std::shared_ptr<FaceProcessorImpl> _pimpl;
};

#endif /* face_processor_hpp */
