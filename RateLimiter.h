#pragma once

#include <chrono>
#include <atomic>
#include <mutex>

// Token Bucket Rate Limiter
class RateLimiter
{
public:
    RateLimiter(size_t tokensPerSec, size_t bucketSize)
        : tokensPerSec_(tokensPerSec)
        , bucketSize_(bucketSize)
        , tokens_(bucketSize)
        , lastRefill_(std::chrono::steady_clock::now())
    { }

    bool TryAcquire(size_t tokens = 1)
    {
        // Simple thread-safe implementation
        // For HFT ingress, this might be lock-free, but here we use a spinlock or mutex
        std::lock_guard<std::mutex> lock(mutex_);
        
        Refill();
        
        if (tokens_ >= tokens)
        {
            tokens_ -= tokens;
            return true;
        }
        return false;
    }

private:
    void Refill()
    {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRefill_).count();
        
        if (duration > 0)
        {
            size_t newTokens = (duration * tokensPerSec_) / 1000;
            if (newTokens > 0)
            {
                tokens_ = std::min(bucketSize_, tokens_ + newTokens);
                lastRefill_ = now;
            }
        }
    }

    size_t tokensPerSec_;
    size_t bucketSize_;
    size_t tokens_;
    std::chrono::steady_clock::time_point lastRefill_;
    std::mutex mutex_;
};
