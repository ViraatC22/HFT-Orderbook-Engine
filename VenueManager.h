#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <chrono>
#include <variant>
#include <functional>
#include <algorithm>

#include "Orderbook.h"
#include "Order.h"
#include "Trade.h"
#include "SharedMemoryMetrics.h"
#include "PerformanceMonitor.h"

/**
 * Multi-Asset/Cross-Venue Architecture
 * 
 * Professional HFT system supporting multiple exchanges and asset classes.
 * Transforms single-market engine into scalable multi-venue trading platform.
 * 
 * Key features:
 * - Template-based Orderbook<AssetType> for asset-specific logic
 * - VenueManager coordinating multiple independent orderbooks
 * - Cross-venue risk aggregation and position management
 * - Venue-specific order type mapping and validation
 * - Centralized compliance and reporting across all venues
 * - Symbol mapping for consistent instrument identification
 * 
 * Architecture:
 * - VenueManager: Central coordinator for all venues and assets
 * - MultiAssetOrderbook: Template-based orderbook per symbol/venue
 * - SymbolMapper: Consistent instrument identification
 * - CrossVenueArbitrage: Inter-venue order routing
 * - VenueRiskAggregator: Centralized risk across venues
 */

// Asset type definitions for template specialization
template<typename AssetType>
struct AssetTraits;

// Equity asset specialization
struct EquityAsset
{
    static constexpr const char* Name = "EQUITY";
    static constexpr bool SupportsMarketOrders = true;
    static constexpr bool SupportsIcebergOrders = true;
    static constexpr size_t MaxPriceLevels = 10000;
    static constexpr size_t PriceMultiplier = 100; // Cent-based pricing
};

template<>
struct AssetTraits<EquityAsset>
{
    using PriceType = uint32_t;  // Cents
    using QuantityType = uint32_t;
    static constexpr const char* AssetClass = "EQUITY";
    static constexpr bool RequiresRegNMSCompliance = true;
    static constexpr double MinimumPriceIncrement = 0.01; // Penny increment
};

// Futures asset specialization
struct FuturesAsset
{
    static constexpr const char* Name = "FUTURES";
    static constexpr bool SupportsMarketOrders = true;
    static constexpr bool SupportsIcebergOrders = false;
    static constexpr size_t MaxPriceLevels = 5000;
    static constexpr size_t PriceMultiplier = 1000; // Milli-based pricing
};

template<>
struct AssetTraits<FuturesAsset>
{
    using PriceType = uint32_t;  // Millis
    using QuantityType = uint32_t;
    static constexpr const char* AssetClass = "FUTURES";
    static constexpr bool RequiresRegNMSCompliance = false;
    static constexpr double MinimumPriceIncrement = 0.001; // Milli increment
};

// FX asset specialization
struct FXAsset
{
    static constexpr const char* Name = "FX";
    static constexpr bool SupportsMarketOrders = true;
    static constexpr bool SupportsIcebergOrders = true;
    static constexpr size_t MaxPriceLevels = 2000;
    static constexpr size_t PriceMultiplier = 100000; // PIP-based pricing
};

template<>
struct AssetTraits<FXAsset>
{
    using PriceType = uint32_t;  // PIPs (1/100000)
    using QuantityType = uint64_t; // Large FX quantities
    static constexpr const char* AssetClass = "FX";
    static constexpr bool RequiresRegNMSCompliance = false;
    static constexpr double MinimumPriceIncrement = 0.00001; // PIP increment
};

// Venue configuration and capabilities
struct VenueConfig
{
    std::string venue_name;
    std::string venue_code;      // Internal venue identifier
    std::string mic_code;        // Market Identifier Code (ISO 10383)
    std::string country_code;    // ISO 3166-1 alpha-2
    std::vector<std::string> supported_asset_classes;
    bool supports_market_data;   // Venue provides market data
    bool supports_order_routing; // Venue accepts orders
    bool requires_pre_trade_risk; // Pre-trade risk checks required
    double max_order_size;       // Venue-specific size limits
    double max_price_deviation;  // Venue-specific price limits
    std::chrono::milliseconds max_latency_ms; // SLA requirement
};

