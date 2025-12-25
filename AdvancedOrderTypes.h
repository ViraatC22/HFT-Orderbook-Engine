#pragma once

#include <atomic>
#include <variant>
#include <optional>
#include <chrono>
#include <vector>
#include <memory>

#include "Usings.h"
#include "Side.h"
#include "OrderType.h"

/**
 * Advanced Order Types for Professional HFT Systems
 * 
 * This header defines sophisticated order types used in institutional trading:
 * - Iceberg Orders: Large orders split into visible portions
 * - Hidden Orders: Completely invisible liquidity
 * - Stop Orders: Triggered at specified price levels
 * - Stop-Limit Orders: Stop orders with limit price protection
 * - Trailing Stop Orders: Dynamic stop levels based on market movement
 * - One-Cancels-Other (OCO): Linked order pairs
 * - Good-Till-Date (GTD): Time-based order expiration
 * - Market-on-Close (MOC): Execute at market close
 * - Immediate-or-Cancel (IOC): Enhanced with partial fill logic
 */

enum class AdvancedOrderType : uint8_t
{
    Iceberg,           // Large order with visible portion
    Hidden,            // Completely invisible order
    Stop,              // Market order triggered at stop price
    StopLimit,         // Limit order triggered at stop price
    TrailingStop,      // Stop order with dynamic trigger price
    OCO,               // One-Cancels-Other order pair
    GTD,               // Good-Till-Date order
    MOC,               // Market-on-Close order
    MOO,               // Market-on-Open order
    Pegged,            // Pegged to market price
    Discretionary,     // Hidden price improvement
    Cross,             // Cross order for dark pools
    Swap,              // Swap order for derivatives
    Algo,              // Algorithmic order with custom logic
    Auction,           // Auction-only order
    Retail,            // Retail investor order
    Institutional      // Institutional investor order
};

enum class StopTriggerType : uint8_t
{
    Last,              // Trigger on last traded price
    Bid,               // Trigger on best bid price
    Ask,               // Trigger on best ask price
    Mid,               // Trigger on mid-market price
    VWAP,              // Trigger on VWAP
    TWAP               // Trigger on TWAP
};

enum class TrailingType : uint8_t
{
    Fixed,             // Fixed distance from reference
    Percentage,        // Percentage distance from reference
    Dynamic            // Dynamic based on volatility
};

enum class PegType : uint8_t
{
    Mid,               // Peg to mid-market price
    Primary,           // Peg to primary market
    Market,            // Peg to market price
    Limit,             // Peg to limit price
    Discretionary      // Peg to discretionary price
};

struct IcebergOrderData
{
    Quantity total_quantity;      // Total order size
    Quantity visible_quantity;    // Currently displayed quantity
    Quantity minimum_quantity;    // Minimum refresh quantity
    uint8_t refresh_type;        // 0=immediate, 1=partial, 2=full
    uint32_t refresh_delay_ms;    // Delay before refresh
    Quantity displayed_so_far;    // Track what was shown
    uint32_t refresh_count;       // Number of refreshes
};

struct HiddenOrderData
{
    Quantity minimum_quantity;    // Minimum execution size
    Price discretionary_offset;   // Hidden price improvement
    bool allow_display;           // Can become visible if needed
    uint8_t priority;            // Execution priority vs visible orders
};

struct StopOrderData
{
    Price stop_price;             // Trigger price level
    StopTriggerType trigger_type; // What price triggers the stop
    Price trigger_price;          // Actual trigger price (for trailing)
    std::chrono::system_clock::time_point trigger_time; // When triggered
    bool triggered;                // Has the stop been triggered
    uint8_t trigger_count;         // Number of trigger attempts
};

struct StopLimitOrderData
{
    StopOrderData stop_data;      // Stop order component
    Price limit_price;            // Limit price after trigger
    Quantity minimum_quantity;    // Minimum execution size after trigger
};

struct TrailingStopOrderData
{
    StopOrderData stop_data;      // Base stop order data
    TrailingType trailing_type;   // How to calculate trailing distance
    double trailing_distance;     // Distance from reference (fixed or %)
    Price reference_price;        // Reference price for trailing
    Price highest_high;          // Highest price since order (for sell stops)
    Price lowest_low;            // Lowest price since order (for buy stops)
    bool use_peak_reference;       // Use peak/trough as reference
};

