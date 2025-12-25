#pragma once

#include <list>
#include <exception>
#include <format>

#include "OrderType.h"
#include "Side.h"
#include "Usings.h"
#include "Constants.h"


class alignas(64) Order
{
public:
    Order() = default;

    Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity)
        : orderType_{ orderType }
        , orderId_{ orderId }
        , side_{ side }
        , price_{ price }
        , initialQuantity_{ quantity }
        , remainingQuantity_{ quantity }
    { }

    Order(OrderId orderId, Side side, Quantity quantity)
        : Order(OrderType::Market, orderId, side, Constants::InvalidPrice, quantity)
    { }

    OrderId GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    OrderType GetOrderType() const { return orderType_; }
    Quantity GetInitialQuantity() const { return initialQuantity_; }
    Quantity GetRemainingQuantity() const { return remainingQuantity_; }
    Quantity GetFilledQuantity() const { return GetInitialQuantity() - GetRemainingQuantity(); }
    bool IsFilled() const { return GetRemainingQuantity() == 0; }
    void Fill(Quantity quantity)
    {
        if (quantity > GetRemainingQuantity())
            throw std::logic_error(std::format("Order ({}) cannot be filled for more than its remaining quantity.", GetOrderId()));

        remainingQuantity_ -= quantity;
    }
    void ToGoodTillCancel(Price price) 
    { 
        if (GetOrderType() != OrderType::Market)
            throw std::logic_error(std::format("Order ({}) cannot be filled for more than its remaining quantity.", GetOrderId()));

        price_ = price;
        orderType_ = OrderType::GoodTillCancel;
    }

    void Reset(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity)
    {
        orderType_ = orderType;
        orderId_ = orderId;
        side_ = side;
        price_ = price;
        initialQuantity_ = quantity;
        remainingQuantity_ = quantity;
    }

private:
    OrderType orderType_;
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;
};

#include <memory_resource>

// Forward declare for OrderPointer
class Order;

// Custom deleter or just use raw pointers for PMR if we want to be pure?
// For compatibility with existing code, we will stick to shared_ptr but allocated via PMR?
// Actually, std::pmr::polymorphic_allocator is for containers. 
// For individual objects, we can just use raw pointers or unique_ptr with custom deleter.
// But to keep it simple for now, let's just make Order compatible with PMR vectors if needed.

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

