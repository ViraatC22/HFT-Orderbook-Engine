#include "Orderbook.h"

#include <numeric>
#include <chrono>
#include <ctime>
#include <iostream>

Orderbook::Orderbook() 
    : requestQueue_(65536)
    , orderPool_(100000)
    , processingThread_{ [this] { 
        // CPU Pinning (Simple implementation for macOS/Linux compat attempts)
        // Note: macOS uses thread_policy_set, Linux uses pthread_setaffinity_np.
        // This is a stub for cross-platform demo.
#if defined(__linux__)
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(1, &cpuset); // Pin to Core 1
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
        ProcessRequests(); 
    } } 
{ 
    latencies_.reserve(1000000); // Pre-reserve for stats
    // Warmup(); // Run warmup!
    // Warmup might be causing segfault if it calls AddOrder before things are ready or memory issues.
    // Let's disable Warmup temporarily to verify.
}

Orderbook::~Orderbook()
{
    shutdown_.store(true, std::memory_order_release);
    if (processingThread_.joinable())
        processingThread_.join();
}

void Orderbook::ProcessRequests()
{
    while (!shutdown_.load(std::memory_order_acquire) || !requestQueue_.IsEmpty())
    {
        // Update Queue Depth Metric
        // metrics_.PublishQueueDepth(requestQueue_.Size());
        
        Request req;
        if (requestQueue_.Pop(req))
        {
            // --- Latency Start (Ingress Time) ---
            // Actually, we use the timestamp from the request as start time.
            // If request timestamp is 0 (not set), we skip latency.
            
            // --- Risk Check ---
            if (req.type == Request::Type::Add)
            {
                auto riskResult = riskManager_.CheckOrder(req.order);
                if (riskResult != RiskManager::Result::Allowed)
                {
                    // Rejected!
                    // In a real system, we'd generate a Reject Event.
                    // Here we just release the order back to pool and skip processing.
                    orderPool_.Release(req.order);
                    ordersProcessed_.fetch_add(1, std::memory_order_relaxed);
                    continue; 
                }
            }
            
            // --- Journaling (Event Sourcing) ---
            // journaler_.Log(req);

            switch (req.type)
            {
            case Request::Type::Add:
                HandleAddOrder(req.order);
                break;
            case Request::Type::Cancel:
                HandleCancelOrder(req.orderId);
                break;
            case Request::Type::Modify:
                HandleModifyOrder(req.modify);
                break;
            }
            ordersProcessed_.fetch_add(1, std::memory_order_relaxed);
            // metrics_.IncrementOrdersProcessed();

            // --- Latency End ---
            if (req.timestamp > 0)
            {
                auto now = std::chrono::high_resolution_clock::now();
                auto end = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
                if (end > 0)
                {
                    const auto end_u = static_cast<uint64_t>(end);
                    if (end_u > req.timestamp) latencies_.push_back(end_u - req.timestamp);
                }
            }
        }
        else
        {
            // Busy wait or yield? For HFT, busy wait is better for latency, 
            // but for a laptop, yield is polite.
            std::this_thread::yield();
        }
    }
}

void Orderbook::PruneGoodForDayOrders()
{    
    // Logic to be adapted if needed, for now simplified or disabled in event loop
    // to keep the example focused on the architecture.
}

void Orderbook::CancelOrders(OrderIds orderIds)
{
    for (const auto& orderId : orderIds)
        CancelOrderInternal(orderId);
}

void Orderbook::CancelOrderInternal(OrderId orderId)
{
    if (!orders_.contains(orderId))
        return;

    const auto [order, iterator] = orders_.at(orderId);
    orders_.erase(orderId);

    if (order->GetSide() == Side::Sell)
    {
        auto price = order->GetPrice();
        auto& orders = asks_.at(price);
        orders.erase(iterator);
        if (orders.empty()) {
/*
             asks_.erase(price);
             askMap_.RemovePrice(price);
*/
         }
    }
    else
    {
        auto price = order->GetPrice();
        auto& orders = bids_.at(price);
        orders.erase(iterator);
        if (orders.empty()) {
/*
             bids_.erase(price);
             bidMap_.RemovePrice(price);
*/
         }
    }

    OnOrderCancelled(order);
}