struct OCOOrderData
{
    OrderId primary_order_id;     // Primary order in the pair
    OrderId secondary_order_id;   // Secondary order in the pair
    bool primary_filled;          // Primary order filled
    bool secondary_filled;        // Secondary order filled
    uint8_t cancel_reason;        // Why the other was cancelled
};

struct GTDOrderData
{
    std::chrono::system_clock::time_point expiry_time; // When order expires
    std::chrono::system_clock::time_point created_time; // When order was created
    std::chrono::seconds time_in_force;                // Original TIF
    bool expired;                                        // Has the order expired
    uint8_t expiry_type;                                // 0=session, 1=day, 2=GTC
};

struct PeggedOrderData
{
    PegType peg_type;             // How to peg the price
    Price offset;                 // Offset from peg price
    double deviation_limit;       // Maximum deviation from peg
    Price current_peg_price;    // Current pegged price
    std::chrono::milliseconds refresh_interval; // How often to refresh
    uint32_t refresh_count;       // Number of refreshes
};

struct DiscretionaryOrderData
{
    Price discretionary_price;    // Hidden better price
    Quantity discretionary_quantity; // Quantity at better price
    bool auto_exercise;          // Automatically exercise discretion
    uint8_t exercise_conditions;  // When to exercise discretion
};

// Main advanced order structure with variant for different types
struct AdvancedOrder
{
    OrderId order_id;
    Side side;
    Quantity quantity;
    Price price;
    AdvancedOrderType type;
    
    // Common advanced order fields
    Quantity minimum_quantity;    // Minimum execution size
    Price maximum_price;          // Maximum execution price
    std::chrono::system_clock::time_point created_time;
    std::chrono::system_clock::time_point modified_time;
    uint64_t sequence_number;     // For ordering
    uint8_t priority;             // Execution priority
    
    // Type-specific data
    std::variant<
        std::monostate,           // No additional data
        IcebergOrderData,         // Iceberg order data
        HiddenOrderData,          // Hidden order data
        StopOrderData,            // Stop order data
        StopLimitOrderData,       // Stop-limit order data
        TrailingStopOrderData,    // Trailing stop order data
        OCOOrderData,             // OCO order data
        GTDOrderData,             // Good-till-date order data
        PeggedOrderData,          // Pegged order data
        DiscretionaryOrderData    // Discretionary order data
    > advanced_data;
    
    // State tracking
    std::atomic<bool> active;      // Order is active
    std::atomic<bool> filled;      // Order is completely filled
    std::atomic<Quantity> filled_quantity; // Quantity filled so far
    std::atomic<uint8_t> status;   // Order status
    
    AdvancedOrder() : active(false), filled(false), filled_quantity(0), status(0) {}
    
    AdvancedOrder(const AdvancedOrder& other)
        : order_id(other.order_id),
          side(other.side),
          quantity(other.quantity),
          price(other.price),
          type(other.type),
          minimum_quantity(other.minimum_quantity),
          maximum_price(other.maximum_price),
          created_time(other.created_time),
          modified_time(other.modified_time),
          sequence_number(other.sequence_number),
          priority(other.priority),
          advanced_data(other.advanced_data),
          active(other.active.load(std::memory_order_relaxed)),
          filled(other.filled.load(std::memory_order_relaxed)),
          filled_quantity(other.filled_quantity.load(std::memory_order_relaxed)),
          status(other.status.load(std::memory_order_relaxed))
    {
    }
    
    AdvancedOrder& operator=(const AdvancedOrder& other)
    {
        if (this == &other) return *this;
        order_id = other.order_id;
        side = other.side;
        quantity = other.quantity;
        price = other.price;
        type = other.type;
        minimum_quantity = other.minimum_quantity;
        maximum_price = other.maximum_price;
        created_time = other.created_time;
        modified_time = other.modified_time;
        sequence_number = other.sequence_number;
        priority = other.priority;
        advanced_data = other.advanced_data;
        active.store(other.active.load(std::memory_order_relaxed), std::memory_order_relaxed);
        filled.store(other.filled.load(std::memory_order_relaxed), std::memory_order_relaxed);
        filled_quantity.store(other.filled_quantity.load(std::memory_order_relaxed), std::memory_order_relaxed);
        status.store(other.status.load(std::memory_order_relaxed), std::memory_order_relaxed);
        return *this;
    }
    
