#pragma once

#include <atomic>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <algorithm>
#include <format>
#include <cerrno>

#include "Usings.h"

/**
 * Shared Memory Observability Suite
 * 
 * Zero-impact monitoring system using atomic counters and shared memory
 * for real-time metrics without affecting matching engine performance.
 * 
 * Key features:
 * - Lock-free atomic updates from engine thread
 * - Zero-copy shared memory for external monitoring
 * - Real-time latency histograms with nanosecond precision
 * - Configurable alert thresholds and health checks
 * - Inter-process communication without system calls
 */

struct alignas(64) SharedMetrics
{
    // Core counters (updated atomically)
    std::atomic<uint64_t> orders_received;
    std::atomic<uint64_t> orders_processed;
    std::atomic<uint64_t> orders_rejected;
    std::atomic<uint64_t> trades_executed;
    std::atomic<uint64_t> total_volume;
    std::atomic<uint64_t> total_notional;
    
    // Queue metrics
    std::atomic<uint64_t> queue_depth;
    std::atomic<uint64_t> queue_drops;
    std::atomic<uint64_t> max_queue_depth;
    
    // Latency metrics (nanoseconds)
    std::atomic<uint64_t> p50_latency_ns;
    std::atomic<uint64_t> p99_latency_ns;
    std::atomic<uint64_t> p999_latency_ns;
    std::atomic<uint64_t> max_latency_ns;
    std::atomic<uint64_t> min_latency_ns;
    
    // Performance counters
    std::atomic<uint64_t> cpu_cycles;
    std::atomic<uint64_t> cache_misses;
    std::atomic<uint64_t> branch_mispredictions;
    std::atomic<uint64_t> memory_bandwidth;
    
    // System health
    std::atomic<uint64_t> uptime_seconds;
    std::atomic<uint64_t> last_heartbeat;
    std::atomic<uint8_t> health_status;  // 0=healthy, 1=warning, 2=critical
    std::atomic<uint8_t> alert_flags;
    
    // Market data
    std::atomic<Price> best_bid_price;
    std::atomic<Price> best_ask_price;
    std::atomic<Quantity> best_bid_quantity;
    std::atomic<Quantity> best_ask_quantity;
    std::atomic<uint64_t> bid_depth_levels;
    std::atomic<uint64_t> ask_depth_levels;
    
    // Memory usage
    std::atomic<uint64_t> memory_used_bytes;
    std::atomic<uint64_t> memory_peak_bytes;
    std::atomic<uint64_t> object_pool_utilization;
    
    // Reserved for future expansion
    std::atomic<uint64_t> reserved[16];
};

static_assert(alignof(SharedMetrics) == 64, "SharedMetrics must be cache-line aligned");

struct alignas(64) LatencyHistogram
{
    static constexpr size_t BUCKETS = 128;
    static constexpr size_t MAX_LATENCY_NS = 1000000000; // 1 second
    
    std::array<std::atomic<uint64_t>, BUCKETS> buckets;
    std::atomic<uint64_t> total_samples;
    std::atomic<uint64_t> sum_latency_ns;
    
    // Exponential bucket scaling for better resolution at low latencies
    [[nodiscard]] size_t get_bucket_index(uint64_t latency_ns) const
    {
        if (latency_ns == 0) return 0;
        if (latency_ns >= MAX_LATENCY_NS) return BUCKETS - 1;
        
        // Exponential scaling: more buckets for lower latencies
        double log_latency = std::log10(static_cast<double>(latency_ns));
        double log_max = std::log10(static_cast<double>(MAX_LATENCY_NS));
        
        size_t index = static_cast<size_t>((log_latency / log_max) * (BUCKETS - 2));
        return std::min(index, BUCKETS - 2);
    }
    
    void record(uint64_t latency_ns)
    {
        size_t bucket = get_bucket_index(latency_ns);
        buckets[bucket].fetch_add(1, std::memory_order_relaxed);
        total_samples.fetch_add(1, std::memory_order_relaxed);
        sum_latency_ns.fetch_add(latency_ns, std::memory_order_relaxed);
    }
    
    [[nodiscard]] uint64_t get_percentile(double p) const
    {
        uint64_t total = total_samples.load(std::memory_order_acquire);
        if (total == 0) return 0;
        
        uint64_t target = static_cast<uint64_t>(total * p);
        uint64_t cumulative = 0;
        
        for (size_t i = 0; i < BUCKETS; ++i)
        {
            cumulative += buckets[i].load(std::memory_order_acquire);
            if (cumulative >= target)
            {
                // Convert bucket index back to latency value
                if (i == 0) return 1;
                if (i == BUCKETS - 1) return MAX_LATENCY_NS;
                
                double log_ratio = static_cast<double>(i) / (BUCKETS - 2);
                double log_latency = log_ratio * std::log10(static_cast<double>(MAX_LATENCY_NS));
                return static_cast<uint64_t>(std::pow(10, log_latency));
            }
        }
        
        return MAX_LATENCY_NS;
    }
    