// Symbol mapping for cross-venue consistency
class SymbolMapper
{
public:
    struct SymbolMapping
    {
        std::string internal_symbol;     // Internal unified symbol
        std::string venue_symbol;        // Venue-specific symbol
        std::string isin;                // International Securities Identification Number
        std::string cusip;               // Committee on Uniform Securities Identification Procedures
        std::string sedol;               // Stock Exchange Daily Official List
        std::string ric;                 // Reuters Instrument Code
        std::string bloomberg_ticker;    // Bloomberg ticker
        std::string asset_class;
        std::string currency;
        double tick_size;
        uint32_t lot_size;
    };

private:
    std::unordered_map<std::string, SymbolMapping> symbol_mappings_; // internal_symbol -> mapping
    std::unordered_map<std::string, std::string> venue_to_internal_; // venue_symbol -> internal_symbol
    std::unordered_map<std::string, std::vector<std::string>> internal_to_venues_; // internal_symbol -> venue_symbols

public:
    void AddSymbolMapping(const SymbolMapping& mapping)
    {
        symbol_mappings_[mapping.internal_symbol] = mapping;
        venue_to_internal_[mapping.venue_symbol] = mapping.internal_symbol;
        internal_to_venues_[mapping.internal_symbol].push_back(mapping.venue_symbol);
    }

    const SymbolMapping* GetInternalMapping(const std::string& internal_symbol) const
    {
        auto it = symbol_mappings_.find(internal_symbol);
        return (it != symbol_mappings_.end()) ? &it->second : nullptr;
    }

    const SymbolMapping* GetVenueMapping(const std::string& venue_symbol) const
    {
        auto it = venue_to_internal_.find(venue_symbol);
        if (it == venue_to_internal_.end()) return nullptr;
        
        auto mapping_it = symbol_mappings_.find(it->second);
        return (mapping_it != symbol_mappings_.end()) ? &mapping_it->second : nullptr;
    }

    std::vector<std::string> GetVenueSymbols(const std::string& internal_symbol) const
    {
        auto it = internal_to_venues_.find(internal_symbol);
        return (it != internal_to_venues_.end()) ? it->second : std::vector<std::string>{};
    }

    bool IsValidSymbol(const std::string& symbol) const
    {
        return symbol_mappings_.find(symbol) != symbol_mappings_.end() ||
               venue_to_internal_.find(symbol) != venue_to_internal_.end();
    }
};

// Cross-venue position and risk aggregation
class VenueRiskAggregator
{
public:
    struct PositionSnapshot
    {
        std::string internal_symbol;
        std::unordered_map<std::string, int64_t> venue_positions; // venue -> position
        int64_t net_position;
        double notional_exposure;
        double average_price;
        std::chrono::steady_clock::time_point timestamp;
    };

    struct RiskMetrics
    {
        double total_notional_exposure;
        double max_single_venue_exposure;
        double net_exposure;
        double gross_exposure;
        uint32_t symbol_count;
        uint32_t venue_count;
        std::chrono::steady_clock::time_point timestamp;
    };

private:
    std::unordered_map<std::string, PositionSnapshot> positions_; // internal_symbol -> position
    std::unordered_map<std::string, double> venue_exposures_; // venue -> total exposure
    std::atomic<double> total_exposure_{0.0};
    mutable std::mutex position_mutex_;

public:
    void UpdatePosition(const std::string& internal_symbol, const std::string& venue, 
                       int64_t position_change, double price)
    {
        std::lock_guard<std::mutex> lock(position_mutex_);
        
        auto& snapshot = positions_[internal_symbol];
        snapshot.internal_symbol = internal_symbol;
        snapshot.venue_positions[venue] += position_change;
        
        // Recalculate net position
        snapshot.net_position = 0;
        for (const auto& [v, pos] : snapshot.venue_positions)
        {
            snapshot.net_position += pos;
        }
        
        snapshot.notional_exposure = snapshot.net_position * price;
        snapshot.average_price = (snapshot.average_price + price) / 2.0; // Simplified
        snapshot.timestamp = std::chrono::steady_clock::now();
        
        // Update venue exposures
        double old_venue_exposure = venue_exposures_[venue];
        venue_exposures_[venue] = 0.0;
        
        for (const auto& [sym, pos_snapshot] : positions_)
        {
            auto venue_it = pos_snapshot.venue_positions.find(venue);
            if (venue_it != pos_snapshot.venue_positions.end())
            {
                venue_exposures_[venue] += venue_it->second * pos_snapshot.average_price;
            }
        }
        
        // Update total exposure
        total_exposure_.store(0.0, std::memory_order_relaxed);
        for (const auto& [v, exposure] : venue_exposures_)
        {
            total_exposure_.fetch_add(exposure, std::memory_order_relaxed);
        }
    }

