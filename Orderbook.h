#pragma once

#include <map>
#include <unordered_map>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <variant>
#include <atomic>

#include "Usings.h"
#include "Order.h"
#include "OrderModify.h"
#include "OrderbookLevelInfos.h"
#include "Trade.h"
#include "LockFreeQueue.h"
#include "ObjectPool.h"
#include "RiskManager.h"
#include "SimdPriceMatcher.h"
#include "FlatPriceMap.h"
#include "Journaler.h"
#include "RateLimiter.h"
#include "MetricsPublisher.h"
#include <vector>
#include <algorithm>

class Orderbook
{
public:
    struct alignas(64) Request
    {
        enum class Type { Add, Cancel, Modify };
        Type type;
        OrderPointer order{ nullptr };
        OrderId orderId{ 0 };
        OrderModify modify{ 0, Side::Buy, 0, 0 };
        uint64_t timestamp{ 0 }; // For latency tracking
    };

private:
    struct OrderEntry
    {
        OrderPointer order_{ nullptr };
        OrderPointers::iterator location_;
    };

    struct LevelData
    {
        Quantity quantity_{ };
        Quantity count_{ };

        enum class Action
        {
            Add,
            Remove,
            Match,
        };
    };

    std::unordered_map<Price, LevelData> data_;
    std::map<Price, OrderPointers, std::greater<Price>> bids_;
    std::map<Price, OrderPointers, std::less<Price>> asks_;
    
    // SIMD Matchers (Shadowing the maps for fast scan)
    // SimdPriceMatcher bidMatcher_;
    // SimdPriceMatcher askMatcher_;
    
    // FlatPriceMap bidMap_;
    // FlatPriceMap askMap_;
    
    std::unordered_map<OrderId, OrderEntry> orders_;
    
    // Concurrency & Event Loop
    LockFreeQueue<Request> requestQueue_;
    ObjectPool<Order> orderPool_;
    std::thread processingThread_;
    std::atomic<bool> shutdown_{ false };
    
    RiskManager riskManager_;
    // AsyncJournaler journaler_{"events.log"};
    // RateLimiter rateLimiter_{2000000, 100000}; // 2M MPS, 100k burst
    // MetricsPublisher metrics_;
    
    // Latency Stats (using simple vector for histogram/percentiles)
    // In production, use HdrHistogram
    std::vector<uint64_t> latencies_;
    
    // Pruning (Keep on separate thread or integrate? For simplicity, integrate into loop or keep separate but careful)
    // Actually, Pruning GFD orders is time-based. In an event loop, we can check time every N iterations.
    // For now, let's keep the logic simple and remove the separate pruning thread to avoid locking issues.
    
    void ProcessRequests();
    
    void PruneGoodForDayOrders(); // Now called from main loop
    void CancelOrders(OrderIds orderIds);
    void CancelOrderInternal(OrderId orderId);

    void OnOrderCancelled(OrderPointer order);
    void OnOrderAdded(OrderPointer order);
    void OnOrderMatched(Price price, Quantity quantity, bool isFullyFilled);
    void UpdateLevelData(Price price, Quantity quantity, LevelData::Action action);

    bool CanFullyFill(Side side, Price price, Quantity quantity) const;
    bool CanMatch(Side side, Price price) const;
    Trades MatchOrders();

    // Internal handlers for requests
    Trades HandleAddOrder(OrderPointer order);
    void HandleCancelOrder(OrderId orderId);
    Trades HandleModifyOrder(OrderModify order);

public:

    Orderbook();
    Orderbook(const Orderbook&) = delete;
    void operator=(const Orderbook&) = delete;
    Orderbook(Orderbook&&) = delete;
    void operator=(Orderbook&&) = delete;
    ~Orderbook();

    // These now push to queue
    void AddOrder(OrderPointer order);
    void CancelOrder(OrderId orderId);
    void ModifyOrder(OrderModify order);

    // Getters need to be careful now as they read from a moving target. 
    // In a real lock-free engine, we'd use a snapshot mechanism. 
    // For this exercise, we will assume Size() and GetOrderInfos() are for debugging 
    // and might be slightly inconsistent, or we can add a "Request" to get them (Async).
    // To keep signature, we'll leave them as is but warn they are not thread-safe vs the writer.
    std::size_t Size() const;
    OrderbookLevelInfos GetOrderInfos() const;
    
    // Helper to get from pool
    OrderPointer AcquireOrder(OrderType type, OrderId orderId, Side side, Price price, Quantity quantity);

    std::size_t GetOrdersProcessed() const { return ordersProcessed_.load(std::memory_order_relaxed); }
    
    // Warmup
    void Warmup();
    
    struct LatencyStats
    {
        uint64_t p50;
        uint64_t p99;
        uint64_t p999;
        uint64_t max;
    };
    
    LatencyStats GetLatencyStats(); // Not thread safe, call after shutdown or carefully

private:
    std::atomic<std::size_t> ordersProcessed_{ 0 };
};
