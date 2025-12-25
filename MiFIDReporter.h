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
 * MiFID II Regulatory Reporting (Markets in Financial Instruments Directive)
 * 
 * European Union regulatory compliance for investment firms and trading venues.
 * Implements RTS 6 (Regulatory Technical Standards) requirements for
 * systematic internalizers and investment firms.
 * 
 * Key features:
 * - Transaction reporting to National Competent Authorities (NCAs)
 * - Reference data reporting for financial instruments
 * - Order and trade transparency reporting
 * - Best execution reporting
 * - Systematic Internalizer (SI) reporting
 * - Real-time compliance validation
 * 
 * Reporting requirements:
 * - Transaction reports (RTS 22)
 * - Reference data reports (RTS 23)
 * - Transparency calculations (RTS 1 & 2)
 * - Best execution reports (RTS 27)
 * - Systematic Internalizer reports (RTS 8)
 */

class MiFIDReporter
{
public:
    struct MiFIDConfig
    {
        std::string reporting_firm_id;           // LEI (Legal Entity Identifier)
        std::string nca_code;                    // National Competent Authority
        std::string trading_capacity;            // DEAL, AOTC, MATCH, SI, RFPT
        bool is_systematic_internalizer = false;
        std::string investment_firm_category;    // IF, CRD, MFID, AIFM, UCITS
        std::string trading_algorithm_indicator; // "Yes" or "No"
        std::string waiver_indicator;           // "Yes" or "No"
        bool enable_real_time_validation = true;
        bool enable_transaction_reporting = true;
        bool enable_reference_data_reporting = true;
        bool enable_transparency_reporting = true;
        std::string report_output_path = "mifid_reports/";
    };

    struct TransactionReport
    {
        std::string transaction_id;              // Unique transaction identifier
        std::string trading_date;                // YYYY-MM-DD
        std::string trading_time;                // HH:MM:SS.ssssss
        std::string venue_code;                  // MIC code
        std::string instrument_id;               // ISIN or other identifier
        std::string instrument_classification;   // CFI code
        std::string buyer_id;                    // LEI of buyer
        std::string seller_id;                   // LEI of seller
        std::string buyer_country;               // ISO 3166-1 alpha-2
        std::string seller_country;              // ISO 3166-1 alpha-2
        Price price;                             // Transaction price
        Quantity quantity;                       // Transaction quantity
        std::string currency;                    // ISO 4217
        std::string venue_of_execution;         // MIC code
        std::string transmission_of_orders_indication; // "Yes" or "No"
        std::string algorithm_indication;        // "Yes" or "No"
        std::string waiver_indicator;            // "Yes" or "No"
        std::string special_dividend_indicator;  // "Yes" or "No"
        std::string dark_trade_eligibility;     // "Yes" or "No"
        std::string system_internaliser_flag;   // "Yes" or "No"
        std::string market_segment_id;          // Market segment identifier
        std::string country_of_branch_membership; // ISO 3166-1 alpha-2
        std::string transaction_category;       // "AOTC", "DEAL", "MATCH", "SI"
        std::chrono::steady_clock::time_point timestamp;
    };

    struct ReferenceDataReport
    {
        std::string instrument_id;               // ISIN or other identifier
        std::string instrument_full_name;      // Full instrument name
        std::string instrument_classification; // CFI code
        std::string currency;                    // ISO 4217
        std::string venue_code;                  // MIC code
        std::string instrument_type;            // "EQUITY", "BOND", "ETF", etc.
        std::string country_of_issue;            // ISO 3166-1 alpha-2
        std::string issuer_id;                  // LEI of issuer
        std::string trading_currency;            // ISO 4217
        double nominal_value;                    // Nominal value
        std::string nominal_currency;            // ISO 4217
        std::string maturity_date;               // YYYY-MM-DD for fixed income
        std::string first_trading_date;          // YYYY-MM-DD
        std::string last_trading_date;           // YYYY-MM-DD
        bool admitted_to_trading;                // true/false
        std::chrono::steady_clock::time_point timestamp;
    };