    PositionSnapshot GetPosition(const std::string& internal_symbol) const
    {
        std::lock_guard<std::mutex> lock(position_mutex_);
        auto it = positions_.find(internal_symbol);
        return (it != positions_.end()) ? it->second : PositionSnapshot{};
    }

    RiskMetrics GetRiskMetrics() const
    {
        std::lock_guard<std::mutex> lock(position_mutex_);
        
        RiskMetrics metrics{};
        metrics.timestamp = std::chrono::steady_clock::now();
        metrics.total_notional_exposure = total_exposure_.load(std::memory_order_relaxed);
        metrics.symbol_count = positions_.size();
        metrics.venue_count = venue_exposures_.size();
        
        double max_single_exposure = 0.0;
        double gross_exposure = 0.0;
        double net_exposure = 0.0;
        
        for (const auto& [venue, exposure] : venue_exposures_)
        {
            max_single_exposure = std::max(max_single_exposure, std::abs(exposure));
            gross_exposure += std::abs(exposure);
        }
        
        for (const auto& [symbol, snapshot] : positions_)
        {
            net_exposure += snapshot.notional_exposure;
        }
        
        metrics.max_single_venue_exposure = max_single_exposure;
        metrics.gross_exposure = gross_exposure;
        metrics.net_exposure = net_exposure;
        
        return metrics;
    }
};

// Template-based multi-asset orderbook
template<typename AssetType>
class MultiAssetOrderbook : public Orderbook
{
public:
    using AssetTraitsType = AssetTraits<AssetType>;
    using PriceType = typename AssetTraitsType::PriceType;
    using QuantityType = typename AssetTraitsType::QuantityType;

private:
    std::string venue_name_;
    std::string internal_symbol_;
    std::string venue_symbol_;
    AssetType asset_config_;
    
    // Asset-specific validations
    bool ValidatePrice(Price price) const
    {
        // Check minimum price increment
        double price_double = static_cast<double>(price) / AssetTraitsType::PriceMultiplier;
        double remainder = std::fmod(price_double, AssetTraitsType::MinimumPriceIncrement);
        return std::abs(remainder) < 0.000001; // Floating point tolerance
    }

    bool ValidateQuantity(Quantity quantity) const
    {
        // Check lot size requirements
        return (quantity % AssetTraitsType::lot_size) == 0;
    }

public:
    MultiAssetOrderbook(const std::string& venue_name, const std::string& internal_symbol,
                       const std::string& venue_symbol)
        : Orderbook()
        , venue_name_(venue_name)
        , internal_symbol_(internal_symbol)
        , venue_symbol_(venue_symbol)
    {
    }

    // Asset-specific AddOrder with validation
    OrderPointer AddOrder(OrderPointer order) override
    {
        if (!ValidatePrice(order->GetPrice()) || !ValidateQuantity(order->GetQuantity()))
        {
            return nullptr; // Invalid order for this asset class
        }
        
        return Orderbook::AddOrder(order);
    }

    // Venue-specific order type mapping
    OrderType MapVenueOrderType(uint8_t venue_order_type) const
    {
        // Map venue-specific order types to internal types
        // This is venue and asset class specific
        switch (venue_order_type)
        {
            case 1: return OrderType::GoodTillCancel;
            case 2: return OrderType::FillAndKill;
            case 3: return OrderType::FillOrKill;
            case 4: return OrderType::Market;
            default: return OrderType::GoodTillCancel;
        }
    }

    const std::string& GetVenueName() const { return venue_name_; }
    const std::string& GetInternalSymbol() const { return internal_symbol_; }
    const std::string& GetVenueSymbol() const { return venue_symbol_; }
    const AssetType& GetAssetConfig() const { return asset_config_; }
    
    std::string GetAssetClass() const
    {
        return AssetTraitsType::AssetClass;
    }
};

// Central venue manager coordinating all venues and assets
class VenueManager
{
public:
    struct VenueRegistration
    {
        VenueConfig config;
        std::unique_ptr<SymbolMapper> symbol_mapper;
        std::unique_ptr<VenueRiskAggregator> risk_aggregator;
        std::chrono::steady_clock::time_point registration_time;
        bool active;
    };

