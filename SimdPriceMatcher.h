#pragma once

#include "Usings.h"
#include <vector>
#include <algorithm>
#include <optional>

// SIMD Price Matcher
// Stores prices in a contiguous vector (SoA layout) for vectorized comparison.
// Note: Actual AVX2 intrinsics (<immintrin.h>) are architecture specific (x86_64).
// On Apple Silicon (ARM64), these headers fail compilation.
// For portability in this project, we implement the "Auto-Vectorization Friendly" loop pattern
// which modern compilers (Clang/GCC) optimize to NEON (ARM) or AVX (x86) automatically at -O3.

class SimdPriceMatcher
{
public:
    void AddPrice(Price price)
    {
        // Optimization: Check if exists to avoid duplicates or expensive sort if already sorted
        // But for this demo, we assume AddPrice is called only when a NEW level is created.
        
        // Find insertion point O(log N) + Insert O(N)
        // Vector insertion is slow, but price levels don't change as often as orders match.
        // Also, we can optimize by just appending and sorting if batching, but here it's incremental.
        auto it = std::lower_bound(prices_.begin(), prices_.end(), price, std::greater<Price>());
        if (it == prices_.end() || *it != price) {
            prices_.insert(it, price);
        }
    }

    void RemovePrice(Price price)
    {
        auto it = std::lower_bound(prices_.begin(), prices_.end(), price, std::greater<Price>());
        if (it != prices_.end() && *it == price)
        {
            prices_.erase(it);
        }
    }

    // Finds the first price >= limit (for Bids) or <= limit (for Asks)
    std::optional<Price> FindBestMatch(Price limitPrice, bool isBid) const
    {
        if (prices_.empty()) return std::nullopt;

        size_t n = prices_.size();
        size_t i = 0;
        
        // Unrolled loop to encourage auto-vectorization
        for (; i + 8 <= n; i += 8)
        {
            for (int k = 0; k < 8; ++k)
            {
                if (isBid) {
                    if (prices_[i+k] >= limitPrice) return prices_[i+k];
                } else {
                    if (prices_[i+k] <= limitPrice) return prices_[i+k];
                }
            }
        }
        
        // Tail
        for (; i < n; ++i)
        {
            if (isBid) {
                if (prices_[i] >= limitPrice) return prices_[i];
            } else {
                if (prices_[i] <= limitPrice) return prices_[i];
            }
        }

        return std::nullopt;
    }

private:
    // Aligned vector for SIMD
    std::vector<Price> prices_;
};