    void reset()
    {
        for (auto& bucket : buckets)
        {
            bucket.store(0, std::memory_order_relaxed);
        }
        total_samples.store(0, std::memory_order_relaxed);
        sum_latency_ns.store(0, std::memory_order_relaxed);
    }
};

class SharedMemoryMetrics
{
public:
    static constexpr const char* DEFAULT_SHM_NAME = "/hft_orderbook_metrics";
    static constexpr size_t SHM_SIZE = sizeof(SharedMetrics) + sizeof(LatencyHistogram);
    
    explicit SharedMemoryMetrics(const std::string& shm_name = DEFAULT_SHM_NAME)
        : shm_name_(shm_name),
          shm_fd_(-1),
          metrics_(nullptr),
          histogram_(nullptr),
          is_owner_(false)
    {
        initialize_shared_memory();
    }
    
    ~SharedMemoryMetrics()
    {
        cleanup_shared_memory();
    }
    
    // Non-blocking atomic updates (safe from engine thread)
    void IncrementOrdersReceived(uint64_t count = 1)
    {
        if (metrics_) metrics_->orders_received.fetch_add(count, std::memory_order_relaxed);
    }
    
    void IncrementOrdersProcessed(uint64_t count = 1)
    {
        if (metrics_) metrics_->orders_processed.fetch_add(count, std::memory_order_relaxed);
    }
    
    void IncrementOrdersRejected(uint64_t count = 1)
    {
        if (metrics_) metrics_->orders_rejected.fetch_add(count, std::memory_order_relaxed);
    }
    
    void IncrementTradesExecuted(uint64_t count = 1, Quantity volume = 0, Price price = 0)
    {
        if (metrics_)
        {
            metrics_->trades_executed.fetch_add(count, std::memory_order_relaxed);
            if (volume > 0) metrics_->total_volume.fetch_add(volume, std::memory_order_relaxed);
            if (price > 0 && volume > 0) 
                metrics_->total_notional.fetch_add(volume * price, std::memory_order_relaxed);
        }
    }
    
    void UpdateQueueDepth(uint64_t depth)
    {
        if (metrics_)
        {
            metrics_->queue_depth.store(depth, std::memory_order_relaxed);
            
            // Track maximum queue depth
            uint64_t current_max = metrics_->max_queue_depth.load(std::memory_order_relaxed);
            while (depth > current_max && 
                   !metrics_->max_queue_depth.compare_exchange_weak(current_max, depth))
            {
                // Retry until successful
            }
        }
    }
    
    void RecordLatency(uint64_t latency_ns)
    {
        if (histogram_) histogram_->record(latency_ns);
        
        if (metrics_)
        {
            // Update min/max latency
            uint64_t current_min = metrics_->min_latency_ns.load(std::memory_order_relaxed);
            while (latency_ns < current_min && 
                   !metrics_->min_latency_ns.compare_exchange_weak(current_min, latency_ns))
            {
                // Retry until successful
            }
            
            uint64_t current_max = metrics_->max_latency_ns.load(std::memory_order_relaxed);
            while (latency_ns > current_max && 
                   !metrics_->max_latency_ns.compare_exchange_weak(current_max, latency_ns))
            {
                // Retry until successful
            }
        }
    }
    
    void UpdateBestPrices(Price bid_price, Price ask_price, 
                         Quantity bid_qty = 0, Quantity ask_qty = 0)
    {
        if (metrics_)
        {
            if (bid_price > 0) metrics_->best_bid_price.store(bid_price, std::memory_order_relaxed);
            if (ask_price > 0) metrics_->best_ask_price.store(ask_price, std::memory_order_relaxed);
            if (bid_qty > 0) metrics_->best_bid_quantity.store(bid_qty, std::memory_order_relaxed);
            if (ask_qty > 0) metrics_->best_ask_quantity.store(ask_qty, std::memory_order_relaxed);
        }
    }
    
    void UpdateMarketDepth(uint64_t bid_levels, uint64_t ask_levels)
    {
        if (metrics_)
        {
            metrics_->bid_depth_levels.store(bid_levels, std::memory_order_relaxed);
            metrics_->ask_depth_levels.store(ask_levels, std::memory_order_relaxed);
        }
    }
    
    void UpdateMemoryUsage(uint64_t used_bytes, uint64_t peak_bytes = 0)
    {
        if (metrics_)
        {
            metrics_->memory_used_bytes.store(used_bytes, std::memory_order_relaxed);
            if (peak_bytes > 0) metrics_->memory_peak_bytes.store(peak_bytes, std::memory_order_relaxed);
        }
    }
    