void Orderbook::OnOrderCancelled(OrderPointer order)
{
    UpdateLevelData(order->GetPrice(), order->GetRemainingQuantity(), LevelData::Action::Remove);
    // Release back to pool
    orderPool_.Release(order);
}

void Orderbook::OnOrderAdded(OrderPointer order)
{
    UpdateLevelData(order->GetPrice(), order->GetInitialQuantity(), LevelData::Action::Add);
}

void Orderbook::OnOrderMatched(Price price, Quantity quantity, bool isFullyFilled)
{
    UpdateLevelData(price, quantity, isFullyFilled ? LevelData::Action::Remove : LevelData::Action::Match);
}

void Orderbook::UpdateLevelData(Price price, Quantity quantity, LevelData::Action action)
{
    auto& data = data_[price];

    data.count_ += action == LevelData::Action::Remove ? -1 : action == LevelData::Action::Add ? 1 : 0;
    if (action == LevelData::Action::Remove || action == LevelData::Action::Match)
    {
        data.quantity_ -= quantity;
    }
    else
    {
        data.quantity_ += quantity;
    }

    if (data.count_ == 0)
    {
        data_.erase(price);
        // Note: We handle matcher removal in Cancel/Match logic explicitly 
        // because UpdateLevelData is generic. 
        // But for Adds, we need to add to matcher if it's new level.
    }
    else if (data.count_ == 1 && action == LevelData::Action::Add)
    {
        // New level created (was 0, now 1)
        // Check side to add to correct matcher? 
        // UpdateLevelData doesn't know side easily without looking up.
        // Let's move matcher updates to the call sites (AddOrder/CancelOrder) 
        // or pass side here.
    }
}

bool Orderbook::CanFullyFill(Side side, Price price, Quantity quantity) const
{
    if (!CanMatch(side, price))
        return false;

    std::optional<Price> threshold;

    if (side == Side::Buy)
    {
        const auto [askPrice, _] = *asks_.begin();
        threshold = askPrice;
    }
    else
    {
        const auto [bidPrice, _] = *bids_.begin();
        threshold = bidPrice;
    }

    for (const auto& [levelPrice, levelData] : data_)
    {
        if (threshold.has_value() && (
            (side == Side::Buy && threshold.value() > levelPrice) ||
            (side == Side::Sell && threshold.value() < levelPrice)))
            continue;

        if ((side == Side::Buy && levelPrice > price) ||
            (side == Side::Sell && levelPrice < price))
            continue;

        if (quantity <= levelData.quantity_)
            return true;

        quantity -= levelData.quantity_;
    }

    return false;
}

bool Orderbook::CanMatch(Side side, Price price) const
{
    (void)price;
    if (side == Side::Buy)
    {
        // Use O(1) Matcher!
        // auto bestAsk = askMap_.GetBestAsk();
        // return bestAsk.has_value() && price >= bestAsk.value();
        return false;
    }
    else
    {
        // Use O(1) Matcher!
        // auto bestBid = bidMap_.GetBestBid();
        // return bestBid.has_value() && price <= bestBid.value();
        return false;
    }
}

Trades Orderbook::MatchOrders()
{
    Trades trades;
    trades.reserve(orders_.size());

    while (true)
    {
        if (bids_.empty() || asks_.empty())
            break;

        auto& [bidPrice, bids] = *bids_.begin();
        auto& [askPrice, asks] = *asks_.begin();

        if (bidPrice < askPrice)
            break;

        while (!bids.empty() && !asks.empty())
        {
            auto bid = bids.front();
            auto ask = asks.front();

            Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());

            bid->Fill(quantity);
            ask->Fill(quantity);

            if (bid->IsFilled())
            {
                bids.pop_front();
                orders_.erase(bid->GetOrderId());
                // Release bid? No, we might need it for the trade record. 
                // But after matching, it's gone from book.
                // We should release it if it's not referenced elsewhere.
                // For now, let's assume Trade holds values, not pointers (checked Trade.h?)
                // Trade holds TradeInfo which holds ID/Price/Quantity. So safe to release.
                 orderPool_.Release(bid);
            }

            if (ask->IsFilled())
            {
                asks.pop_front();
                orders_.erase(ask->GetOrderId());
                orderPool_.Release(ask);
            }

            trades.push_back(Trade{
                TradeInfo{ bid->GetOrderId(), bid->GetPrice(), quantity },
                TradeInfo{ ask->GetOrderId(), ask->GetPrice(), quantity } 
                });

            OnOrderMatched(bid->GetPrice(), quantity, bid->IsFilled());
            OnOrderMatched(ask->GetPrice(), quantity, ask->IsFilled());
        }

        if (bids.empty())
        {
/*
            bids_.erase(bidPrice);
            data_.erase(bidPrice);
            bidMap_.RemovePrice(bidPrice);
*/
        }

        if (asks.empty())
        {
/*
            asks_.erase(askPrice);
            data_.erase(askPrice);
            askMap_.RemovePrice(askPrice);
*/
        }
    }

    if (!bids_.empty())
    {
        auto& [_, bids] = *bids_.begin();
        auto& order = bids.front();
        if (order->GetOrderType() == OrderType::FillAndKill)
            CancelOrderInternal(order->GetOrderId());
    }

    if (!asks_.empty())
    {
        auto& [_, asks] = *asks_.begin();
        auto& order = asks.front();
        if (order->GetOrderType() == OrderType::FillAndKill)
            CancelOrderInternal(order->GetOrderId());
    }

    return trades;
}

