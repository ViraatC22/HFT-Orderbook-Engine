#pragma once

#include <atomic>
#include <vector>
#include <optional>

template<typename T>
class LockFreeQueue
{
public:
    explicit LockFreeQueue(size_t size) : buffer_(size), capacity_(size), head_(0), tail_(0) {}

    bool Push(const T& item)
    {
        size_t currentTail = tail_.load(std::memory_order_relaxed);
        size_t nextTail = (currentTail + 1) % capacity_;

        if (nextTail == head_.load(std::memory_order_acquire))
        {
            return false; // Full
        }

        buffer_[currentTail] = item;
        tail_.store(nextTail, std::memory_order_release);
        return true;
    }

    bool Pop(T& item)
    {
        size_t currentHead = head_.load(std::memory_order_relaxed);

        if (currentHead == tail_.load(std::memory_order_acquire))
        {
            return false; // Empty
        }

        item = buffer_[currentHead];
        head_.store((currentHead + 1) % capacity_, std::memory_order_release);
        return true;
    }

    bool IsEmpty() const
    {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }
    
    size_t Size() const
    {
        size_t head = head_.load(std::memory_order_acquire);
        size_t tail = tail_.load(std::memory_order_acquire);
        if (tail >= head) return tail - head;
        return capacity_ - (head - tail);
    }
    
    size_t Capacity() const { return capacity_; }

private:
    std::vector<T> buffer_;
    size_t capacity_;
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
};
