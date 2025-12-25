#pragma once

#include <fstream>
#include <string>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "Order.h"
#include "OrderModify.h"
#include "LockFreeQueue.h" // Reuse the ring buffer

// Async Journaler (Zero-Jitter I/O)
// Writes to a ring buffer, background thread drains to disk.
class AsyncJournaler
{
public:
    // Raw bytes structure for logging to avoid complex object lifecycle in buffer
    struct LogEntry
    {
        // Max size enough for Add order (Type+Id+Side+Price+Qty ~ 32 bytes)
        // We make it simple: just copy the bytes we need.
        char data[64]; 
        size_t length;
    };

    AsyncJournaler(const std::string& filename) 
        : filename_(filename)
        , queue_(65536) // 64k entries buffer
        , running_(true)
        , writerThread_(&AsyncJournaler::WriterLoop, this)
    {
    }
    
    ~AsyncJournaler()
    {
        running_.store(false, std::memory_order_release);
        if (writerThread_.joinable()) writerThread_.join();
    }
    
    template<typename T>
    void Log(const T& req)
    {
        LogEntry entry;
        char* ptr = entry.data;
        
        // Serialize to stack buffer first
        *reinterpret_cast<int*>(ptr) = static_cast<int>(req.type); ptr += sizeof(int);
        *reinterpret_cast<OrderId*>(ptr) = req.orderId; ptr += sizeof(OrderId);
        
        if (static_cast<int>(req.type) == 0 && req.order) // Add
        {
            *reinterpret_cast<OrderType*>(ptr) = req.order->GetOrderType(); ptr += sizeof(OrderType);
            *reinterpret_cast<OrderId*>(ptr) = req.order->GetOrderId(); ptr += sizeof(OrderId);
            *reinterpret_cast<Side*>(ptr) = req.order->GetSide(); ptr += sizeof(Side);
            *reinterpret_cast<Price*>(ptr) = req.order->GetPrice(); ptr += sizeof(Price);
            *reinterpret_cast<Quantity*>(ptr) = req.order->GetInitialQuantity(); ptr += sizeof(Quantity);
        }
        else if (static_cast<int>(req.type) == 2) // Modify
        {
             *reinterpret_cast<OrderModify*>(ptr) = req.modify; ptr += sizeof(OrderModify);
        }
        
        entry.length = ptr - entry.data;
        
        // Push to Ring Buffer (Non-blocking)
        // If full, we drop log (or busy wait, here we busy wait slightly or drop)
        // For zero jitter, we should have large enough buffer.
        while (!queue_.Push(entry)) {
            // Drop or Yield? In strict HFT, dropping log is better than stalling engine.
            // But for correctness audit, we might stall.
            // Let's yield once.
            std::this_thread::yield();
        }
    }
    
private:
    void WriterLoop()
    {
        std::ofstream file(filename_, std::ios::binary | std::ios::out | std::ios::trunc);
        
        while (running_.load(std::memory_order_acquire) || !queue_.IsEmpty())
        {
            LogEntry entry;
            if (queue_.Pop(entry))
            {
                file.write(entry.data, entry.length);
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    std::string filename_;
    LockFreeQueue<LogEntry> queue_;
    std::atomic<bool> running_;
    std::thread writerThread_;
};
