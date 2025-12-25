#pragma once

#include "Order.h"
#include <iostream>
#include <format>

class RiskManager
{
public:
    struct Config
    {
        Quantity maxOrderQuantity = 10000;
        Price maxPrice = 1000000;
        Price minPrice = 1;
    };

    RiskManager() : config_{} {}
    RiskManager(Config config) : config_(config) {}

    enum class Result
    {
        Allowed,
        RejectedMaxQty,
        RejectedPriceRange,
    };

    Result CheckOrder(const OrderPointer& order) const
    {
        if (order->GetInitialQuantity() > config_.maxOrderQuantity)
            return Result::RejectedMaxQty;

        // Market orders might have invalid price (or 0), skip price check for them if needed
        // But for this engine, let's assume even Market orders have some constraints or are converted
        if (order->GetOrderType() != OrderType::Market)
        {
            if (order->GetPrice() > config_.maxPrice || order->GetPrice() < config_.minPrice)
                return Result::RejectedPriceRange;
        }

        return Result::Allowed;
    }

private:
    Config config_;
};
