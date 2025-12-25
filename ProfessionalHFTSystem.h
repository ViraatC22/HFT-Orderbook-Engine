#pragma once

#include "MarketDataSimulator.h"
#include "PerformanceMonitor.h"
#include "VenueManager.h"
#include "FixEngine.h"
#include "MiFIDReporter.h"
#include "CATReporter.h"
#include "ProductionOrderbook.h"

/**
 * Professional HFT Production Integration
 * 
 * This header provides a unified interface for all professional-grade components:
 * 
 * 1. Market Data Simulation Framework (Digital Twin)
 *    - MarketDataSimulator with chaos injection (latency spikes, packet loss, sequence gaps)
 *    - Real-world validation using events.log replay with configurable stress testing
 *    - Performance regression detection against baseline metrics
 * 
 * 2. Hardware Performance Monitoring (PAPI/PMU)
 *    - PerformanceMonitor with CPU cache miss/branch misprediction tracking
 *    - MicroBenchmark framework for code path analysis
 *    - Hardware counter correlation with business metrics
 * 
 * 3. Multi-Asset/Cross-Venue Architecture
 *    - VenueManager coordinating multiple independent orderbooks
 *    - Template-based MultiAssetOrderbook<AssetType> for asset-specific logic
 *    - Cross-venue risk aggregation and position management
 *    - Symbol mapping for consistent instrument identification
 * 
 * 4. FIX Engine & Regulatory Compliance
 *    - FixEngine implementing FIX 4.2/4.4 protocol for exchange connectivity
 *    - MiFIDReporter for European regulatory reporting (RTS 6 compliance)
 *    - CATReporter for US Consolidated Audit Trail reporting (SEC Rule 613)
 *    - Real-time trade capture and audit trail integration
 * 
 * Usage Example:
 * ```cpp
 * // Initialize professional components
 * auto simulator = std::make_unique<MarketDataSimulator>();
 * auto perf_monitor = std::make_unique<PerformanceMonitor>();
 * auto venue_manager = std::make_unique<VenueManager>();
 * auto fix_engine = std::make_unique<FixEngine>();
 * auto mifid_reporter = std::make_unique<MiFIDReporter>();
 * auto cat_reporter = std::make_unique<CATReporter>();
 * 
 * // Configure chaos injection
 * MarketDataSimulator::SimulatorConfig sim_config;
 * sim_config.enable_chaos_injection = true;
 * sim_config.chaos_config.packet_loss_rate = 0.0001; // 0.01%
 * sim_config.chaos_config.latency_spike_probability = 0.001; // 0.1%
 * 
 * // Start simulation with chaos
 * simulator->StartSimulation();
 * 
 * // Monitor hardware performance
 * perf_monitor->StartMonitoring();
 * 
 * // Process orders through multi-venue architecture
 * venue_manager->SubmitCrossVenueOrder("SPY", order, {"NYSE", "NASDAQ"});
 * 
 * // Report trades for regulatory compliance
 * auto transaction_report = mifid_reporter->CreateTransactionReport(trade, buyer_id, seller_id, "XNYS", "US0378331005");
 * mifid_reporter->SubmitTransactionReport(transaction_report);
 * 
 * // Generate daily regulatory reports
 * mifid_reporter->GenerateDailyReport("2025-12-24");
 * cat_reporter->GenerateDailyReport("2025-12-24");
 * ```
 * 
 * Professional Features:
 * - Zero-allocation design maintained across all components
 * - Lock-free data structures for ultra-low latency
 * - Cache-line aligned structures for optimal CPU performance
 * - Comprehensive error handling and validation
 * - Real-time monitoring and alerting
 * - Regulatory compliance out-of-the-box
 * - Multi-jurisdiction support (US/EU)
 * - Production-grade logging and audit trails
 */

// Professional HFT System Configuration
struct ProfessionalHFTConfig
{
    // Market Data Simulation
    bool enable_market_data_simulation = true;
    MarketDataSimulator::SimulatorConfig simulator_config;
    
    // Performance Monitoring
    bool enable_performance_monitoring = true;
    PerformanceMonitor::MonitorConfig perf_monitor_config;
    
    // Multi-Venue Architecture
    bool enable_multi_venue_trading = true;
    std::vector<VenueConfig> venue_configs;
    
    // FIX Engine
    bool enable_fix_engine = true;
    FixEngine::EngineConfig fix_engine_config;
    
    // Regulatory Reporting
    bool enable_mifid_reporting = true;
    MiFIDReporter::MiFIDConfig mifid_config;
    
    bool enable_cat_reporting = true;
    CATReporter::CATConfig cat_config;
    
