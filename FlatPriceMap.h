#pragma once

#include "Usings.h"
#include <vector>
#include <limits>
#include <optional>
#include <iostream>

// O(1) Price Lookup using a Flat Array
// Assumes prices are integers and fall within a reasonable range (e.g., 0 to 1,000,000).
// For sparse markets or huge price ranges, a hierarchical bitset (or 2-level array) is better.
class FlatPriceMap
{
public:
    FlatPriceMap(size_t maxPrice = 1000000) 
        : maxPrice_(std::numeric_limits<Price>::min())
        , minPrice_(std::numeric_limits<Price>::max())
    {
        // Vector<bool> is specialized and not thread safe, but we are single threaded.
        // 1M entries.
        try {
            exists_.resize(maxPrice + 1, false);
        } catch (const std::exception& e) {
            std::cerr << "FlatPriceMap alloc failed: " << e.what() << std::endl;
            throw;
        }
    }

    void AddPrice(Price price)
    {
        if (price > maxPrice_) maxPrice_ = price; // Fast update for Bids
        if (price < minPrice_) minPrice_ = price; // Fast update for Asks
        
        if (price < exists_.size())
            exists_[price] = true;
    }

    void RemovePrice(Price price)
    {
        if (price < exists_.size())
            exists_[price] = false;
            
        // Lazy update of min/max? 
        // If we removed the max, we need to scan down to find new max.
        // This is the trade-off. O(1) Add, but Remove might be O(K) where K is gap.
        
        if (price == maxPrice_)
        {
            // Scan down
            while (maxPrice_ > 0 && !exists_[maxPrice_]) {
                maxPrice_--;
            }
        }
        
        if (price == minPrice_)
        {
            // Scan up
            while (minPrice_ < exists_.size() && !exists_[minPrice_]) {
                minPrice_++;
            }
        }
    }

    // O(1) Best Price Lookup
    std::optional<Price> GetBestBid() const
    {
        if (maxPrice_ == std::numeric_limits<Price>::min() || !exists_[maxPrice_]) 
            return std::nullopt;
        return maxPrice_;
    }

    std::optional<Price> GetBestAsk() const
    {
        if (minPrice_ == std::numeric_limits<Price>::max() || !exists_[minPrice_]) 
            return std::nullopt;
        return minPrice_;
    }

private:
    std::vector<bool> exists_;
    Price minPrice_;
    Price maxPrice_;
};
