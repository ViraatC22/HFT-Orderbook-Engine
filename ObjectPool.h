#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <concepts>
#include <memory_resource>

template<typename T>
class ObjectPool
{
public:
    using ObjectPtr = std::shared_ptr<T>;

    ObjectPool(size_t initialSize = 10000) 
        // Default init for resource_ (uses heap)
    {
        pool_.reserve(initialSize);
        for (size_t i = 0; i < initialSize; ++i)
        {
             // Let's try standard allocation to debug.
             T* obj = new T(); 
             pool_.push_back(std::shared_ptr<T>(obj)); // Standard deleter
        }
    }

    ObjectPtr Acquire()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (pool_.empty())
        {
            // Standard alloc for fallback
            T* obj = new T();
            return std::shared_ptr<T>(obj);
        }

        ObjectPtr obj = pool_.back();
        pool_.pop_back();
        return obj;
    }

    void Release(ObjectPtr obj)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push_back(obj);
    }

private:
    // std::vector<std::byte> buffer_; // Removed backing store to let PMR manage heap
    std::pmr::monotonic_buffer_resource resource_;
    
    std::vector<ObjectPtr> pool_;
    std::mutex mutex_;
};