    AdvancedOrder(AdvancedOrder&& other) noexcept
        : order_id(other.order_id),
          side(other.side),
          quantity(other.quantity),
          price(other.price),
          type(other.type),
          minimum_quantity(other.minimum_quantity),
          maximum_price(other.maximum_price),
          created_time(other.created_time),
          modified_time(other.modified_time),
          sequence_number(other.sequence_number),
          priority(other.priority),
          advanced_data(std::move(other.advanced_data)),
          active(other.active.load(std::memory_order_relaxed)),
          filled(other.filled.load(std::memory_order_relaxed)),
          filled_quantity(other.filled_quantity.load(std::memory_order_relaxed)),
          status(other.status.load(std::memory_order_relaxed))
    {
    }
    
    AdvancedOrder& operator=(AdvancedOrder&& other) noexcept
    {
        if (this == &other) return *this;
        order_id = other.order_id;
        side = other.side;
        quantity = other.quantity;
        price = other.price;
        type = other.type;
        minimum_quantity = other.minimum_quantity;
        maximum_price = other.maximum_price;
        created_time = other.created_time;
        modified_time = other.modified_time;
        sequence_number = other.sequence_number;
        priority = other.priority;
        advanced_data = std::move(other.advanced_data);
        active.store(other.active.load(std::memory_order_relaxed), std::memory_order_relaxed);
        filled.store(other.filled.load(std::memory_order_relaxed), std::memory_order_relaxed);
        filled_quantity.store(other.filled_quantity.load(std::memory_order_relaxed), std::memory_order_relaxed);
        status.store(other.status.load(std::memory_order_relaxed), std::memory_order_relaxed);
        return *this;
    }
};

// Advanced order type helpers
namespace AdvancedOrderUtils
{
    // Create iceberg order
    [[nodiscard]] AdvancedOrder CreateIcebergOrder(
        OrderId id, Side side, Quantity total_qty, Quantity visible_qty,
        Price price, Quantity min_refresh = 1, uint32_t refresh_delay_ms = 0)
    {
        AdvancedOrder order;
        order.order_id = id;
        order.side = side;
        order.quantity = visible_qty; // Display visible quantity
        order.price = price;
        order.type = AdvancedOrderType::Iceberg;
        order.minimum_quantity = min_refresh;
        order.created_time = std::chrono::system_clock::now();
        order.sequence_number = id;
        order.priority = 5; // Medium priority
        
        IcebergOrderData iceberg;
        iceberg.total_quantity = total_qty;
        iceberg.visible_quantity = visible_qty;
        iceberg.minimum_quantity = min_refresh;
        iceberg.refresh_type = 0; // Immediate refresh
        iceberg.refresh_delay_ms = refresh_delay_ms;
        iceberg.displayed_so_far = 0;
        iceberg.refresh_count = 0;
        
        order.advanced_data = iceberg;
        order.active = true;
        order.filled = false;
        order.filled_quantity = 0;
        order.status = 0;
        
        return order;
    }
    
    // Create hidden order
    [[nodiscard]] AdvancedOrder CreateHiddenOrder(
        OrderId id, Side side, Quantity quantity, Price price,
        Quantity min_qty = 1, Price discretionary_offset = 0)
    {
        AdvancedOrder order;
        order.order_id = id;
        order.side = side;
        order.quantity = quantity;
        order.price = price;
        order.type = AdvancedOrderType::Hidden;
        order.minimum_quantity = min_qty;
        order.created_time = std::chrono::system_clock::now();
        order.sequence_number = id;
        order.priority = 3; // Lower priority than visible orders
        
        HiddenOrderData hidden;
        hidden.minimum_quantity = min_qty;
        hidden.discretionary_offset = discretionary_offset;
        hidden.allow_display = false;
        hidden.priority = 3;
        
        order.advanced_data = hidden;
        order.active = true;
        order.filled = false;
        order.filled_quantity = 0;
        order.status = 0;
        
        return order;
    }
    