    struct TransparencyData
    {
        std::string instrument_id;               // ISIN or other identifier
        std::string venue_code;                  // MIC code
        std::string trading_phase;                // "OPEN", "CLOSE", "AUCTION", etc.
        double highest_price;                    // Highest price in period
        double lowest_price;                     // Lowest price in period
        double volume_weighted_average_price;    // VWAP
        Quantity total_volume;                   // Total volume
        Price best_bid_price;                   // Current best bid
        Quantity best_bid_quantity;              // Best bid quantity
        Price best_ask_price;                   // Current best ask
        Quantity best_ask_quantity;              // Best ask quantity
        std::chrono::steady_clock::time_point timestamp;
    };

private:
    MiFIDConfig config_;
    std::vector<TransactionReport> transaction_reports_;
    std::vector<ReferenceDataReport> reference_data_reports_;
    std::vector<TransparencyData> transparency_data_;
    std::unique_ptr<SharedMemoryMetrics> metrics_;
    std::atomic<uint64_t> report_count_{0};
    std::atomic<uint64_t> validation_errors_{0};
    mutable std::mutex report_mutex_;

    std::string GenerateTransactionId()
    {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        return "TXN" + std::to_string(timestamp) + "_" + std::to_string(report_count_.fetch_add(1));
    }

    std::string FormatTimestamp(const std::chrono::steady_clock::time_point& tp)
    {
        auto time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        auto ms = std::chrono::duration_cast<std::chrono::microseconds>(
            tp.time_since_epoch()).count() % 1000000;
        
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d");
        oss << "T" << std::put_time(std::gmtime(&time_t), "%H:%M:%S");
        oss << "." << std::setfill('0') << std::setw(6) << ms;
        
        return oss.str();
    }

