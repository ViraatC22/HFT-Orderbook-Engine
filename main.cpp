#include "Orderbook.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <format>

int main()
{
    std::cout << "===================================================" << std::endl;
    std::cout << "   HFT Orderbook Engine (Lock-Free / Zero-Alloc)   " << std::endl;
    std::cout << "===================================================" << std::endl;

    // Allocate Orderbook on Heap to avoid stack overflow
    // AsyncJournaler has 4MB ring buffer, which might exceed default stack limits.
    auto orderbookPtr = std::make_unique<Orderbook>();
    Orderbook& orderbook = *orderbookPtr;
    
    const int NUM_ORDERS = 1000000;

    std::cout << "[Info] Initializing Engine..." << std::endl;
    std::cout << "[Info] Warming up Object Pool..." << std::endl;
    
    // Pre-warm? The pool constructor does it.

    std::cout << "[Test] Starting Load Generator (1 Producer -> 1 Consumer)..." << std::endl;
    std::cout << "[Test] Generating " << NUM_ORDERS << " orders..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    std::thread producer([&]() {
        for (int i = 0; i < NUM_ORDERS; ++i)
        {
            // Create matching flow: Buy at 100, Sell at 100
            // This triggers trades.
            Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
            Price price = 100; 
            
            // Use Object Pool!
            auto order = orderbook.AcquireOrder(OrderType::GoodTillCancel, i+1, side, price, 10);
            orderbook.AddOrder(order);
        }
    });

    producer.join();
    
    // Wait for consumer to finish
    while (orderbook.GetOrdersProcessed() < NUM_ORDERS)
    {
        std::this_thread::yield();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "---------------------------------------------------" << std::endl;
    std::cout << "Results:" << std::endl;
    std::cout << "  Count:      " << NUM_ORDERS << " orders" << std::endl;
    std::cout << "  Time:       " << duration.count() << " ms" << std::endl;
    std::cout << "  Throughput: " << (NUM_ORDERS * 1000.0 / duration.count()) << " ops/sec" << std::endl;
    std::cout << "---------------------------------------------------" << std::endl;
    
    auto stats = orderbook.GetLatencyStats();
    std::cout << "Latency Metrics (Internal Processing):" << std::endl;
    std::cout << "  p50 (Median):   " << stats.p50 << " ns" << std::endl;
    std::cout << "  p99:            " << stats.p99 << " ns" << std::endl;
    std::cout << "  p99.9 (Tail):   " << stats.p999 << " ns" << std::endl;
    std::cout << "  Max:            " << stats.max << " ns" << std::endl;
    std::cout << "---------------------------------------------------" << std::endl;

    return 0;
}