    // Create stop order
    [[nodiscard]] AdvancedOrder CreateStopOrder(
        OrderId id, Side side, Quantity quantity, Price stop_price,
        StopTriggerType trigger_type = StopTriggerType::Last)
    {
        AdvancedOrder order;
        order.order_id = id;
        order.side = side;
        order.quantity = quantity;
        order.price = 0; // Market order after trigger
        order.type = AdvancedOrderType::Stop;
        order.minimum_quantity = 1;
        order.created_time = std::chrono::system_clock::now();
        order.sequence_number = id;
        order.priority = 6; // High priority for stops
        
        StopOrderData stop;
        stop.stop_price = stop_price;
        stop.trigger_type = trigger_type;
        stop.trigger_price = stop_price;
        stop.triggered = false;
        stop.trigger_count = 0;
        
        order.advanced_data = stop;
        order.active = true;
        order.filled = false;
        order.filled_quantity = 0;
        order.status = 0;
        
        return order;
    }
    
    // Create stop-limit order
    [[nodiscard]] AdvancedOrder CreateStopLimitOrder(
        OrderId id, Side side, Quantity quantity, Price stop_price,
        Price limit_price, StopTriggerType trigger_type = StopTriggerType::Last)
    {
        AdvancedOrder order;
        order.order_id = id;
        order.side = side;
        order.quantity = quantity;
        order.price = limit_price; // Limit price after trigger
        order.type = AdvancedOrderType::StopLimit;
        order.minimum_quantity = 1;
        order.created_time = std::chrono::system_clock::now();
        order.sequence_number = id;
        order.priority = 6; // High priority for stops
        
        StopLimitOrderData stop_limit;
        stop_limit.stop_data.stop_price = stop_price;
        stop_limit.stop_data.trigger_type = trigger_type;
        stop_limit.stop_data.trigger_price = stop_price;
        stop_limit.stop_data.triggered = false;
        stop_limit.stop_data.trigger_count = 0;
        stop_limit.limit_price = limit_price;
        stop_limit.minimum_quantity = 1;
        
        order.advanced_data = stop_limit;
        order.active = true;
        order.filled = false;
        order.filled_quantity = 0;
        order.status = 0;
        
        return order;
    }
    
    // Create trailing stop order
    [[nodiscard]] AdvancedOrder CreateTrailingStopOrder(
        OrderId id, Side side, Quantity quantity, double trailing_distance,
        TrailingType trailing_type = TrailingType::Fixed,
        StopTriggerType trigger_type = StopTriggerType::Last)
    {
        AdvancedOrder order;
        order.order_id = id;
        order.side = side;
        order.quantity = quantity;
        order.price = 0; // Market order after trigger
        order.type = AdvancedOrderType::TrailingStop;
        order.minimum_quantity = 1;
        order.created_time = std::chrono::system_clock::now();
        order.sequence_number = id;
        order.priority = 6; // High priority for stops
        
        TrailingStopOrderData trailing;
        trailing.stop_data.stop_price = 0; // Will be calculated dynamically
        trailing.stop_data.trigger_type = trigger_type;
        trailing.stop_data.trigger_price = 0;
        trailing.stop_data.triggered = false;
        trailing.stop_data.trigger_count = 0;
        trailing.trailing_type = trailing_type;
        trailing.trailing_distance = trailing_distance;
        trailing.reference_price = 0;
        trailing.highest_high = 0;
        trailing.lowest_low = 0;
        trailing.use_peak_reference = true;
        
        order.advanced_data = trailing;
        order.active = true;
        order.filled = false;
        order.filled_quantity = 0;
        order.status = 0;
        
        return order;
    }
    
