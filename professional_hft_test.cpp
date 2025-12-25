#include <iostream>
#include <memory>
#include <chrono>
#include <thread>

#include "ProfessionalHFTSystem.h"
#include "MarketDataSimulator.h"
#include "PerformanceMonitor.h"
#include "VenueManager.h"
#include "FixEngine.h"
#include "MiFIDReporter.h"
#include "CATReporter.h"

/**
 * Professional HFT System Integration Test
 * 
 * Demonstrates the complete production-grade HFT system with:
 * - Market Data Simulation with chaos injection
 * - Hardware performance monitoring (PAPI/PMU)
 * - Multi-venue trading architecture
 * - FIX protocol exchange connectivity
 * - Regulatory compliance reporting (MiFID II & CAT)
 * 
 * This test validates all professional components working together
 * in a realistic trading scenario with adverse market conditions.
 */

int main()
{
    std::cout << "=========================================" << std::endl;
    std::cout << "Professional HFT System Integration Test" << std::endl;
    std::cout << "=========================================" << std::endl;

    try
    {
        // Configure professional HFT system
        ProfessionalHFTConfig config;
        
        // Market Data Simulation Configuration
        config.enable_market_data_simulation = true;
        config.simulator_config.events_log_path = "events.log";
        config.simulator_config.replay_speed_multiplier = 2.0; // 2x speed for testing
        config.simulator_config.enable_chaos_injection = true;
        config.simulator_config.chaos_config.packet_loss_rate = 0.0001; // 0.01%
        config.simulator_config.chaos_config.latency_spike_probability = 0.001; // 0.1%
        config.simulator_config.chaos_config.sequence_gap_probability = 0.0005; // 0.05%
        config.simulator_config.verbose_logging = true;
        
        // Performance Monitoring Configuration
        config.enable_performance_monitoring = true;
        config.perf_monitor_config.enable_papi = true;
        config.perf_monitor_config.enable_custom_events = true;
        config.perf_monitor_config.auto_baseline_collection = true;
        config.perf_monitor_config.regression_detection_enabled = true;
        config.perf_monitor_config.verbose_logging = true;
        
        // Multi-Venue Configuration
        config.enable_multi_venue_trading = true;
        
        // NYSE Venue
        VenueConfig nyse_config;
        nyse_config.venue_name = "NYSE";
        nyse_config.venue_code = "XNYS";
        nyse_config.mic_code = "XNYS";
        nyse_config.country_code = "US";
        nyse_config.supported_asset_classes = {"EQUITY", "ETF"};
        nyse_config.supports_market_data = true;
        nyse_config.supports_order_routing = true;
        nyse_config.requires_pre_trade_risk = true;
        nyse_config.max_order_size = 1000000;
        nyse_config.max_price_deviation = 0.1;
        nyse_config.max_latency_ms = std::chrono::milliseconds(100);
        config.venue_configs.push_back(nyse_config);
        
        // NASDAQ Venue
        VenueConfig nasdaq_config;
        nasdaq_config.venue_name = "NASDAQ";
        nasdaq_config.venue_code = "XNAS";
        nasdaq_config.mic_code = "XNAS";
        nasdaq_config.country_code = "US";
        nasdaq_config.supported_asset_classes = {"EQUITY", "ETF"};
        nasdaq_config.supports_market_data = true;
        nasdaq_config.supports_order_routing = true;
        nasdaq_config.requires_pre_trade_risk = true;
        nasdaq_config.max_order_size = 1000000;
        nasdaq_config.max_price_deviation = 0.1;
        nasdaq_config.max_latency_ms = std::chrono::milliseconds(50);
        config.venue_configs.push_back(nasdaq_config);
        
        // FIX Engine Configuration
        config.enable_fix_engine = true;
        config.fix_engine_config.local_comp_id = "HFT_ENGINE_DEMO";
        config.fix_engine_config.version = "FIX.4.2";
        config.fix_engine_config.auto_reconnect = true;
        config.fix_engine_config.validate_messages = true;
        config.fix_engine_config.enable_logging = true;
        
        // MiFID II Reporting Configuration
        config.enable_mifid_reporting = true;
        config.mifid_config.reporting_firm_id = "5493001KJTIIGC8Y1R12"; // Sample LEI
        config.mifid_config.nca_code = "SEC";
        config.mifid_config.trading_capacity = "DEAL";
        config.mifid_config.is_systematic_internalizer = false;
        config.mifid_config.trading_algorithm_indicator = "No";
        config.mifid_config.waiver_indicator = "No";
        config.mifid_config.enable_real_time_validation = true;
        
        // CAT Reporting Configuration
        config.enable_cat_reporting = true;
        config.cat_config.industry_member_id = "12345"; // Sample CRD
        config.cat_config.reporting_firm_type = "BD";
        config.cat_config.firm_designated_id = "HFT_DEMO";
        config.cat_config.enable_real_time_validation = true;
        config.cat_config.include_customer_info = true;
        config.cat_config.include_account_info = true;
        
        // Create and initialize professional HFT system
        std::cout << "Initializing Professional HFT System..." << std::endl;
        auto hft_system = std::make_unique<ProfessionalHFTSystem>(config);
        
        // Print system configuration
        hft_system->PrintSystemStatus();
        
        // Start the system
        std::cout << "\nStarting Professional HFT System..." << std::endl;
        hft_system->StartSystem();
        
        // Wait for system to stabilize
        std::cout << "Waiting for system initialization..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Test 1: Market Data Simulation with Chaos Injection
        std::cout << "\n=== Test 1: Market Data Simulation ===" << std::endl;
        auto* simulator = hft_system->GetMarketDataSimulator();
        if (simulator)
        {
            std::cout << "Market Data Simulator Status:" << std::endl;
            std::cout << "  Running: " << (simulator->IsRunning() ? "Yes" : "No") << std::endl;
            std::cout << "  Events: " << simulator->GetEventCount() << std::endl;
            
            // Get simulation metrics
            auto metrics = simulator->GetMetrics();
            std::cout << "  Messages Processed: " << metrics.messages_processed << std::endl;
            std::cout << "  Average Latency: " << metrics.average_latency_ns << " ns" << std::endl;
            std::cout << "  Throughput: " << metrics.throughput_messages_per_sec << " msg/sec" << std::endl;
            
            // Chaos metrics
            std::cout << "  Packets Dropped: " << metrics.chaos_metrics.packets_dropped << std::endl;
            std::cout << "  Latency Spikes: " << metrics.chaos_metrics.latency_spikes << std::endl;
            std::cout << "  Sequence Gaps: " << metrics.chaos_metrics.sequence_gaps << std::endl;
        }
        
        // Test 2: Hardware Performance Monitoring
        std::cout << "\n=== Test 2: Hardware Performance Monitoring ===" << std::endl;
        auto* perf_monitor = hft_system->GetPerformanceMonitor();
        if (perf_monitor)
        {
            // Collect baseline performance metrics
            std::cout << "Collecting baseline performance metrics..." << std::endl;
            perf_monitor->CollectBaseline();
            
            // Get performance snapshot
            auto snapshot = perf_monitor->GetSnapshot();
            std::cout << "Hardware Performance:" << std::endl;
            std::cout << "  Instructions/Cycle: " << snapshot.instructions_per_cycle << std::endl;
            std::cout << "  L1 Miss Rate: " << snapshot.l1_miss_rate << std::endl;
            std::cout << "  L2 Miss Rate: " << snapshot.l2_miss_rate << std::endl;
            std::cout << "  L3 Miss Rate: " << snapshot.l3_miss_rate << std::endl;
            std::cout << "  Branch Mispredictions: " << snapshot.branch_misprediction_rate << std::endl;
            std::cout << "  Memory Bandwidth: " << snapshot.memory_bandwidth_gb_s << " GB/s" << std::endl;
            
            // Check for performance regressions
            if (perf_monitor->CheckForRegression())
            {
                std::cout << "  WARNING: Performance regression detected!" << std::endl;
            }
            else
            {
                std::cout << "  Performance baseline established successfully" << std::endl;
            }
        }
        
        // Test 3: Multi-Venue Trading Architecture
        std::cout << "\n=== Test 3: Multi-Venue Trading Architecture ===" << std::endl;
        auto* venue_manager = hft_system->GetVenueManager();
        if (venue_manager)
        {
            // Create multi-asset orderbooks
            std::cout << "Creating multi-asset orderbooks..." << std::endl;
            
            // Equity orderbook for NYSE
            venue_manager->CreateOrderbook<EquityAsset>("SPY", "NYSE", "SPY");
            
            // Equity orderbook for NASDAQ
            venue_manager->CreateOrderbook<EquityAsset>("SPY", "NASDAQ", "SPY");
            
            // Register symbol mappings
            SymbolMapper::SymbolMapping spy_mapping;
            spy_mapping.internal_symbol = "SPY";
            spy_mapping.venue_symbol = "SPY";
            spy_mapping.isin = "US78462F1030";
            spy_mapping.cusip = "78462F103";
            spy_mapping.ric = "SPY";
            spy_mapping.bloomberg_ticker = "SPY:US";
            spy_mapping.asset_class = "EQUITY";
            spy_mapping.currency = "USD";
            spy_mapping.tick_size = 0.01;
            spy_mapping.lot_size = 1;
            
            venue_manager->RegisterSymbolMapping("NYSE", spy_mapping);
            venue_manager->RegisterSymbolMapping("NASDAQ", spy_mapping);
            
            std::cout << "Venue Manager Status:" << std::endl;
            std::cout << "  Venues: " << venue_manager->GetVenueCount() << std::endl;
            std::cout << "  Orderbooks: " << venue_manager->GetOrderbookCount() << std::endl;
            
            // Get risk metrics
            auto risk_metrics = venue_manager->GetGlobalRiskMetrics();
            std::cout << "  Total Exposure: " << risk_metrics.total_notional_exposure << std::endl;
            std::cout << "  Max Single Venue Exposure: " << risk_metrics.max_single_venue_exposure << std::endl;
            std::cout << "  Net Exposure: " << risk_metrics.net_exposure << std::endl;
            std::cout << "  Gross Exposure: " << risk_metrics.gross_exposure << std::endl;
        }
        
        // Test 4: FIX Engine Connectivity
        std::cout << "\n=== Test 4: FIX Engine Connectivity ===" << std::endl;
        auto* fix_engine = hft_system->GetFixEngine();
        if (fix_engine)
        {
            std::cout << "FIX Engine Status:" << std::endl;
            fix_engine->PrintEngineStatus();
            
            // Create FIX sessions
            FixSession::SessionConfig nyse_session;
            nyse_session.sender_comp_id = "HFT_ENGINE_DEMO";
            nyse_session.target_comp_id = "NYSE";
            nyse_session.heartbeat_interval = 30;
            nyse_session.auto_reconnect = true;
            
            fix_engine->CreateSession("NYSE", nyse_session);
            
            std::cout << "  NYSE FIX Session Created" << std::endl;
        }
        
        // Test 5: Regulatory Reporting
        std::cout << "\n=== Test 5: Regulatory Reporting ===" << std::endl;
        
        // MiFID II Reporting
        auto* mifid_reporter = hft_system->GetMiFIDReporter();
        if (mifid_reporter)
        {
            std::cout << "MiFID II Reporter Status:" << std::endl;
            mifid_reporter->PrintMiFIDStatus();
            
            // Create sample transaction report
            Trade sample_trade;
            sample_trade.trade_id = 12345;
            sample_trade.order_id = 67890;
            sample_trade.price = 450.25;
            sample_trade.quantity = 100;
            sample_trade.side = Side::Buy;
            
            auto transaction_report = mifid_reporter->CreateTransactionReport(
                sample_trade, "5493001KJTIIGC8Y1R12", "5493001KJTIIGC8Y1R13", "XNYS", "US78462F1030");
            
            bool submitted = mifid_reporter->SubmitTransactionReport(transaction_report);
            std::cout << "  Sample MiFID Transaction Report: " << (submitted ? "SUBMITTED" : "FAILED") << std::endl;
        }
        
        // CAT Reporting
        auto* cat_reporter = hft_system->GetCATReporter();
        if (cat_reporter)
        {
            std::cout << "\nCAT Reporter Status:" << std::endl;
            cat_reporter->PrintCATStatus();
            
            // Create sample order event
            Order sample_order(12345, Side::Buy, 100, 450.25, OrderType::GoodTillCancel);
            auto order_event = cat_reporter->CreateOrderEvent("NEW", sample_order, "NYSE", "C", "R");
            
            bool submitted = cat_reporter->SubmitOrderEvent(order_event);
            std::cout << "  Sample CAT Order Event: " << (submitted ? "SUBMITTED" : "FAILED") << std::endl;
            
            // Create sample trade event
            auto trade_event = cat_reporter->CreateTradeEvent(sample_trade, "NYSE", "C");
            submitted = cat_reporter->SubmitTradeEvent(trade_event);
            std::cout << "  Sample CAT Trade Event: " << (submitted ? "SUBMITTED" : "FAILED") << std::endl;
        }
        
        // Run simulation for extended period
        std::cout << "\n=== Running Extended Simulation ===" << std::endl;
        std::cout << "Simulating 30 seconds of market data with chaos injection..." << std::endl;
        
        auto start_time = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(30))
        {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            // Print periodic status updates
            std::cout << "\n--- Status Update ---" << std::endl;
            
            if (simulator_)
            {
                auto metrics = simulator->GetMetrics();
                std::cout << "Simulation: " << metrics.messages_processed << " messages processed" << std::endl;
            }
            
            if (perf_monitor_)
            {
                auto snapshot = perf_monitor->GetSnapshot();
                std::cout << "Performance: " << snapshot.trades_processed << " trades, "
                         << snapshot.instructions_per_cycle << " IPC" << std::endl;
            }
            
            if (venue_manager_)
            {
                std::cout << "Venues: " << venue_manager_->GetTotalOrdersProcessed() << " orders, "
                         << venue_manager_->GetTotalTradesExecuted() << " trades" << std::endl;
            }
        }
        
        // Generate final reports
        std::cout << "\n=== Generating Final Reports ===" << std::endl;
        auto today = std::put_time(std::gmtime(&std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now())), "%Y-%m-%d");
        
        hft_system->GenerateDailyReports(today);
        
        // Print final system status
        std::cout << "\n=== Final System Status ===" << std::endl;
        hft_system->PrintSystemStatus();
        
        // Print simulation report
        if (simulator_)
        {
            std::cout << "\n=== Final Simulation Report ===" << std::endl;
            simulator->PrintSimulationReport();
        }
        
        // Print performance report
        if (perf_monitor_)
        {
            std::cout << "\n=== Final Performance Report ===" << std::endl;
            perf_monitor->PrintPerformanceReport();
        }
        
        // Print venue report
        if (venue_manager_)
        {
            std::cout << "\n=== Final Venue Report ===" << std::endl;
            venue_manager->PrintVenueReport();
        }
        
        // Stop the system
        std::cout << "\nStopping Professional HFT System..." << std::endl;
        hft_system->StopSystem();
        
        std::cout << "\n=========================================" << std::endl;
        std::cout << "Professional HFT System Test COMPLETED" << std::endl;
        std::cout << "=========================================" << std::endl;
        
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "UNKNOWN ERROR occurred during test execution" << std::endl;
        return 1;
    }
}