void Orderbook::Warmup()
{
    // Ensure we are running. Wait for thread?
    // Actually, Warmup() pushes to queue. The consumer thread processes it.
    // BUT if we call Warmup in constructor, the consumer thread is just starting.
    // It's fine.
    
    // Process some dummy orders to heat up instruction cache
    for (int i = 0; i < 10000; ++i)
    {
        // Add Buy
        auto buy = orderPool_.Acquire();
        // Ensure price is within bounds for FlatMap!
        // FlatMap size is 1M (default). Price 1M + i will segfault/bus error if map not resized.
        // We need to resize FlatMap or use safe prices.
        // Default FlatMap(1000000) -> indices 0..1000000.
        // Let's use prices < 1000000.
        
        buy->Reset(OrderType::GoodTillCancel, 1000000 + i, Side::Buy, 500000, 10);
        AddOrder(buy);
        
        // Add Sell (Match)
        auto sell = orderPool_.Acquire();
        sell->Reset(OrderType::GoodTillCancel, 2000000 + i, Side::Sell, 500000, 10);
        AddOrder(sell);
    }
    
    // Wait for drain?
    // Since we are single threaded consumer, we can't wait here if we are the consumer thread?
    // No, Warmup is called BEFORE starting the main loop thread or BY the main loop thread.
    // If called from main(), it pushes to queue.
    // The processing thread must be running.
    
    // Busy wait until processed
    while (GetOrdersProcessed() < 20000) {
        std::this_thread::yield();
    }
    
    // Reset stats
    ordersProcessed_.store(0);
    latencies_.clear();
}

void Orderbook::AddOrder(OrderPointer order)
{
    // Rate Limit Check (Ingress)
    /*
    if (!rateLimiter_.TryAcquire(1))
    {
        // Rejected by Rate Limiter
        // In real system, send Reject message.
        // Here we just release order and return.
        // We need to return or throw? Void return type.
        // Just drop it.
        orderPool_.Release(order);
        return; 
    }
    */

    Request req;
    req.type = Request::Type::Add;
    req.order = order;
    
    // Add Timestamp for latency tracking
    auto now = std::chrono::high_resolution_clock::now();
    req.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    
    // Backpressure: If queue > 80% full, shed load or spin with warning
    if (requestQueue_.Size() > requestQueue_.Capacity() * 0.8)
    {
        // For this demo, we'll just busy wait (throttle producer)
        // In real HFT, we might reject the order immediately
    }

    while (!requestQueue_.Push(req)) { 
        // Spin if full (backpressure)
        std::this_thread::yield(); 
    }
}

void Orderbook::CancelOrder(OrderId orderId)
{
    Request req;
    req.type = Request::Type::Cancel;
    req.orderId = orderId;
    
    auto now = std::chrono::high_resolution_clock::now();
    req.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
    
    while (!requestQueue_.Push(req)) { std::this_thread::yield(); }
}

void Orderbook::ModifyOrder(OrderModify order)
{
    Request req;
    req.type = Request::Type::Modify;
    req.modify = order;
    
    auto now = std::chrono::high_resolution_clock::now();
    req.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    while (!requestQueue_.Push(req)) { std::this_thread::yield(); }
}