    struct OrderbookRegistration
    {
        std::string internal_symbol;
        std::string venue_name;
        std::string venue_symbol;
        std::string asset_class;
        std::unique_ptr<Orderbook> orderbook;
        std::chrono::steady_clock::time_point creation_time;
        uint64_t order_count;
        uint64_t trade_count;
        double total_volume;
        bool active;
    };

private:
    std::unordered_map<std::string, VenueRegistration> venues_; // venue_name -> registration
    std::unordered_map<std::string, std::vector<std::string>> symbol_to_venues_; // internal_symbol -> venue_names
    std::unordered_map<std::string, std::string> venue_symbol_to_internal_; // venue_symbol -> internal_symbol
    
    // Orderbook management - multi-asset support
    std::unordered_map<std::string, OrderbookRegistration> orderbooks_; // composite_key -> registration
    
    std::atomic<uint64_t> total_orders_processed_{0};
    std::atomic<uint64_t> total_trades_executed_{0};
    std::atomic<double> total_volume_{0.0};
    
    std::unique_ptr<PerformanceMonitor> performance_monitor_;
    mutable std::mutex venue_mutex_;

    std::string MakeOrderbookKey(const std::string& internal_symbol, const std::string& venue_name) const
    {
        return internal_symbol + "@" + venue_name;
    }

public:
    explicit VenueManager()
    {
        PerformanceMonitor::MonitorConfig config;
        config.enable_papi = true;
        config.verbose_logging = false;
        performance_monitor_ = std::make_unique<PerformanceMonitor>(config);
    }

    // Venue registration
    bool RegisterVenue(const VenueConfig& venue_config)
    {
        std::lock_guard<std::mutex> lock(venue_mutex_);
        
        if (venues_.find(venue_config.venue_name) != venues_.end())
        {
            return false; // Venue already registered
        }
        
        VenueRegistration registration;
        registration.config = venue_config;
        registration.symbol_mapper = std::make_unique<SymbolMapper>();
        registration.risk_aggregator = std::make_unique<VenueRiskAggregator>();
        registration.registration_time = std::chrono::steady_clock::now();
        registration.active = true;
        
        venues_[venue_config.venue_name] = std::move(registration);
        
        if (performance_monitor_->GetConfig().verbose_logging)
        {
            std::cout << "[VenueManager] Registered venue: " << venue_config.venue_name << std::endl;
        }
        
        return true;
    }

    // Symbol mapping registration
    bool RegisterSymbolMapping(const std::string& venue_name, const SymbolMapper::SymbolMapping& mapping)
    {
        std::lock_guard<std::mutex> lock(venue_mutex_);
        
        auto venue_it = venues_.find(venue_name);
        if (venue_it == venues_.end())
        {
            return false; // Venue not registered
        }
        
        venue_it->second.symbol_mapper->AddSymbolMapping(mapping);
        
        // Update reverse mappings
        symbol_to_venues_[mapping.internal_symbol].push_back(venue_name);
        venue_symbol_to_internal_[mapping.venue_symbol] = mapping.internal_symbol;
        
        return true;
    }

    // Multi-asset orderbook creation
    template<typename AssetType>
    bool CreateOrderbook(const std::string& internal_symbol, const std::string& venue_name,
                        const std::string& venue_symbol)
    {
        std::lock_guard<std::mutex> lock(venue_mutex_);
        
        auto venue_it = venues_.find(venue_name);
        if (venue_it == venues_.end())
        {
            return false; // Venue not registered
        }
        
        std::string orderbook_key = MakeOrderbookKey(internal_symbol, venue_name);
        if (orderbooks_.find(orderbook_key) != orderbooks_.end())
        {
            return false; // Orderbook already exists
        }
        
        // Create asset-specific orderbook
        auto orderbook = std::make_unique<MultiAssetOrderbook<AssetType>>(
            venue_name, internal_symbol, venue_symbol);
        
        OrderbookRegistration registration;
        registration.internal_symbol = internal_symbol;
        registration.venue_name = venue_name;
        registration.venue_symbol = venue_symbol;
        registration.asset_class = AssetTraits<AssetType>::AssetClass;
        registration.orderbook = std::move(orderbook);
        registration.creation_time = std::chrono::steady_clock::now();
        registration.order_count = 0;
        registration.trade_count = 0;
        registration.total_volume = 0.0;
        registration.active = true;
        
        orderbooks_[orderbook_key] = std::move(registration);
        
        if (performance_monitor_->GetConfig().verbose_logging)
        {
            std::cout << "[VenueManager] Created orderbook: " << orderbook_key 
                     << " (" << registration.asset_class << ")" << std::endl;
        }
        
        return true;
    }

