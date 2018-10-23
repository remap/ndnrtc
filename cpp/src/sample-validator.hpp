//
// sample-validator.hpp
//
//  Created by Peter Gusev on 22 July 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __sample_validator_h__
#define __sample_validator_h__

#include "ndnrtc-object.hpp"
#include "frame-buffer.hpp"
#include "statistics.hpp"

namespace ndn
{
class Face;
class KeyChain;
}

namespace ndnrtc
{
namespace statistics
{
class StatisticsStorage;
}

class MetaFetcher;

/**
 * Used for validating signed samples.
 */
class SampleValidator : public NdnRtcComponent, public IBufferObserver, statistics::StatObject
{
  public:
    SampleValidator(boost::shared_ptr<ndn::KeyChain> keyChain,
                    const boost::shared_ptr<statistics::StatisticsStorage> &statStorage) : 
                    StatObject(statStorage), keyChain_(keyChain) {}

  private:
    boost::shared_ptr<ndn::KeyChain> keyChain_;

    void onNewRequest(const boost::shared_ptr<BufferSlot> &);
    void onNewData(const BufferReceipt &receipt);
    void onReset() {}
};

/**
 * Used for validating multi-segment unsigned data, where signed manifest is published. 
 */
class ManifestValidator : public NdnRtcComponent, public IBufferObserver, statistics::StatObject
{
  public:
    ManifestValidator(boost::shared_ptr<ndn::Face> face,
                      boost::shared_ptr<ndn::KeyChain> keyChain,
                      const boost::shared_ptr<statistics::StatisticsStorage> &statStorage);

  private:
    template <typename T>
    class Pool
    {
      public:
        Pool(const size_t &capacity) : capacity_(0) { enlarge(capacity); }

        size_t capacity() const { return capacity_; }
        size_t size() const { return pool_.size(); }

        boost::shared_ptr<T> pop()
        {
            if (pool_.size())
            {
                boost::shared_ptr<T> el = pool_.back();
                pool_.pop_back();
                return el;
            }
            return boost::shared_ptr<T>();
        }

        bool push(const boost::shared_ptr<T> &el)
        {
            if (pool_.size() < capacity_)
            {
                pool_.push_back(el);
                return true;
            }
            return false;
        }

        void enlarge(const size_t &capacity)
        {
            for (int i = 0; i < capacity; ++i)
                pool_.push_back(boost::make_shared<T>());
            capacity_ += capacity;
        }

      private:
        Pool(const Pool<T> &) = delete;

        size_t capacity_;
        std::vector<boost::shared_ptr<T>> pool_;
    };

    boost::shared_ptr<ndn::Face> face_;
    boost::shared_ptr<ndn::KeyChain> keyChain_;
    Pool<MetaFetcher> metaFetcherPool_;

    void onNewRequest(const boost::shared_ptr<BufferSlot> &);
    void onNewData(const BufferReceipt &receipt);
    void onReset() {}
    void verifySlot(const boost::shared_ptr<const BufferSlot> slot);
};
}

#endif