    // Production Features
    bool enable_chaos_testing = true;
    bool enable_hardware_counters = true;
    bool enable_regulatory_compliance = true;
    bool enable_cross_venue_arbitrage = false;
    bool enable_real_time_monitoring = true;
    
    std::string system_name = "ProfessionalHFT";
    std::string system_version = "1.0.0";
    std::string log_directory = "logs/";
    std::string report_directory = "reports/";
};

class ProfessionalHFTSystem
{
private:
    ProfessionalHFTConfig config_;
    std::unique_ptr<MarketDataSimulator> simulator_;
    std::unique_ptr<PerformanceMonitor> perf_monitor_;
    std::unique_ptr<VenueManager> venue_manager_;
    std::unique_ptr<FixEngine> fix_engine_;
    std::unique_ptr<MiFIDReporter> mifid_reporter_;
    std::unique_ptr<CATReporter> cat_reporter_;
    std::unique_ptr<ProductionOrderbook> primary_orderbook_;
    
    std::atomic<bool> system_active_{false};
    std::chrono::steady_clock::time_point system_start_time_;

public:
    explicit ProfessionalHFTSystem(const ProfessionalHFTConfig& config = {})
        : config_(config)
        , system_start_time_(std::chrono::steady_clock::now())
    {
        InitializeSystem();
    }

    void InitializeSystem()
    {
        // Create directories
        std::filesystem::create_directories(config_.log_directory);
        std::filesystem::create_directories(config_.report_directory);
        
        // Initialize market data simulator
        if (config_.enable_market_data_simulation)
        {
            simulator_ = std::make_unique<MarketDataSimulator>(config_.simulator_config);
        }
        
        // Initialize performance monitor
        if (config_.enable_performance_monitoring)
        {
            perf_monitor_ = std::make_unique<PerformanceMonitor>(config_.perf_monitor_config);
        }
        
        // Initialize venue manager
        if (config_.enable_multi_venue_trading)
        {
            venue_manager_ = std::make_unique<VenueManager>();
            
            // Register configured venues
            for (const auto& venue_config : config_.venue_configs)
            {
                venue_manager_->RegisterVenue(venue_config);
            }
        }
        
        // Initialize FIX engine
        if (config_.enable_fix_engine)
        {
            fix_engine_ = std::make_unique<FixEngine>(config_.fix_engine_config);
            fix_engine_->Initialize();
        }
        
        // Initialize regulatory reporters
        if (config_.enable_regulatory_compliance)
        {
            if (config_.enable_mifid_reporting)
            {
                mifid_reporter_ = std::make_unique<MiFIDReporter>(config_.mifid_config);
            }
            
            if (config_.enable_cat_reporting)
            {
                cat_reporter_ = std::make_unique<CATReporter>(config_.cat_config);
            }
        }
        
        // Initialize primary orderbook
        primary_orderbook_ = std::make_unique<ProductionOrderbook>();
        
        system_active_.store(true, std::memory_order_relaxed);
        
        std::cout << "[ProfessionalHFTSystem] System initialized successfully" << std::endl;
        std::cout << "[ProfessionalHFTSystem] System: " << config_.system_name 
                  << " v" << config_.system_version << std::endl;
    }

    void StartSystem()
    {
        if (!system_active_.load(std::memory_order_relaxed))
        {
            std::cerr << "[ProfessionalHFTSystem] System not initialized" << std::endl;
            return;
        }
        
        // Start market data simulation
        if (simulator_)
        {
            simulator_->StartSimulation();
            std::cout << "[ProfessionalHFTSystem] Market data simulation started" << std::endl;
        }
        
        // Start performance monitoring
        if (perf_monitor_)
        {
            perf_monitor_->StartMonitoring();
            std::cout << "[ProfessionalHFTSystem] Performance monitoring started" << std::endl;
        }
        
        // Start FIX sessions
        if (fix_engine_)
        {
            // Create default FIX sessions
            FixEngine::EngineConfig fix_config;
            fix_engine_->CreateSession("NYSE", FixSession::SessionConfig{"HFT_ENGINE", "NYSE"});
            fix_engine_->CreateSession("NASDAQ", FixSession::SessionConfig{"HFT_ENGINE", "NASDAQ"});
            std::cout << "[ProfessionalHFTSystem] FIX engine started" << std::endl;
        }
        
        std::cout << "[ProfessionalHFTSystem] System started successfully" << std::endl;
    }