    // Submit order to specific venue
    OrderPointer SubmitOrder(const std::string& internal_symbol, const std::string& venue_name,
                            OrderPointer order)
    {
        std::lock_guard<std::mutex> lock(venue_mutex_);
        
        std::string orderbook_key = MakeOrderbookKey(internal_symbol, venue_name);
        auto orderbook_it = orderbooks_.find(orderbook_key);
        if (orderbook_it == orderbooks_.end())
        {
            return nullptr; // Orderbook not found
        }
        
        if (!orderbook_it->second.active)
        {
            return nullptr; // Orderbook inactive
        }
        
        // Pre-trade risk check
        auto venue_it = venues_.find(venue_name);
        if (venue_it != venues_.end() && venue_it->second.config.requires_pre_trade_risk)
        {
            // Perform pre-trade risk validation
            // This would integrate with existing RiskManager
        }
        
        // Submit order
        OrderPointer result = orderbook_it->second.orderbook->AddOrder(order);
        
        if (result)
        {
            orderbook_it->second.order_count++;
            total_orders_processed_.fetch_add(1, std::memory_order_relaxed);
            performance_monitor_->RecordTradeProcessed();
        }
        
        return result;
    }

    // Cross-venue order routing (smart order routing)
    std::vector<OrderPointer> SubmitCrossVenueOrder(const std::string& internal_symbol,
                                                    OrderPointer order,
                                                    const std::vector<std::string>& preferred_venues = {})
    {
        std::lock_guard<std::mutex> lock(venue_mutex_);
        
        std::vector<OrderPointer> results;
        
        // Get available venues for this symbol
        auto venue_it = symbol_to_venues_.find(internal_symbol);
        if (venue_it == symbol_to_venues_.end())
        {
            return results; // No venues available
        }
        
        const auto& available_venues = venue_it->second;
        
        // Determine routing strategy
        std::vector<std::string> target_venues;
        if (!preferred_venues.empty())
        {
            // Use preferred venues if specified
            target_venues = preferred_venues;
        }
        else
        {
            // Smart routing: route to all available venues
            target_venues = available_venues;
        }
        
        // Split order across venues (simplified)
        Quantity remaining_quantity = order->GetQuantity();
        size_t venue_count = target_venues.size();
        
        for (size_t i = 0; i < venue_count && remaining_quantity > 0; ++i)
        {
            const std::string& venue_name = target_venues[i];
            
            // Calculate venue allocation
            Quantity venue_quantity = remaining_quantity / (venue_count - i);
            if (venue_quantity == 0) venue_quantity = 1;
            
            // Create venue-specific order
            auto venue_order = std::make_shared<Order>(
                order->GetOrderId() + "_" + venue_name,
                order->GetSide(),
                venue_quantity,
                order->GetPrice(),
                order->GetOrderType()
            );
            
            // Submit to venue
            OrderPointer result = SubmitOrder(internal_symbol, venue_name, venue_order);
            if (result)
            {
                results.push_back(result);
                remaining_quantity -= venue_quantity;
            }
        }
        
        return results;
    }

    // Get orderbook for specific venue/symbol
    Orderbook* GetOrderbook(const std::string& internal_symbol, const std::string& venue_name)
    {
        std::lock_guard<std::mutex> lock(venue_mutex_);
        
        std::string orderbook_key = MakeOrderbookKey(internal_symbol, venue_name);
        auto it = orderbooks_.find(orderbook_key);
        return (it != orderbooks_.end() && it->second.active) ? it->second.orderbook.get() : nullptr;
    }

    // Get all orderbooks for a symbol
    std::vector<Orderbook*> GetSymbolOrderbooks(const std::string& internal_symbol)
    {
        std::lock_guard<std::mutex> lock(venue_mutex_);
        
        std::vector<Orderbook*> result;
        
        auto venue_it = symbol_to_venues_.find(internal_symbol);
        if (venue_it != symbol_to_venues_.end())
        {
            for (const std::string& venue_name : venue_it->second)
            {
                std::string orderbook_key = MakeOrderbookKey(internal_symbol, venue_name);
                auto orderbook_it = orderbooks_.find(orderbook_key);
                if (orderbook_it != orderbooks_.end() && orderbook_it->second.active)
                {
                    result.push_back(orderbook_it->second.orderbook.get());
                }
            }
        }
        
        return result;
    }

    // Performance monitoring
    PerformanceMonitor* GetPerformanceMonitor() 
    {
        return performance_monitor_.get();
    }