    // Create OCO order pair
    [[nodiscard]] std::pair<AdvancedOrder, AdvancedOrder> CreateOCOOrders(
        OrderId primary_id, OrderId secondary_id,
        const AdvancedOrder& primary_template, const AdvancedOrder& secondary_template)
    {
        AdvancedOrder primary = primary_template;
        AdvancedOrder secondary = secondary_template;
        
        primary.order_id = primary_id;
        secondary.order_id = secondary_id;
        
        primary.type = AdvancedOrderType::OCO;
        secondary.type = AdvancedOrderType::OCO;
        
        OCOOrderData oco_primary;
        oco_primary.primary_order_id = primary_id;
        oco_primary.secondary_order_id = secondary_id;
        oco_primary.primary_filled = false;
        oco_primary.secondary_filled = false;
        oco_primary.cancel_reason = 0;
        
        OCOOrderData oco_secondary;
        oco_secondary.primary_order_id = secondary_id;
        oco_secondary.secondary_order_id = primary_id;
        oco_secondary.primary_filled = false;
        oco_secondary.secondary_filled = false;
        oco_secondary.cancel_reason = 0;
        
        primary.advanced_data = oco_primary;
        secondary.advanced_data = oco_secondary;
        
        return {primary, secondary};
    }
    
    // Create GTD order
    [[nodiscard]] AdvancedOrder CreateGTDOrder(
        OrderId id, Side side, Quantity quantity, Price price,
        std::chrono::system_clock::time_point expiry_time)
    {
        AdvancedOrder order;
        order.order_id = id;
        order.side = side;
        order.quantity = quantity;
        order.price = price;
        order.type = AdvancedOrderType::GTD;
        order.minimum_quantity = 1;
        order.created_time = std::chrono::system_clock::now();
        order.sequence_number = id;
        order.priority = 4; // Normal priority
        
        GTDOrderData gtd;
        gtd.expiry_time = expiry_time;
        gtd.created_time = order.created_time;
        gtd.time_in_force = std::chrono::hours(24); // 24 hours default
        gtd.expired = false;
        gtd.expiry_type = 1; // Day
        
        order.advanced_data = gtd;
        order.active = true;
        order.filled = false;
        order.filled_quantity = 0;
        order.status = 0;
        
        return order;
    }
    
    // Check if order should be triggered (for stop orders)
    [[nodiscard]] bool ShouldTrigger(const AdvancedOrder& order, Price current_price, 
                                   Price best_bid, Price best_ask)
    {
        if (order.type != AdvancedOrderType::Stop && 
            order.type != AdvancedOrderType::StopLimit &&
            order.type != AdvancedOrderType::TrailingStop)
        {
            return false;
        }
        
        const auto* stop_data = std::get_if<StopOrderData>(&order.advanced_data);
        if (!stop_data) return false;
        
        if (stop_data->triggered) return false;
        
        Price trigger_price = 0;
        
        switch (stop_data->trigger_type)
        {
            case StopTriggerType::Last:
                trigger_price = current_price;
                break;
            case StopTriggerType::Bid:
                trigger_price = best_bid;
                break;
            case StopTriggerType::Ask:
                trigger_price = best_ask;
                break;
            case StopTriggerType::Mid:
                trigger_price = (best_bid + best_ask) / 2;
                break;
            default:
                trigger_price = current_price;
        }
        
        // For sell stops, trigger when price falls below stop price
        // For buy stops, trigger when price rises above stop price
        if (order.side == Side::Sell)
        {
            return trigger_price <= stop_data->stop_price;
        }
        else
        {
            return trigger_price >= stop_data->stop_price;
        }
    }
    
    // Update trailing stop order
    void UpdateTrailingStop(AdvancedOrder& order, Price current_price, 
                           Price best_bid, Price best_ask)
    {
        if (order.type != AdvancedOrderType::TrailingStop) return;
        (void)best_bid;
        (void)best_ask;
        
        auto* trailing = std::get_if<TrailingStopOrderData>(&order.advanced_data);
        if (!trailing) return;
        
        // Update reference price based on market movement
        if (order.side == Side::Sell)
        {
            // For sell trailing stops, track highest high
            if (current_price > trailing->highest_high)
            {
                trailing->highest_high = current_price;
                
                // Update stop price based on trailing distance
                if (trailing->trailing_type == TrailingType::Fixed)
                {
                    trailing->stop_data.stop_price = trailing->highest_high - trailing->trailing_distance;
                }
                else if (trailing->trailing_type == TrailingType::Percentage)
                {
                    trailing->stop_data.stop_price = trailing->highest_high * (1.0 - trailing->trailing_distance / 100.0);
                }
            }
        }
        else
        {
            // For buy trailing stops, track lowest low
            if (trailing->lowest_low == 0 || current_price < trailing->lowest_low)
            {
                trailing->lowest_low = current_price;
                
                // Update stop price based on trailing distance
                if (trailing->trailing_type == TrailingType::Fixed)
                {
                    trailing->stop_data.stop_price = trailing->lowest_low + trailing->trailing_distance;
                }
                else if (trailing->trailing_type == TrailingType::Percentage)
                {
                    trailing->stop_data.stop_price = trailing->lowest_low * (1.0 + trailing->trailing_distance / 100.0);
                }
            }
        }
    }
    