    void StopSystem()
    {
        std::cout << "[ProfessionalHFTSystem] Stopping system..." << std::endl;
        
        // Stop market data simulation
        if (simulator_)
        {
            simulator_->StopSimulation();
        }
        
        // Stop performance monitoring
        if (perf_monitor_)
        {
            perf_monitor_->StopMonitoring();
        }
        
        // Shutdown FIX engine
        if (fix_engine_)
        {
            fix_engine_->Shutdown();
        }
        
        system_active_.store(false, std::memory_order_relaxed);
        
        std::cout << "[ProfessionalHFTSystem] System stopped" << std::endl;
    }

    // Market Data Simulation Interface
    MarketDataSimulator* GetMarketDataSimulator() { return simulator_.get(); }
    
    // Performance Monitoring Interface
    PerformanceMonitor* GetPerformanceMonitor() { return perf_monitor_.get(); }
    
    // Multi-Venue Management Interface
    VenueManager* GetVenueManager() { return venue_manager_.get(); }
    
    // FIX Engine Interface
    FixEngine* GetFixEngine() { return fix_engine_.get(); }
    
    // Regulatory Reporting Interface
    MiFIDReporter* GetMiFIDReporter() { return mifid_reporter_.get(); }
    CATReporter* GetCATReporter() { return cat_reporter_.get(); }
    
    // Primary Orderbook Interface
    ProductionOrderbook* GetPrimaryOrderbook() { return primary_orderbook_.get(); }

    // System Status
    bool IsSystemActive() const { return system_active_.load(std::memory_order_relaxed); }
    
    std::chrono::seconds GetSystemUptime() const
    {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - system_start_time_);
    }

    void PrintSystemStatus()
    {
        std::cout << "\n=== Professional HFT System Status ===" << std::endl;
        std::cout << "System: " << config_.system_name << " v" << config_.system_version << std::endl;
        std::cout << "Status: " << (IsSystemActive() ? "ACTIVE" : "INACTIVE") << std::endl;
        std::cout << "Uptime: " << GetSystemUptime().count() << " seconds" << std::endl;
        
        std::cout << "\nComponents:" << std::endl;
        std::cout << "  Market Data Simulation: " << (simulator_ ? "ENABLED" : "DISABLED") << std::endl;
        std::cout << "  Performance Monitoring: " << (perf_monitor_ ? "ENABLED" : "DISABLED") << std::endl;
        std::cout << "  Multi-Venue Trading: " << (venue_manager_ ? "ENABLED" : "DISABLED") << std::endl;
        std::cout << "  FIX Engine: " << (fix_engine_ ? "ENABLED" : "DISABLED") << std::endl;
        std::cout << "  MiFID Reporting: " << (mifid_reporter_ ? "ENABLED" : "DISABLED") << std::endl;
        std::cout << "  CAT Reporting: " << (cat_reporter_ ? "ENABLED" : "DISABLED") << std::endl;
        
        if (simulator_)
        {
            std::cout << "\nMarket Data Simulation:" << std::endl;
            std::cout << "  Status: " << (simulator_->IsRunning() ? "RUNNING" : "STOPPED") << std::endl;
            std::cout << "  Events: " << simulator_->GetEventCount() << std::endl;
        }
        
        if (perf_monitor_)
        {
            std::cout << "\nPerformance Monitoring:" << std::endl;
            std::cout << "  Status: " << (perf_monitor_->IsMonitoring() ? "MONITORING" : "STOPPED") << std::endl;
        }
        
        if (venue_manager_)
        {
            std::cout << "\nMulti-Venue Trading:" << std::endl;
            std::cout << "  Venues: " << venue_manager_->GetVenueCount() << std::endl;
            std::cout << "  Orderbooks: " << venue_manager_->GetOrderbookCount() << std::endl;
            std::cout << "  Total Orders: " << venue_manager_->GetTotalOrdersProcessed() << std::endl;
            std::cout << "  Total Trades: " << venue_manager_->GetTotalTradesExecuted() << std::endl;
        }
        
        std::cout << "=======================================" << std::endl;
    }

    void GenerateDailyReports(const std::string& date)
    {
        std::cout << "[ProfessionalHFTSystem] Generating daily reports for " << date << std::endl;
        
        if (mifid_reporter_)
        {
            mifid_reporter_->GenerateDailyReport(date);
            std::cout << "[ProfessionalHFTSystem] MiFID daily report generated" << std::endl;
        }
        
        if (cat_reporter_)
        {
            cat_reporter_->GenerateDailyReport(date);
            std::cout << "[ProfessionalHFTSystem] CAT daily report generated" << std::endl;
        }
        
        // Generate performance report
        if (perf_monitor_)
        {
            perf_monitor_->PrintPerformanceReport();
            std::cout << "[ProfessionalHFTSystem] Performance report generated" << std::endl;
        }
    }
};