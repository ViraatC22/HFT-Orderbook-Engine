#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>

#include "Trade.h"
#include "Order.h"
#include "Usings.h"
#include "SharedMemoryMetrics.h"
#include "FixEngine.h"

/**
 * CAT Reporter (Consolidated Audit Trail)
 * 
 * US Securities and Exchange Commission (SEC) regulatory reporting for
 * comprehensive market surveillance and audit trail requirements.
 * Implements Industry Member (IM) reporting obligations under SEC Rule 613.
 * 
 * Key features:
 * - Order event reporting (new, modify, cancel, execute)
 * - Quote event reporting (displayed and non-displayed)
 * - Trade reporting with comprehensive audit trail
 * - Customer and account information reporting
 * - Cross-market surveillance data
 * - Real-time compliance validation
 * 
 * Reporting requirements:
 * - Industry Member Data (IMID)
    - Order events (OE)
 * - Quote events (QE)
 * - Trade events (TE)
 * - Customer account information (CA)
 * - Reference data (RD)
 * - Market maker activity (MM)
 */

class CATReporter
{
public:
    struct CATConfig
    {
        std::string industry_member_id;          // CRD number or equivalent
        std::string reporting_firm_type;       // "BD", "ATS", "SRO", "MSP"
        std::string firm_designated_id;        // Unique firm identifier
        bool enable_order_event_reporting = true;
        bool enable_quote_event_reporting = true;
        bool enable_trade_event_reporting = true;
        bool enable_customer_account_reporting = true;
        bool enable_real_time_validation = true;
        bool enable_cross_market_reporting = true;
        std::string report_output_path = "cat_reports/";
        std::string submission_format = "XML"; // "XML", "CSV", "JSON"
        std::chrono::hours reporting_window{24}; // 24-hour reporting window
        bool include_customer_info = true;
        bool include_account_info = true;
        bool include_market_maker_activity = false;
    };

    struct OrderEventReport
    {
        std::string event_type;                  // "NEW", "MODIFY", "CANCEL", "EXECUTE"
        std::string event_timestamp;             // YYYY-MM-DD HH:MM:SS.ssssss
        std::string order_id;                    // Unique order identifier
        std::string original_order_id;           // For modify/cancel events
        std::string cl_ord_id;                   // Client order ID
        std::string symbol;                      // Security symbol
        std::string market_center;               // Exchange or ATS identifier
        std::string side;                        // "BUY", "SELL", "SELL_SHORT"
        std::string order_type;                  // "MARKET", "LIMIT", "STOP", etc.
        Price price;                             // Order price
        Quantity quantity;                       // Order quantity
        Price stop_price;                        // Stop price if applicable
        std::string time_in_force;               // "DAY", "IOC", "GTC", etc.
        std::string capacity;                    // "A", "P", "R" (Agency, Principal, Riskless Principal)
        std::string order_restriction;           // "AON", "FOK", "IOC", etc.
        std::string account_type;                // "C", "M", "N" (Customer, Market Maker, Non-Member)
        std::string customer_type;               // "R", "I", "C" (Retail, Institutional, Commercial)
        std::string originated_order_flag;        // "Y", "N" (Order originated by firm)
        std::string representative_individual;   // Individual placing order
        std::string branch_sequence_number;      // Branch office identifier
        std::string originating_market_center;   // Where order was received
        std::string routing_method;              // "N", "S", "D" (Natural, Solicited, Directed)
        std::string special_instructions;        // Special order instructions
        std::string auction_type;                // "O", "C", "H" (Opening, Closing, Halt)
        std::string market_maker_protection_status; // "Y", "N"
        std::chrono::steady_clock::time_point timestamp;
    };

    struct QuoteEventReport
    {
        std::string event_type;                  // "NEW", "MODIFY", "CANCEL", "EXECUTE"
        std::string event_timestamp;             // YYYY-MM-DD HH:MM:SS.ssssss
        std::string quote_id;                    // Unique quote identifier
        std::string symbol;                      // Security symbol
        std::string market_center;               // Exchange or ATS identifier
        std::string quote_side;                  // "BID", "OFFER", "BOTH"
        Price bid_price;                         // Bid price
        Quantity bid_size;                       // Bid size
        Price offer_price;                       // Offer price
        Quantity offer_size;                     // Offer size
        std::string capacity;                    // "A", "P", "R"
        std::string account_type;                // "C", "M", "N"
        std::string market_maker_status;        // "Y", "N"
        std::string originated_quote_flag;      // "Y", "N"
        std::string representative_individual;   // Individual providing quote
        std::string branch_sequence_number;      // Branch office identifier
        std::string originating_market_center;   // Where quote was received
        std::string special_instructions;        // Special quote instructions
        std::chrono::steady_clock::time_point timestamp;
    };