    // Check if GTD order has expired
    [[nodiscard]] bool HasExpired(const AdvancedOrder& order)
    {
        if (order.type != AdvancedOrderType::GTD) return false;
        
        const auto* gtd = std::get_if<GTDOrderData>(&order.advanced_data);
        if (!gtd) return false;
        
        if (gtd->expired) return true;
        
        auto now = std::chrono::system_clock::now();
        if (now >= gtd->expiry_time)
        {
            const_cast<AdvancedOrder&>(order).advanced_data = 
                std::get<GTDOrderData>(order.advanced_data);
            auto* mutable_gtd = std::get_if<GTDOrderData>(&
                const_cast<AdvancedOrder&>(order).advanced_data);
            if (mutable_gtd) mutable_gtd->expired = true;
            return true;
        }
        
        return false;
    }
    
    // Refresh iceberg order (show more quantity)
    void RefreshIceberg(AdvancedOrder& order, Quantity new_visible_quantity)
    {
        if (order.type != AdvancedOrderType::Iceberg) return;
        
        auto* iceberg = std::get_if<IcebergOrderData>(&order.advanced_data);
        if (!iceberg) return;
        
        Quantity remaining = iceberg->total_quantity - order.filled_quantity.load();
        
        if (new_visible_quantity > remaining)
        {
            new_visible_quantity = remaining;
        }
        
        order.quantity = new_visible_quantity;
        iceberg->visible_quantity = new_visible_quantity;
        iceberg->displayed_so_far += new_visible_quantity;
        iceberg->refresh_count++;
        
        order.modified_time = std::chrono::system_clock::now();
    }
    
    // Get order type name for logging/metrics
    [[nodiscard]] const char* GetOrderTypeName(AdvancedOrderType type)
    {
        switch (type)
        {
            case AdvancedOrderType::Iceberg: return "Iceberg";
            case AdvancedOrderType::Hidden: return "Hidden";
            case AdvancedOrderType::Stop: return "Stop";
            case AdvancedOrderType::StopLimit: return "StopLimit";
            case AdvancedOrderType::TrailingStop: return "TrailingStop";
            case AdvancedOrderType::OCO: return "OCO";
            case AdvancedOrderType::GTD: return "GTD";
            case AdvancedOrderType::MOC: return "MOC";
            case AdvancedOrderType::MOO: return "MOO";
            case AdvancedOrderType::Pegged: return "Pegged";
            case AdvancedOrderType::Discretionary: return "Discretionary";
            case AdvancedOrderType::Cross: return "Cross";
            case AdvancedOrderType::Swap: return "Swap";
            case AdvancedOrderType::Algo: return "Algo";
            case AdvancedOrderType::Auction: return "Auction";
            case AdvancedOrderType::Retail: return "Retail";
            case AdvancedOrderType::Institutional: return "Institutional";
            default: return "Unknown";
        }
    }
}

// Convenience aliases
using IcebergOrder = AdvancedOrder;
using HiddenOrder = AdvancedOrder;
using StopOrder = AdvancedOrder;
using StopLimitOrder = AdvancedOrder;
using TrailingStopOrder = AdvancedOrder;
using OCOOrder = AdvancedOrder;
using GTDOrder = AdvancedOrder;
using PeggedOrder = AdvancedOrder;
using DiscretionaryOrder = AdvancedOrder;