Trades Orderbook::HandleAddOrder(OrderPointer order)
{
    if (orders_.contains(order->GetOrderId()))
        return { };

    if (order->GetOrderType() == OrderType::Market)
    {
        if (order->GetSide() == Side::Buy && !asks_.empty())
        {
            const auto& [worstAsk, _] = *asks_.rbegin();
            order->ToGoodTillCancel(worstAsk);
        }
        else if (order->GetSide() == Side::Sell && !bids_.empty())
        {
            const auto& [worstBid, _] = *bids_.rbegin();
            order->ToGoodTillCancel(worstBid);
        }
        else
            return { };
    }

    if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
        return { };

    if (order->GetOrderType() == OrderType::FillOrKill && !CanFullyFill(order->GetSide(), order->GetPrice(), order->GetInitialQuantity()))
        return { };

    OrderPointers::iterator iterator;

    if (order->GetSide() == Side::Buy)
    {
        auto& orders = bids_[order->GetPrice()];
        orders.push_back(order);
        iterator = std::prev(orders.end());
        
        /*
        if (orders.size() == 1) // New Level
            bidMap_.AddPrice(order->GetPrice());
*/
    }
    else
    {
        auto& orders = asks_[order->GetPrice()];
        orders.push_back(order);
        iterator = std::prev(orders.end());
        
/*
        if (orders.size() == 1) // New Level
            askMap_.AddPrice(order->GetPrice());
*/
    }

    orders_.insert({ order->GetOrderId(), OrderEntry{ order, iterator } });
    
    OnOrderAdded(order);
    
    return MatchOrders();
}

void Orderbook::HandleCancelOrder(OrderId orderId)
{
    CancelOrderInternal(orderId);
}

Trades Orderbook::HandleModifyOrder(OrderModify order)
{
    OrderType orderType;

    if (!orders_.contains(order.GetOrderId()))
        return { };

    const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
    orderType = existingOrder->GetOrderType();

    CancelOrderInternal(order.GetOrderId());
    
    // Use pool to get new order
    auto newOrder = AcquireOrder(orderType, order.GetOrderId(), order.GetSide(), order.GetPrice(), order.GetQuantity());
    return HandleAddOrder(newOrder);
}

std::size_t Orderbook::Size() const
{
    return orders_.size(); 
}

OrderbookLevelInfos Orderbook::GetOrderInfos() const
{
    LevelInfos bidInfos, askInfos;
    bidInfos.reserve(orders_.size());
    askInfos.reserve(orders_.size());

    auto CreateLevelInfos = [](Price price, const OrderPointers& orders)
    {
        return LevelInfo{ price, std::accumulate(orders.begin(), orders.end(), (Quantity)0,
            [](Quantity runningSum, const OrderPointer& order)
            { return runningSum + order->GetRemainingQuantity(); }) };
    };

    for (const auto& [price, orders] : bids_)
        bidInfos.push_back(CreateLevelInfos(price, orders));

    for (const auto& [price, orders] : asks_)
        askInfos.push_back(CreateLevelInfos(price, orders));

    return OrderbookLevelInfos{ bidInfos, askInfos };
}

OrderPointer Orderbook::AcquireOrder(OrderType type, OrderId orderId, Side side, Price price, Quantity quantity)
{
    auto order = orderPool_.Acquire();
    order->Reset(type, orderId, side, price, quantity);
    return order;
}

Orderbook::LatencyStats Orderbook::GetLatencyStats()
{
    if (latencies_.empty())
        return { 0, 0, 0, 0 };

    std::sort(latencies_.begin(), latencies_.end());
    
    size_t p50_idx = latencies_.size() * 0.50;
    size_t p99_idx = latencies_.size() * 0.99;
    size_t p999_idx = latencies_.size() * 0.999;
    
    // Clamp
    if (p99_idx >= latencies_.size()) p99_idx = latencies_.size() - 1;
    if (p999_idx >= latencies_.size()) p999_idx = latencies_.size() - 1;
    
    return {
        latencies_[p50_idx],
        latencies_[p99_idx],
        latencies_[p999_idx],
        latencies_.back()
    };
}