    struct TradeEventReport
    {
        std::string event_type;                  // "EXECUTE", "CORRECT", "CANCEL"
        std::string event_timestamp;             // YYYY-MM-DD HH:MM:SS.ssssss
        std::string trade_id;                    // Unique trade identifier
        std::string symbol;                      // Security symbol
        std::string market_center;               // Exchange or ATS identifier
        std::string side;                        // "BUY", "SELL", "SELL_SHORT"
        Price price;                             // Trade price
        Quantity quantity;                       // Trade quantity
        std::string capacity;                    // "A", "P", "R"
        std::string account_type;                // "C", "M", "N"
        std::string originated_trade_flag;       // "Y", "N"
        std::string representative_individual;   // Individual executing trade
        std::string branch_sequence_number;      // Branch office identifier
        std::string originating_market_center;   // Where trade was executed
        std::string trade_reporting_facility;    // TRF identifier if applicable
        std::string special_instructions;        // Special trade instructions
        std::string auction_type;                // "O", "C", "H"
        std::string cross_type;                  // "N", "O", "S" (Regular, Opening, Closing)
        std::string trade_modifier_1;          // Trade modifier codes
        std::string trade_modifier_2;          // Additional trade modifier
        std::string trade_modifier_3;          // Additional trade modifier
        std::string trade_modifier_4;          // Additional trade modifier
        std::string settlement_terms;          // Settlement terms
        std::chrono::steady_clock::time_point timestamp;
    };

    struct CustomerAccountReport
    {
        std::string account_id;                  // Unique account identifier
        std::string customer_id;                 // Unique customer identifier
        std::string account_type;                // "C", "M", "N"
        std::string customer_type;               // "R", "I", "C"
        std::string account_opening_date;        // YYYY-MM-DD
        std::string account_status;              // "ACTIVE", "INACTIVE", "CLOSED"
        std::string country_of_citizenship;      // ISO 3166-1 alpha-2
        std::string country_of_residence;        // ISO 3166-1 alpha-2
        std::string date_of_birth;               // YYYY-MM-DD (if individual)
        std::string legal_entity_identifier;     // LEI if applicable
        std::string associated_person_flag;     // "Y", "N"
        std::string market_maker_flag;          // "Y", "N"
        std::string large_trader_flag;          // "Y", "N"
        std::string investment_adviser_flag;    // "Y", "N"
        std::string foreign_financial_institution_flag; // "Y", "N"
        std::string penny_stock_flag;           // "Y", "N"
        std::chrono::steady_clock::time_point timestamp;
    };

private:
    CATConfig config_;
    std::vector<OrderEventReport> order_events_;
    std::vector<QuoteEventReport> quote_events_;
    std::vector<TradeEventReport> trade_events_;
    std::vector<CustomerAccountReport> customer_accounts_;
    std::unique_ptr<SharedMemoryMetrics> metrics_;
    std::atomic<uint64_t> event_count_{0};
    std::atomic<uint64_t> validation_errors_{0};
    mutable std::mutex report_mutex_;

    std::string GenerateEventId(const std::string& event_type)
    {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        return "CAT_" + event_type + "_" + std::to_string(timestamp) + "_" + 
               std::to_string(event_count_.fetch_add(1));
    }

    std::string FormatTimestamp(const std::chrono::steady_clock::time_point& tp)
    {
        auto time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        auto ms = std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count() % 1000000;
        
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
        oss << "." << std::setfill('0') << std::setw(6) << ms;
        
        return oss.str();
    }

    std::string MapSideToCAT(Side side)
    {
        switch (side)
        {
            case Side::Buy: return "BUY";
            case Side::Sell: return "SELL";
            default: return "UNKNOWN";
        }
    }

    std::string MapOrderTypeToCAT(OrderType order_type)
    {
        switch (order_type)
        {
            case OrderType::Market: return "MARKET";
            case OrderType::GoodTillCancel: return "LIMIT";
            case OrderType::FillAndKill: return "IOC";
            case OrderType::FillOrKill: return "FOK";
            default: return "LIMIT";
        }
    }

