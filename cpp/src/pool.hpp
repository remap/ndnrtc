//
//  pool.hpp
//  ndnrtc
//
//  Created by Peter Gusev on 3/15/19.
//  Copyright 2019 Regents of the University of California
//

#ifndef __pool_hpp__
#define __pool_hpp__

#include <stdio.h>
#include <vector>
#include <type_traits>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

namespace ndnrtc
{

class IPoolObject {
public:
    IPoolObject(){}
    virtual ~IPoolObject(){}
    virtual void clear() = 0;
};

template<typename T>
class Pool {
    static_assert(std::is_base_of<IPoolObject, T>::value,
                  "T must derive from IPoolObject");
public:
    Pool(const size_t& capacity = 300)
    {
        capacity_ = capacity;
        for (int i = 0; i < capacity; ++i)
            pool_.push_back(boost::make_shared<T>());
    }
    
    boost::shared_ptr<T> pop()
    {
        if (pool_.size())
        {
            boost::shared_ptr<T> slot = pool_.back();
            pool_.pop_back();
            return slot;
        }
        
        return boost::shared_ptr<T>();
    }
    
    bool push(const boost::shared_ptr<T>& slot)
    {
        if (pool_.size() < capacity_)
        {
            slot->clear();
            pool_.push_back(slot);
            return true;
        }
        
        return false;
    }
    
    size_t capacity() const { return capacity_; }
    size_t size() const { return pool_.size(); }
    
private:
    Pool(const Pool&) = delete;
    
    size_t capacity_;
    std::vector<boost::shared_ptr<T>> pool_;
};
    
}

#endif