    void UpdateObjectPoolUtilization(uint64_t utilization_percent)
    {
        if (metrics_) 
            metrics_->object_pool_utilization.store(utilization_percent, std::memory_order_relaxed);
    }
    
    void SetHealthStatus(uint8_t status)
    {
        if (metrics_) metrics_->health_status.store(status, std::memory_order_relaxed);
    }
    
    void SetAlertFlag(uint8_t flag)
    {
        if (metrics_)
        {
            uint8_t current = metrics_->alert_flags.load(std::memory_order_relaxed);
            metrics_->alert_flags.store(current | flag, std::memory_order_relaxed);
        }
    }
    
    void ClearAlertFlag(uint8_t flag)
    {
        if (metrics_)
        {
            uint8_t current = metrics_->alert_flags.load(std::memory_order_relaxed);
            metrics_->alert_flags.store(current & ~flag, std::memory_order_relaxed);
        }
    }
    
    void UpdateHeartbeat()
    {
        if (metrics_)
        {
            auto now = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            metrics_->last_heartbeat.store(now, std::memory_order_relaxed);
        }
    }
    
    void UpdateUptime(uint64_t seconds)
    {
        if (metrics_) metrics_->uptime_seconds.store(seconds, std::memory_order_relaxed);
    }
    
    // Get current metrics (for monitoring dashboard)
    struct MetricsSnapshot
    {
        uint64_t orders_received{};
        uint64_t orders_processed{};
        uint64_t orders_rejected{};
        uint64_t trades_executed{};
        uint64_t total_volume{};
        uint64_t total_notional{};
        
        uint64_t queue_depth{};
        uint64_t max_queue_depth{};
        
        Price best_bid_price{};
        Price best_ask_price{};
        Quantity best_bid_quantity{};
        Quantity best_ask_quantity{};
        
        uint8_t health_status{};
        uint8_t alert_flags{};
        
        uint64_t uptime_seconds{};
        uint64_t last_heartbeat{};
        
        uint64_t memory_used_bytes{};
        uint64_t memory_peak_bytes{};
        uint64_t object_pool_utilization{};
    };
    
    [[nodiscard]] MetricsSnapshot GetSnapshot() const
    {
        MetricsSnapshot snapshot{};
        if (metrics_)
        {
            snapshot.orders_received = metrics_->orders_received.load(std::memory_order_acquire);
            snapshot.orders_processed = metrics_->orders_processed.load(std::memory_order_acquire);
            snapshot.orders_rejected = metrics_->orders_rejected.load(std::memory_order_acquire);
            snapshot.trades_executed = metrics_->trades_executed.load(std::memory_order_acquire);
            snapshot.total_volume = metrics_->total_volume.load(std::memory_order_acquire);
            snapshot.total_notional = metrics_->total_notional.load(std::memory_order_acquire);
            snapshot.queue_depth = metrics_->queue_depth.load(std::memory_order_acquire);
            snapshot.max_queue_depth = metrics_->max_queue_depth.load(std::memory_order_acquire);
            snapshot.best_bid_price = metrics_->best_bid_price.load(std::memory_order_acquire);
            snapshot.best_ask_price = metrics_->best_ask_price.load(std::memory_order_acquire);
            snapshot.best_bid_quantity = metrics_->best_bid_quantity.load(std::memory_order_acquire);
            snapshot.best_ask_quantity = metrics_->best_ask_quantity.load(std::memory_order_acquire);
            snapshot.health_status = metrics_->health_status.load(std::memory_order_acquire);
            snapshot.alert_flags = metrics_->alert_flags.load(std::memory_order_acquire);
            snapshot.uptime_seconds = metrics_->uptime_seconds.load(std::memory_order_acquire);
            snapshot.last_heartbeat = metrics_->last_heartbeat.load(std::memory_order_acquire);
            snapshot.memory_used_bytes = metrics_->memory_used_bytes.load(std::memory_order_acquire);
            snapshot.memory_peak_bytes = metrics_->memory_peak_bytes.load(std::memory_order_acquire);
            snapshot.object_pool_utilization = metrics_->object_pool_utilization.load(std::memory_order_acquire);
        }
        return snapshot;
    }
    
    struct LatencyHistogramSnapshot
    {
        std::array<uint64_t, LatencyHistogram::BUCKETS> buckets{};
        uint64_t total_samples{};
        uint64_t sum_latency_ns{};
    };
    
    [[nodiscard]] LatencyHistogramSnapshot GetLatencyHistogram() const
    {
        LatencyHistogramSnapshot snapshot{};
        if (!histogram_) return snapshot;
        
        for (size_t i = 0; i < LatencyHistogram::BUCKETS; ++i)
        {
            snapshot.buckets[i] = histogram_->buckets[i].load(std::memory_order_acquire);
        }
        snapshot.total_samples = histogram_->total_samples.load(std::memory_order_acquire);
        snapshot.sum_latency_ns = histogram_->sum_latency_ns.load(std::memory_order_acquire);
        return snapshot;
    }
    