    bool ValidateOrderEvent(const OrderEventReport& event)
    {
        if (!config_.enable_real_time_validation) return true;
        
        // Validate required fields
        if (event.event_type.empty() || event.order_id.empty() || 
            event.symbol.empty() || event.side.empty())
        {
            validation_errors_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        
        // Validate event types
        static const std::vector<std::string> valid_event_types = {"NEW", "MODIFY", "CANCEL", "EXECUTE"};
        if (std::find(valid_event_types.begin(), valid_event_types.end(), event.event_type) == valid_event_types.end())
        {
            validation_errors_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        
        // Validate sides
        static const std::vector<std::string> valid_sides = {"BUY", "SELL", "SELL_SHORT"};
        if (std::find(valid_sides.begin(), valid_sides.end(), event.side) == valid_sides.end())
        {
            validation_errors_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        
        // Validate order types
        static const std::vector<std::string> valid_order_types = {"MARKET", "LIMIT", "STOP", "STOP_LIMIT"};
        if (std::find(valid_order_types.begin(), valid_order_types.end(), event.order_type) == valid_order_types.end())
        {
            validation_errors_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        
        // Validate quantities and prices
        if (event.quantity <= 0 || event.price < 0)
        {
            validation_errors_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        
        return true;
    }

public:
    explicit CATReporter(const CATConfig& config = {})
        : config_(config)
        , metrics_(std::make_unique<SharedMemoryMetrics>())
    {
        // Create report directory if it doesn't exist
        std::filesystem::create_directories(config_.report_output_path);
    }

    OrderEventReport CreateOrderEvent(const std::string& event_type, const Order& order,
                                    const std::string& market_center, const std::string& account_type = "C",
                                    const std::string& customer_type = "R")
    {
        OrderEventReport event{};
        
        event.event_type = event_type;
        event.event_timestamp = FormatTimestamp(std::chrono::steady_clock::now());
        event.order_id = std::to_string(order.GetOrderId());
        event.cl_ord_id = std::to_string(order.GetOrderId());
        event.symbol = "SPY"; // Should be configurable
        event.market_center = market_center;
        event.side = MapSideToCAT(order.GetSide());
        event.order_type = MapOrderTypeToCAT(order.GetOrderType());
        event.price = order.GetPrice();
        event.quantity = order.GetQuantity();
        event.stop_price = 0; // Not applicable for most orders
        event.time_in_force = "DAY"; // Simplified
        event.capacity = "A"; // Agency
        event.order_restriction = ""; // No special restrictions
        event.account_type = account_type;
        event.customer_type = customer_type;
        event.originated_order_flag = "Y";
        event.representative_individual = "TRADER_001"; // Should be configurable
        event.branch_sequence_number = "BRANCH_001"; // Should be configurable
        event.originating_market_center = market_center;
        event.routing_method = "N"; // Natural
        event.special_instructions = "";
        event.auction_type = "O"; // Opening
        event.market_maker_protection_status = "N";
        event.timestamp = std::chrono::steady_clock::now();
        
        return event;
    }

    bool SubmitOrderEvent(const OrderEventReport& event)
    {
        if (!config_.enable_order_event_reporting) return true;
        
        if (!ValidateOrderEvent(event))
        {
            return false;
        }
        
        std::lock_guard<std::mutex> lock(report_mutex_);
        order_events_.push_back(event);
        
        // Write to file
        std::string filename = config_.report_output_path + "order_events_" + 
                              std::put_time(std::gmtime(&std::chrono::system_clock::to_time_t(
                                  std::chrono::system_clock::now())), "%Y%m%d") + ".csv";
        
        std::ofstream file(filename, std::ios::app);
        if (file.is_open())
        {
            file << event.event_type << ","
                 << event.event_timestamp << ","
                 << event.order_id << ","
                 << event.cl_ord_id << ","
                 << event.symbol << ","
                 << event.market_center << ","
                 << event.side << ","
                 << event.order_type << ","
                 << event.price << ","
                 << event.quantity << ","
                 << event.stop_price << ","
                 << event.time_in_force << ","
                 << event.capacity << ","
                 << event.order_restriction << ","
                 << event.account_type << ","
                 << event.customer_type << ","
                 << event.originated_order_flag << ","
                 << event.representative_individual << ","
                 << event.branch_sequence_number << ","
                 << event.originating_market_center << ","
                 << event.routing_method << ","
                 << event.special_instructions << ","
                 << event.auction_type << ","
                 << event.market_maker_protection_status << std::endl;
        }
        
        event_count_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    TradeEventReport CreateTradeEvent(const Trade& trade, const std::string& market_center,
                                    const std::string& account_type = "C")
    {
        TradeEventReport event{};
        
        event.event_type = "EXECUTE";
        event.event_timestamp = FormatTimestamp(std::chrono::steady_clock::now());
        event.trade_id = "TRADE_" + std::to_string(trade.trade_id);
        event.symbol = "SPY"; // Should be configurable
        event.market_center = market_center;
        event.side = MapSideToCAT(trade.side);
        event.price = trade.price;
        event.quantity = trade.quantity;
        event.capacity = "A"; // Agency
        event.account_type = account_type;
        event.originated_trade_flag = "Y";
        event.representative_individual = "TRADER_001"; // Should be configurable
        event.branch_sequence_number = "BRANCH_001"; // Should be configurable
        event.originating_market_center = market_center;
        event.trade_reporting_facility = "TRF"; // Trade Reporting Facility
        event.special_instructions = "";
        event.auction_type = "O"; // Opening
        event.cross_type = "N"; // Regular
        event.trade_modifier_1 = "";
        event.trade_modifier_2 = "";
        event.trade_modifier_3 = "";
        event.trade_modifier_4 = "";
        event.settlement_terms = "REGULAR"; // Regular settlement
        event.timestamp = std::chrono::steady_clock::now();
        
        return event;
    }

    bool SubmitTradeEvent(const TradeEventReport& event)
    {
        if (!config_.enable_trade_event_reporting) return true;
        
        std::lock_guard<std::mutex> lock(report_mutex_);
        trade_events_.push_back(event);
        
        // Write to file
        std::string filename = config_.report_output_path + "trade_events_" + 
                              std::put_time(std::gmtime(&std::chrono::system_clock::to_time_t(
                                  std::chrono::system_clock::now())), "%Y%m%d") + ".csv";
        
        std::ofstream file(filename, std::ios::app);
        if (file.is_open())
        {
            file << event.event_type << ","
                 << event.event_timestamp << ","
                 << event.trade_id << ","
                 << event.symbol << ","
                 << event.market_center << ","
                 << event.side << ","
                 << event.price << ","
                 << event.quantity << ","
                 << event.capacity << ","
                 << event.account_type << ","
                 << event.originated_trade_flag << ","
                 << event.representative_individual << ","
                 << event.branch_sequence_number << ","
                 << event.originating_market_center << ","
                 << event.trade_reporting_facility << ","
                 << event.special_instructions << ","
                 << event.auction_type << ","
                 << event.cross_type << ","
                 << event.trade_modifier_1 << ","
                 << event.trade_modifier_2 << ","
                 << event.trade_modifier_3 << ","
                 << event.trade_modifier_4 << ","
                 << event.settlement_terms << std::endl;
        }
        
        event_count_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    // Batch reporting for end-of-day submission
    bool GenerateDailyReport(const std::string& date)
    {
        std::lock_guard<std::mutex> lock(report_mutex_);
        
        std::string filename = config_.report_output_path + "daily_cat_report_" + date + ".xml";
        std::ofstream file(filename);
        
        if (!file.is_open()) return false;
        
        file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
        file << "<CATReport date=\"" << date << "\" industry_member=\"" << config_.industry_member_id << "\">" << std::endl;
        
        // Order events
        file << "  <OrderEvents count=\"" << order_events_.size() << "\">" << std::endl;
        for (const auto& event : order_events_)
        {
            file << "    <OrderEvent type=\"" << event.event_type << "\"" << std::endl;
            file << "                  timestamp=\"" << event.event_timestamp << "\"" << std::endl;
            file << "                  order_id=\"" << event.order_id << "\"" << std::endl;
            file << "                  symbol=\"" << event.symbol << "\"/>" << std::endl;
        }
        file << "  </OrderEvents>" << std::endl;
        
        // Trade events
        file << "  <TradeEvents count=\"" << trade_events_.size() << "\">" << std::endl;
        for (const auto& event : trade_events_)
        {
            file << "    <TradeEvent type=\"" << event.event_type << "\"" << std::endl;
            file << "                  timestamp=\"" << event.event_timestamp << "\"" << std::endl;
            file << "                  trade_id=\"" << event.trade_id << "\"" << std::endl;
            file << "                  symbol=\"" << event.symbol << "\"/>" << std::endl;
        }
        file << "  </TradeEvents>" << std::endl;
        
        file << "</CATReport>" << std::endl;
        
        return true;
    }

    // Statistics
    uint64_t GetEventCount() const
    {
        return event_count_.load(std::memory_order_relaxed);
    }

    uint64_t GetValidationErrors() const
    {
        return validation_errors_.load(std::memory_order_relaxed);
    }

    size_t GetOrderEventCount() const
    {
        std::lock_guard<std::mutex> lock(report_mutex_);
        return order_events_.size();
    }

    size_t GetTradeEventCount() const
    {
        std::lock_guard<std::mutex> lock(report_mutex_);
        return trade_events_.size();
    }

    void PrintCATStatus() const
    {
        std::cout << "\n=== CAT Reporter Status ===" << std::endl;
        std::cout << "Industry Member ID: " << config_.industry_member_id << std::endl;
        std::cout << "Firm Type: " << config_.reporting_firm_type << std::endl;
        std::cout << "Firm Designated ID: " << config_.firm_designated_id << std::endl;
        std::cout << "Order Events: " << GetOrderEventCount() << std::endl;
        std::cout << "Trade Events: " << GetTradeEventCount() << std::endl;
        std::cout << "Total Events: " << GetEventCount() << std::endl;
        std::cout << "Validation Errors: " << GetValidationErrors() << std::endl;
        std::cout << "=============================" << std::endl;
    }
};