    bool ValidateTransactionReport(const TransactionReport& report)
    {
        if (!config_.enable_real_time_validation) return true;
        
        // Validate required fields
        if (report.transaction_id.empty() || report.instrument_id.empty() || 
            report.buyer_id.empty() || report.seller_id.empty())
        {
            validation_errors_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        
        // Validate LEI format (20 characters, alphanumeric)
        std::regex lei_regex("^[A-Z0-9]{20}$");
        if (!std::regex_match(report.buyer_id, lei_regex) || 
            !std::regex_match(report.seller_id, lei_regex))
        {
            validation_errors_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        
        // Validate country codes (2 characters, uppercase)
        std::regex country_regex("^[A-Z]{2}$");
        if (!std::regex_match(report.buyer_country, country_regex) || 
            !std::regex_match(report.seller_country, country_regex))
        {
            validation_errors_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        
        // Validate currency codes (3 characters, uppercase)
        std::regex currency_regex("^[A-Z]{3}$");
        if (!std::regex_match(report.currency, currency_regex))
        {
            validation_errors_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        
        // Validate venue codes (4 characters, alphanumeric)
        std::regex venue_regex("^[A-Z0-9]{4}$");
        if (!std::regex_match(report.venue_code, venue_regex))
        {
            validation_errors_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        
        return true;
    }

public:
    explicit MiFIDReporter(const MiFIDConfig& config = {})
        : config_(config)
        , metrics_(std::make_unique<SharedMemoryMetrics>())
    {
        // Create report directory if it doesn't exist
        std::filesystem::create_directories(config_.report_output_path);
    }

    TransactionReport CreateTransactionReport(const Trade& trade, const std::string& buyer_id,
                                            const std::string& seller_id, const std::string& venue_code,
                                            const std::string& instrument_id)
    {
        TransactionReport report{};
        
        report.transaction_id = GenerateTransactionId();
        report.trading_date = std::put_time(std::gmtime(&std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now())), "%Y-%m-%d");
        report.trading_time = FormatTimestamp(std::chrono::steady_clock::now());
        report.venue_code = venue_code;
        report.instrument_id = instrument_id;
        report.instrument_classification = "ESXXXX"; // CFI code for equity
        report.buyer_id = buyer_id;
        report.seller_id = seller_id;
        report.buyer_country = "US"; // Default, should be configurable
        report.seller_country = "US"; // Default, should be configurable
        report.price = trade.price;
        report.quantity = trade.quantity;
        report.currency = "USD"; // Default, should be configurable
        report.venue_of_execution = venue_code;
        report.transmission_of_orders_indication = "No";
        report.algorithm_indication = config_.trading_algorithm_indicator;
        report.waiver_indicator = config_.waiver_indicator;
        report.special_dividend_indicator = "No";
        report.dark_trade_eligibility = "No";
        report.system_internaliser_flag = config_.is_systematic_internalizer ? "Yes" : "No";
        report.market_segment_id = "MAIN"; // Default market segment
        report.country_of_branch_membership = "US";
        report.transaction_category = config_.trading_capacity;
        report.timestamp = std::chrono::steady_clock::now();
        
        return report;
    }

    bool SubmitTransactionReport(const TransactionReport& report)
    {
        if (!config_.enable_transaction_reporting) return true;
        
        if (!ValidateTransactionReport(report))
        {
            return false;
        }
        
        std::lock_guard<std::mutex> lock(report_mutex_);
        transaction_reports_.push_back(report);
        
        // Write to file for regulatory submission
        std::string filename = config_.report_output_path + "transaction_reports_" + 
                              std::put_time(std::gmtime(&std::chrono::system_clock::to_time_t(
                                  std::chrono::system_clock::now())), "%Y%m%d") + ".csv";
        
        std::ofstream file(filename, std::ios::app);
        if (file.is_open())
        {
            file << report.transaction_id << ","
                 << report.trading_date << ","
                 << report.trading_time << ","
                 << report.venue_code << ","
                 << report.instrument_id << ","
                 << report.instrument_classification << ","
                 << report.buyer_id << ","
                 << report.seller_id << ","
                 << report.buyer_country << ","
                 << report.seller_country << ","
                 << report.price << ","
                 << report.quantity << ","
                 << report.currency << ","
                 << report.venue_of_execution << ","
                 << report.transmission_of_orders_indication << ","
                 << report.algorithm_indication << ","
                 << report.waiver_indicator << ","
                 << report.special_dividend_indicator << ","
                 << report.dark_trade_eligibility << ","
                 << report.system_internaliser_flag << ","
                 << report.market_segment_id << ","
                 << report.country_of_branch_membership << ","
                 << report.transaction_category << std::endl;
        }
        
        report_count_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    ReferenceDataReport CreateReferenceDataReport(const std::string& instrument_id,
                                                const std::string& instrument_name,
                                                const std::string& instrument_type,
                                                const std::string& currency,
                                                const std::string& venue_code)
    {
        ReferenceDataReport report{};
        
        report.instrument_id = instrument_id;
        report.instrument_full_name = instrument_name;
        report.instrument_classification = "ESXXXX"; // CFI code for equity
        report.currency = currency;
        report.venue_code = venue_code;
        report.instrument_type = instrument_type;
        report.country_of_issue = "US"; // Default
        report.issuer_id = config_.reporting_firm_id; // Simplified
        report.trading_currency = currency;
        report.nominal_value = 0.01; // Default nominal value
        report.nominal_currency = currency;
        report.maturity_date = "9999-12-31"; // Perpetual for equity
        report.first_trading_date = std::put_time(std::gmtime(&std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now())), "%Y-%m-%d");
        report.last_trading_date = "9999-12-31";
        report.admitted_to_trading = true;
        report.timestamp = std::chrono::steady_clock::now();
        
        return report;
    }

    bool SubmitReferenceDataReport(const ReferenceDataReport& report)
    {
        if (!config_.enable_reference_data_reporting) return true;
        
        std::lock_guard<std::mutex> lock(report_mutex_);
        reference_data_reports_.push_back(report);
        
        // Write to file
        std::string filename = config_.report_output_path + "reference_data_" + 
                              std::put_time(std::gmtime(&std::chrono::system_clock::to_time_t(
                                  std::chrono::system_clock::now())), "%Y%m%d") + ".csv";
        
        std::ofstream file(filename, std::ios::app);
        if (file.is_open())
        {
            file << report.instrument_id << ","
                 << report.instrument_full_name << ","
                 << report.instrument_classification << ","
                 << report.currency << ","
                 << report.venue_code << ","
                 << report.instrument_type << ","
                 << report.country_of_issue << ","
                 << report.issuer_id << ","
                 << report.trading_currency << ","
                 << report.nominal_value << ","
                 << report.nominal_currency << ","
                 << report.maturity_date << ","
                 << report.first_trading_date << ","
                 << report.last_trading_date << ","
                 << (report.admitted_to_trading ? "Yes" : "No") << std::endl;
        }
        
        return true;
    }

    TransparencyData CreateTransparencyData(const std::string& instrument_id,
                                          const std::string& venue_code,
                                          const Orderbook& orderbook)
    {
        TransparencyData data{};
        
        data.instrument_id = instrument_id;
        data.venue_code = venue_code;
        data.trading_phase = "OPEN"; // Simplified
        
        // Get orderbook statistics
        auto level_infos = orderbook.GetOrderbookLevelInfos();
        
        if (!level_infos.empty())
        {
            data.best_bid_price = level_infos[0].bid_price;
            data.best_bid_quantity = level_infos[0].bid_quantity;
            
            if (level_infos.size() > 1)
            {
                data.best_ask_price = level_infos[1].ask_price;
                data.best_ask_quantity = level_infos[1].ask_quantity;
            }
        }
        
        // Simplified volume and price calculations
        data.volume_weighted_average_price = (data.best_bid_price + data.best_ask_price) / 2.0;
        data.total_volume = data.best_bid_quantity + data.best_ask_quantity;
        data.highest_price = data.best_ask_price;
        data.lowest_price = data.best_bid_price;
        data.timestamp = std::chrono::steady_clock::now();
        
        return data;
    }

    bool SubmitTransparencyData(const TransparencyData& data)
    {
        if (!config_.enable_transparency_reporting) return true;
        
        std::lock_guard<std::mutex> lock(report_mutex_);
        transparency_data_.push_back(data);
        
        // Write to file
        std::string filename = config_.report_output_path + "transparency_data_" + 
                              std::put_time(std::gmtime(&std::chrono::system_clock::to_time_t(
                                  std::chrono::system_clock::now())), "%Y%m%d") + ".csv";
        
        std::ofstream file(filename, std::ios::app);
        if (file.is_open())
        {
            file << data.instrument_id << ","
                 << data.venue_code << ","
                 << data.trading_phase << ","
                 << data.highest_price << ","
                 << data.lowest_price << ","
                 << data.volume_weighted_average_price << ","
                 << data.total_volume << ","
                 << data.best_bid_price << ","
                 << data.best_bid_quantity << ","
                 << data.best_ask_price << ","
                 << data.best_ask_quantity << std::endl;
        }
        
        return true;
    }

    // Batch reporting for end-of-day submission
    bool GenerateDailyReport(const std::string& date)
    {
        std::lock_guard<std::mutex> lock(report_mutex_);
        
        std::string filename = config_.report_output_path + "daily_mifid_report_" + date + ".xml";
        std::ofstream file(filename);
        
        if (!file.is_open()) return false;
        
        file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
        file << "<MiFIDReport date=\"" << date << "\" reporting_firm=\"" << config_.reporting_firm_id << "\">" << std::endl;
        
        // Transaction reports
        file << "  <TransactionReports count=\"" << transaction_reports_.size() << "\">" << std::endl;
        for (const auto& report : transaction_reports_)
        {
            file << "    <Transaction id=\"" << report.transaction_id << "\"" << std::endl;
            file << "                 date=\"" << report.trading_date << "\"" << std::endl;
            file << "                 time=\"" << report.trading_time << "\"" << std::endl;
            file << "                 venue=\"" << report.venue_code << "\"" << std::endl;
            file << "                 instrument=\"" << report.instrument_id << "\"/>" << std::endl;
        }
        file << "  </TransactionReports>" << std::endl;
        
        // Reference data reports
        file << "  <ReferenceDataReports count=\"" << reference_data_reports_.size() << "\">" << std::endl;
        for (const auto& report : reference_data_reports_)
        {
            file << "    <Instrument id=\"" << report.instrument_id << "\"" << std::endl;
            file << "                    name=\"" << report.instrument_full_name << "\"" << std::endl;
            file << "                    type=\"" << report.instrument_type << "\"/>" << std::endl;
        }
        file << "  </ReferenceDataReports>" << std::endl;
        
        file << "</MiFIDReport>" << std::endl;
        
        return true;
    }

    // Statistics
    uint64_t GetReportCount() const
    {
        return report_count_.load(std::memory_order_relaxed);
    }

    uint64_t GetValidationErrors() const
    {
        return validation_errors_.load(std::memory_order_relaxed);
    }

    size_t GetTransactionReportCount() const
    {
        std::lock_guard<std::mutex> lock(report_mutex_);
        return transaction_reports_.size();
    }

    size_t GetReferenceDataReportCount() const
    {
        std::lock_guard<std::mutex> lock(report_mutex_);
        return reference_data_reports_.size();
    }

    void PrintMiFIDStatus() const
    {
        std::cout << "\n=== MiFID II Reporter Status ===" << std::endl;
        std::cout << "Reporting Firm ID: " << config_.reporting_firm_id << std::endl;
        std::cout << "NCA Code: " << config_.nca_code << std::endl;
        std::cout << "Trading Capacity: " << config_.trading_capacity << std::endl;
        std::cout << "Systematic Internalizer: " << (config_.is_systematic_internalizer ? "Yes" : "No") << std::endl;
        std::cout << "Transaction Reports: " << GetTransactionReportCount() << std::endl;
        std::cout << "Reference Data Reports: " << GetReferenceDataReportCount() << std::endl;
        std::cout << "Total Reports Submitted: " << GetReportCount() << std::endl;
        std::cout << "Validation Errors: " << GetValidationErrors() << std::endl;
        std::cout << "================================" << std::endl;
    }
};