    // Risk aggregation
    VenueRiskAggregator* GetRiskAggregator(const std::string& venue_name)
    {
        std::lock_guard<std::mutex> lock(venue_mutex_);
        
        auto it = venues_.find(venue_name);
        return (it != venues_.end()) ? it->second.risk_aggregator.get() : nullptr;
    }

    // Global risk metrics
    VenueRiskAggregator::RiskMetrics GetGlobalRiskMetrics()
    {
        std::lock_guard<std::mutex> lock(venue_mutex_);
        
        VenueRiskAggregator::RiskMetrics global_metrics{};
        global_metrics.timestamp = std::chrono::steady_clock::now();
        
        double total_exposure = 0.0;
        double max_single_exposure = 0.0;
        double gross_exposure = 0.0;
        double net_exposure = 0.0;
        uint32_t symbol_count = 0;
        uint32_t venue_count = 0;
        
        for (const auto& [venue_name, registration] : venues_)
        {
            auto venue_metrics = registration.risk_aggregator->GetRiskMetrics();
            
            total_exposure += venue_metrics.total_notional_exposure;
            max_single_exposure = std::max(max_single_exposure, venue_metrics.max_single_venue_exposure);
            gross_exposure += venue_metrics.gross_exposure;
            net_exposure += venue_metrics.net_exposure;
            symbol_count = std::max(symbol_count, venue_metrics.symbol_count);
            venue_count++;
        }
        
        global_metrics.total_notional_exposure = total_exposure;
        global_metrics.max_single_venue_exposure = max_single_exposure;
        global_metrics.gross_exposure = gross_exposure;
        global_metrics.net_exposure = net_exposure;
        global_metrics.symbol_count = symbol_count;
        global_metrics.venue_count = venue_count;
        
        return global_metrics;
    }

    // Statistics
    uint64_t GetTotalOrdersProcessed() const
    {
        return total_orders_processed_.load(std::memory_order_relaxed);
    }

    uint64_t GetTotalTradesExecuted() const
    {
        return total_trades_executed_.load(std::memory_order_relaxed);
    }

    double GetTotalVolume() const
    {
        return total_volume_.load(std::memory_order_relaxed);
    }

    size_t GetVenueCount() const
    {
        std::lock_guard<std::mutex> lock(venue_mutex_);
        return venues_.size();
    }

    size_t GetOrderbookCount() const
    {
        std::lock_guard<std::mutex> lock(venue_mutex_);
        return orderbooks_.size();
    }

    void PrintVenueReport()
    {
        std::lock_guard<std::mutex> lock(venue_mutex_);
        
        std::cout << "\n=== Venue Manager Report ===" << std::endl;
        std::cout << "Venues Registered: " << venues_.size() << std::endl;
        std::cout << "Orderbooks Created: " << orderbooks_.size() << std::endl;
        std::cout << "Total Orders Processed: " << GetTotalOrdersProcessed() << std::endl;
        std::cout << "Total Trades Executed: " << GetTotalTradesExecuted() << std::endl;
        std::cout << "Total Volume: " << GetTotalVolume() << std::endl;
        std::cout << std::endl;
        
        std::cout << "Venues:" << std::endl;
        for (const auto& [venue_name, registration] : venues_)
        {
            std::cout << "  " << venue_name << " (" << registration.config.mic_code << ")" << std::endl;
            std::cout << "    Country: " << registration.config.country_code << std::endl;
            std::cout << "    Asset Classes: ";
            for (const auto& asset_class : registration.config.supported_asset_classes)
            {
                std::cout << asset_class << " ";
            }
            std::cout << std::endl;
        }
        
        std::cout << "Orderbooks:" << std::endl;
        for (const auto& [orderbook_key, registration] : orderbooks_)
        {
            std::cout << "  " << orderbook_key << " (" << registration.asset_class << ")" << std::endl;
            std::cout << "    Orders: " << registration.order_count << std::endl;
            std::cout << "    Trades: " << registration.trade_count << std::endl;
            std::cout << "    Volume: " << registration.total_volume << std::endl;
        }
        
        auto risk_metrics = GetGlobalRiskMetrics();
        std::cout << "Risk Metrics:" << std::endl;
        std::cout << "  Total Exposure: " << risk_metrics.total_notional_exposure << std::endl;
        std::cout << "  Max Single Venue Exposure: " << risk_metrics.max_single_venue_exposure << std::endl;
        std::cout << "  Net Exposure: " << risk_metrics.net_exposure << std::endl;
        std::cout << "  Gross Exposure: " << risk_metrics.gross_exposure << std::endl;
        
        std::cout << "================================" << std::endl;
    }
};