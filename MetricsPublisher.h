#pragma once

#include <atomic>
#include <cstdint>

// Metrics Publisher (Shared Memory Layout)
// In production, this struct would be placed in a boost::interprocess::shared_memory_object
// or mmap-ed file (/dev/shm/hft_metrics) so external agents (Prometheus exporter) can read it.
struct Metrics
{
    // Align to cache line to avoid false sharing with other potential processes (though unlikely here)
    alignas(64) std::atomic<uint64_t> ordersProcessed{0};
    alignas(64) std::atomic<uint64_t> tradesExecuted{0};
    alignas(64) std::atomic<uint64_t> currentQueueDepth{0};
    alignas(64) std::atomic<uint64_t> p99LatencyNs{0}; // Updated periodically
};

class MetricsPublisher
{
public:
    MetricsPublisher()
    {
        // Mock: In real code, shm_open /dev/shm/hft_metrics + mmap
        // Here we just own the memory.
        metrics_ = new Metrics(); 
    }
    
    ~MetricsPublisher()
    {
        delete metrics_;
    }
    
    void PublishQueueDepth(uint64_t depth)
    {
        metrics_->currentQueueDepth.store(depth, std::memory_order_relaxed);
    }
    
    void IncrementOrdersProcessed()
    {
        metrics_->ordersProcessed.fetch_add(1, std::memory_order_relaxed);
    }
    
    void PublishP99(uint64_t latency)
    {
        metrics_->p99LatencyNs.store(latency, std::memory_order_relaxed);
    }
    
    // For demo purposes, allow reading back
    uint64_t GetQueueDepth() const { return metrics_->currentQueueDepth.load(std::memory_order_relaxed); }
    uint64_t GetOrdersProcessed() const { return metrics_->ordersProcessed.load(std::memory_order_relaxed); }

private:
    Metrics* metrics_;
};