    // Calculate latency percentiles from histogram
    [[nodiscard]] std::pair<uint64_t, uint64_t> GetLatencyPercentiles(double p1, double p2) const
    {
        if (!histogram_) return {0, 0};
        
        uint64_t lat1 = histogram_->get_percentile(p1);
        uint64_t lat2 = histogram_->get_percentile(p2);
        
        return {lat1, lat2};
    }
    
    // Reset metrics (useful for testing or periodic reset)
    void Reset()
    {
        if (metrics_)
        {
            // Reset all atomic counters
            metrics_->orders_received.store(0, std::memory_order_relaxed);
            metrics_->orders_processed.store(0, std::memory_order_relaxed);
            metrics_->orders_rejected.store(0, std::memory_order_relaxed);
            metrics_->trades_executed.store(0, std::memory_order_relaxed);
            metrics_->total_volume.store(0, std::memory_order_relaxed);
            metrics_->total_notional.store(0, std::memory_order_relaxed);
            metrics_->queue_depth.store(0, std::memory_order_relaxed);
            metrics_->queue_drops.store(0, std::memory_order_relaxed);
            metrics_->max_queue_depth.store(0, std::memory_order_relaxed);
            metrics_->memory_used_bytes.store(0, std::memory_order_relaxed);
            metrics_->memory_peak_bytes.store(0, std::memory_order_relaxed);
            metrics_->object_pool_utilization.store(0, std::memory_order_relaxed);
            metrics_->alert_flags.store(0, std::memory_order_relaxed);
        }
        
        if (histogram_)
        {
            histogram_->reset();
        }
    }
    
    // Check if shared memory is available and accessible
    [[nodiscard]] bool IsHealthy() const
    {
        return metrics_ != nullptr && histogram_ != nullptr;
    }
    
private:
    void initialize_shared_memory()
    {
        // Create or open shared memory segment
        shm_fd_ = shm_open(shm_name_.c_str(), O_CREAT | O_RDWR, 0666);
        if (shm_fd_ < 0)
        {
            throw std::runtime_error(std::format("Failed to create shared memory: {}", strerror(errno)));
        }
        
        // Set size
        if (ftruncate(shm_fd_, SHM_SIZE) < 0)
        {
            close(shm_fd_);
            throw std::runtime_error(std::format("Failed to set shared memory size: {}", strerror(errno)));
        }
        
        // Map into memory
        void* shm_ptr = mmap(nullptr, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
        if (shm_ptr == MAP_FAILED)
        {
            close(shm_fd_);
            throw std::runtime_error(std::format("Failed to map shared memory: {}", strerror(errno)));
        }
        
        // Split memory into metrics and histogram sections
        metrics_ = static_cast<SharedMetrics*>(shm_ptr);
        histogram_ = reinterpret_cast<LatencyHistogram*>(static_cast<char*>(shm_ptr) + sizeof(SharedMetrics));
        
        // Initialize if this is the first time
        if (metrics_->orders_received.load(std::memory_order_acquire) == 0)
        {
            // First initialization - zero everything
            std::memset(shm_ptr, 0, SHM_SIZE);
            is_owner_ = true;
            
            // Set initial values
            UpdateHeartbeat();
            SetHealthStatus(0); // Healthy
        }
    }
    
    void cleanup_shared_memory()
    {
        if (shm_fd_ >= 0)
        {
            munmap(metrics_, SHM_SIZE);
            close(shm_fd_);
            
            // Only unlink if we're the owner
            if (is_owner_)
            {
                shm_unlink(shm_name_.c_str());
            }
        }
    }
    
private:
    std::string shm_name_;
    int shm_fd_;
    SharedMetrics* metrics_;
    LatencyHistogram* histogram_;
    bool is_owner_;
};

// Alert flag definitions
namespace AlertFlags
{
    constexpr uint8_t HIGH_LATENCY = 1 << 0;
    constexpr uint8_t HIGH_QUEUE_DEPTH = 1 << 1;
    constexpr uint8_t HIGH_REJECT_RATE = 1 << 2;
    constexpr uint8_t MEMORY_PRESSURE = 1 << 3;
    constexpr uint8_t PACKET_LOSS = 1 << 4;
    constexpr uint8_t SYSTEM_OVERLOAD = 1 << 5;
    constexpr uint8_t HEARTBEAT_MISSED = 1 << 6;
    constexpr uint8_t CONFIG_ERROR = 1 << 7;
}

// Health status definitions
namespace HealthStatus
{
    constexpr uint8_t HEALTHY = 0;
    constexpr uint8_t WARNING = 1;
    constexpr uint8_t CRITICAL = 2;
    constexpr uint8_t FATAL = 3;
}
