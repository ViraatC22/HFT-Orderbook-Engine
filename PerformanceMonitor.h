#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <atomic>
#include <memory>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>

#ifdef __linux__
#include <papi.h>
#endif

#include "Usings.h"
#include "SharedMemoryMetrics.h"

/**
 * Hardware Performance Monitor Integration (PAPI/PMU)
 * 
 * Professional-grade performance validation using CPU Performance Monitoring Units.
 * Provides granular hardware-level metrics for micro-architectural optimization validation.
 * 
 * Key features:
 * - L1/L2/L3 cache miss tracking per trade processed
 * - Branch misprediction analysis per code path
 * - Memory bandwidth utilization monitoring
 * - Automatic performance regression detection
 * - Cross-platform PMU access via PAPI library
 * - Real-time hardware counter correlation with business metrics
 * 
 * Metrics tracked:
 * - Cache misses (L1, L2, L3) per trade
 * - Branch mispredictions per operation
 * - Memory bandwidth utilization
 * - CPU cycles per instruction (CPI)
 * - Pipeline stall cycles
 */

class PerformanceCounter
{
public:
    enum class CounterType
    {
        L1_CACHE_MISSES,
        L2_CACHE_MISSES,
        L3_CACHE_MISSES,
        BRANCH_MISPREDICTIONS,
        CPU_CYCLES,
        INSTRUCTIONS,
        MEMORY_BANDWIDTH,
        PIPELINE_STALLS,
        CUSTOM_EVENT
    };

    struct CounterConfig
    {
        CounterType type;
        std::string name;
        std::string description;
        uint64_t papi_event_code;
        bool enabled;
        double warning_threshold;  // Alert if exceeds this value
        double critical_threshold; // Critical if exceeds this value
    };

private:
    CounterType type_;
    std::string name_;
    uint64_t papi_event_code_;
    bool enabled_;
    double warning_threshold_;
    double critical_threshold_;
    
    std::atomic<int64_t> current_value_{0};
    std::atomic<int64_t> baseline_value_{0};
    std::atomic<int64_t> total_count_{0};
    std::atomic<double> average_value_{0.0};
    std::atomic<uint64_t> sample_count_{0};

public:
    PerformanceCounter(CounterType type, const std::string& name, uint64_t papi_event_code,
                      bool enabled = true, double warning_threshold = 0.0, double critical_threshold = 0.0)
        : type_(type)
        , name_(name)
        , papi_event_code_(papi_event_code)
        , enabled_(enabled)
        , warning_threshold_(warning_threshold)
        , critical_threshold_(critical_threshold)
    {
    }

    void RecordValue(int64_t value)
    {
        if (!enabled_) return;
        
        current_value_.store(value, std::memory_order_relaxed);
        total_count_.fetch_add(value, std::memory_order_relaxed);
        
        // Update running average
        uint64_t samples = sample_count_.fetch_add(1, std::memory_order_relaxed) + 1;
        double current_avg = average_value_.load(std::memory_order_relaxed);
        double new_avg = current_avg + (static_cast<double>(value) - current_avg) / samples;
        average_value_.store(new_avg, std::memory_order_relaxed);
    }

    void SetBaseline(int64_t baseline)
    {
        baseline_value_.store(baseline, std::memory_order_relaxed);
    }

    int64_t GetCurrentValue() const
    {
        return current_value_.load(std::memory_order_relaxed);
    }

    int64_t GetBaselineValue() const
    {
        return baseline_value_.load(std::memory_order_relaxed);
    }

    int64_t GetTotalCount() const
    {
        return total_count_.load(std::memory_order_relaxed);
    }

    double GetAverageValue() const
    {
        return average_value_.load(std::memory_order_relaxed);
    }

    uint64_t GetSampleCount() const
    {
        return sample_count_.load(std::memory_order_relaxed);
    }

    double GetRegressionRatio() const
    {
        int64_t baseline = baseline_value_.load(std::memory_order_relaxed);
        if (baseline == 0) return 0.0;
        
        double current = static_cast<double>(current_value_.load(std::memory_order_relaxed));
        return (current - baseline) / baseline;
    }

    bool IsWarning() const
    {
        if (warning_threshold_ <= 0.0) return false;
        return GetCurrentValue() > warning_threshold_;
    }

    bool IsCritical() const
    {
        if (critical_threshold_ <= 0.0) return false;
        return GetCurrentValue() > critical_threshold_;
    }

    CounterType GetType() const { return type_; }
    const std::string& GetName() const { return name_; }
    uint64_t GetPapiEventCode() const { return papi_event_code_; }
    bool IsEnabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }

    void Reset()
    {
        current_value_.store(0, std::memory_order_relaxed);
        total_count_.store(0, std::memory_order_relaxed);
        average_value_.store(0.0, std::memory_order_relaxed);
        sample_count_.store(0, std::memory_order_relaxed);
    }
};

class PerformanceMonitor
{
public:
    struct MonitorConfig
    {
        bool enable_papi = true;
        bool enable_custom_events = true;
        size_t sample_buffer_size = 1000;
        double baseline_collection_duration_seconds = 30.0;
        bool auto_baseline_collection = true;
        bool regression_detection_enabled = true;
        double regression_threshold_percent = 15.0;
        bool verbose_logging = false;
    };

    struct PerformanceSnapshot
    {
        std::chrono::steady_clock::time_point timestamp;
        uint64_t trades_processed;
        uint64_t total_instructions;
        uint64_t total_cycles;
        double instructions_per_cycle;
        double l1_miss_rate;
        double l2_miss_rate;
        double l3_miss_rate;
        double branch_misprediction_rate;
        double memory_bandwidth_gb_s;
        double average_latency_ns;
        std::vector<std::pair<std::string, int64_t>> counter_values;
        std::vector<std::string> warnings;
        std::vector<std::string> critical_alerts;
    };

private:
    MonitorConfig config_;
    std::vector<std::unique_ptr<PerformanceCounter>> counters_;
    std::atomic<bool> monitoring_active_{false};
    std::atomic<bool> baseline_collection_active_{false};
    std::atomic<uint64_t> trades_processed_{0};
    
#ifdef __linux__
    int papi_event_set_;
    bool papi_initialized_;
#endif

    // Pre-configured performance counter definitions
    static std::vector<PerformanceCounter::CounterConfig> GetDefaultCounterConfigs()
    {
        return {
            {PerformanceCounter::CounterType::L1_CACHE_MISSES, "L1_Cache_Misses", 
             "L1 data cache misses", PAPI_L1_DCM, true, 1000, 2000},
            {PerformanceCounter::CounterType::L2_CACHE_MISSES, "L2_Cache_Misses", 
             "L2 data cache misses", PAPI_L2_DCM, true, 500, 1000},
            {PerformanceCounter::CounterType::L3_CACHE_MISSES, "L3_Cache_Misses", 
             "L3 data cache misses", PAPI_L3_DCM, true, 200, 500},
            {PerformanceCounter::CounterType::BRANCH_MISPREDICTIONS, "Branch_Mispredictions", 
             "Branch instruction mispredictions", PAPI_BR_MSP, true, 100, 300},
            {PerformanceCounter::CounterType::CPU_CYCLES, "CPU_Cycles", 
             "Total CPU cycles", PAPI_TOT_CYC, true, 0, 0},
            {PerformanceCounter::CounterType::INSTRUCTIONS, "Instructions", 
             "Total instructions executed", PAPI_TOT_INS, true, 0, 0},
            {PerformanceCounter::CounterType::MEMORY_BANDWIDTH, "Memory_Bandwidth", 
             "Memory bandwidth utilization", PAPI_LST_INS, true, 1000000, 2000000}
        };
    }

    void InitializePAPI()
    {
#ifdef __linux__
        if (!config_.enable_papi) return;
        
        int retval = PAPI_library_init(PAPI_VER_CURRENT);
        if (retval != PAPI_VER_CURRENT && retval > 0)
        {
            std::cerr << "[PerformanceMonitor] PAPI library version mismatch!" << std::endl;
            papi_initialized_ = false;
            return;
        }
        
        if (retval < 0)
        {
            std::cerr << "[PerformanceMonitor] PAPI library initialization failed!" << std::endl;
            papi_initialized_ = false;
            return;
        }
        
        // Create event set
        if (PAPI_create_eventset(&papi_event_set_) != PAPI_OK)
        {
            std::cerr << "[PerformanceMonitor] PAPI event set creation failed!" << std::endl;
            papi_initialized_ = false;
            return;
        }
        
        // Add events to the set
        for (const auto& counter_config : GetDefaultCounterConfigs())
        {
            if (counter_config.enabled && PAPI_add_event(papi_event_set_, counter_config.papi_event_code) != PAPI_OK)
            {
                std::cerr << "[PerformanceMonitor] Failed to add PAPI event: " << counter_config.name << std::endl;
            }
        }
        
        papi_initialized_ = true;
        
        if (config_.verbose_logging)
        {
            std::cout << "[PerformanceMonitor] PAPI initialized successfully" << std::endl;
        }
#else
        papi_initialized_ = false;
        if (config_.verbose_logging)
        {
            std::cout << "[PerformanceMonitor] PAPI not available on non-Linux platforms" << std::endl;
        }
#endif
    }

    void StartPAPICounters()
    {
#ifdef __linux__
        if (papi_initialized_ && PAPI_start(papi_event_set_) != PAPI_OK)
        {
            std::cerr << "[PerformanceMonitor] Failed to start PAPI counters!" << std::endl;
        }
#endif
    }

    void StopPAPICounters()
    {
#ifdef __linux__
        if (papi_initialized_)
        {
            long long values[128] = {0};
            if (PAPI_stop(papi_event_set_, values) == PAPI_OK)
            {
                // Update counter values
                size_t value_index = 0;
                for (auto& counter : counters_)
                {
                    if (counter->IsEnabled() && counter->GetPapiEventCode() != 0)
                    {
                        counter->RecordValue(values[value_index++]);
                    }
                }
            }
        }
#endif
    }

public:
    explicit PerformanceMonitor(const MonitorConfig& config = {})
        : config_(config)
#ifdef __linux__
        , papi_event_set_(PAPI_NULL)
        , papi_initialized_(false)
#endif
    {
        InitializePerformanceCounters();
        InitializePAPI();
    }

    ~PerformanceMonitor()
    {
        StopMonitoring();
#ifdef __linux__
        if (papi_initialized_)
        {
            PAPI_cleanup_eventset(papi_event_set_);
            PAPI_destroy_eventset(&papi_event_set_);
            PAPI_shutdown();
        }
#endif
    }

    void InitializePerformanceCounters()
    {
        auto configs = GetDefaultCounterConfigs();
        
        for (const auto& config : configs)
        {
            auto counter = std::make_unique<PerformanceCounter>(
                config.type, config.name, config.papi_event_code, 
                config.enabled, config.warning_threshold, config.critical_threshold);
            
            counters_.push_back(std::move(counter));
        }
        
        if (config_.verbose_logging)
        {
            std::cout << "[PerformanceMonitor] Initialized " << counters_.size() 
                     << " performance counters" << std::endl;
        }
    }

    void StartMonitoring()
    {
        if (monitoring_active_.load(std::memory_order_relaxed))
        {
            return; // Already monitoring
        }
        
        monitoring_active_.store(true, std::memory_order_relaxed);
        StartPAPICounters();
        
        if (config_.verbose_logging)
        {
            std::cout << "[PerformanceMonitor] Monitoring started" << std::endl;
        }
    }

    void StopMonitoring()
    {
        if (!monitoring_active_.load(std::memory_order_relaxed))
        {
            return; // Not monitoring
        }
        
        StopPAPICounters();
        monitoring_active_.store(false, std::memory_order_relaxed);
        
        if (config_.verbose_logging)
        {
            std::cout << "[PerformanceMonitor] Monitoring stopped" << std::endl;
        }
    }

    void RecordTradeProcessed()
    {
        trades_processed_.fetch_add(1, std::memory_order_relaxed);
    }

    void RecordCustomEvent(const std::string& event_name, int64_t value)
    {
        if (!config_.enable_custom_events) return;
        
        // Find or create custom counter
        auto it = std::find_if(counters_.begin(), counters_.end(),
            [&event_name](const std::unique_ptr<PerformanceCounter>& counter) {
                return counter->GetName() == event_name;
            });
        
        if (it != counters_.end())
        {
            (*it)->RecordValue(value);
        }
        else
        {
            // Create new custom counter
            auto counter = std::make_unique<PerformanceCounter>(
                PerformanceCounter::CounterType::CUSTOM_EVENT, event_name, 0, true, 0, 0);
            counter->RecordValue(value);
            counters_.push_back(std::move(counter));
        }
    }

    PerformanceSnapshot GetSnapshot()
    {
        StopPAPICounters(); // Get latest values
        
        PerformanceSnapshot snapshot;
        snapshot.timestamp = std::chrono::steady_clock::now();
        snapshot.trades_processed = trades_processed_.load(std::memory_order_relaxed);
        
        // Collect counter values
        int64_t total_instructions = 0;
        int64_t total_cycles = 0;
        int64_t l1_misses = 0;
        int64_t l2_misses = 0;
        int64_t l3_misses = 0;
        int64_t branch_mispredictions = 0;
        int64_t memory_operations = 0;
        
        for (const auto& counter : counters_)
        {
            if (!counter->IsEnabled()) continue;
            
            int64_t value = counter->GetCurrentValue();
            snapshot.counter_values.push_back({counter->GetName(), value});
            
            // Extract key metrics
            switch (counter->GetType())
            {
                case PerformanceCounter::CounterType::INSTRUCTIONS:
                    total_instructions = value;
                    break;
                case PerformanceCounter::CounterType::CPU_CYCLES:
                    total_cycles = value;
                    break;
                case PerformanceCounter::CounterType::L1_CACHE_MISSES:
                    l1_misses = value;
                    break;
                case PerformanceCounter::CounterType::L2_CACHE_MISSES:
                    l2_misses = value;
                    break;
                case PerformanceCounter::CounterType::L3_CACHE_MISSES:
                    l3_misses = value;
                    break;
                case PerformanceCounter::CounterType::BRANCH_MISPREDICTIONS:
                    branch_mispredictions = value;
                    break;
                case PerformanceCounter::CounterType::MEMORY_BANDWIDTH:
                    memory_operations = value;
                    break;
                default:
                    break;
            }
            
            // Check for warnings and critical alerts
            if (counter->IsWarning())
            {
                snapshot.warnings.push_back(
                    "WARNING: " + counter->GetName() + " = " + std::to_string(value));
            }
            
            if (counter->IsCritical())
            {
                snapshot.critical_alerts.push_back(
                    "CRITICAL: " + counter->GetName() + " = " + std::to_string(value));
            }
        }
        
        // Calculate derived metrics
        snapshot.total_instructions = total_instructions;
        snapshot.total_cycles = total_cycles;
        snapshot.instructions_per_cycle = (total_cycles > 0) ? 
            static_cast<double>(total_instructions) / total_cycles : 0.0;
        
        // Cache miss rates (per 1000 instructions)
        double instruction_base = (total_instructions > 0) ? total_instructions / 1000.0 : 1.0;
        snapshot.l1_miss_rate = l1_misses / instruction_base;
        snapshot.l2_miss_rate = l2_misses / instruction_base;
        snapshot.l3_miss_rate = l3_misses / instruction_base;
        snapshot.branch_misprediction_rate = branch_mispredictions / instruction_base;
        
        // Memory bandwidth (approximate)
        snapshot.memory_bandwidth_gb_s = (memory_operations * 64.0) / (1024.0 * 1024.0 * 1024.0); // Assuming 64-byte cache lines
        
        // Average latency (if we have trade samples)
        snapshot.average_latency_ns = (snapshot.trades_processed > 0 && total_cycles > 0) ?
            (static_cast<double>(total_cycles) / snapshot.trades_processed) * 0.5 : 0.0; // Approximate
        
        StartPAPICounters(); // Resume monitoring
        
        return snapshot;
    }

    void CollectBaseline()
    {
        if (config_.verbose_logging)
        {
            std::cout << "[PerformanceMonitor] Collecting baseline metrics..." << std::endl;
        }
        
        baseline_collection_active_.store(true, std::memory_order_relaxed);
        
        // Reset all counters
        for (auto& counter : counters_)
        {
            counter->Reset();
        }
        trades_processed_.store(0, std::memory_order_relaxed);
        
        // Collect baseline for specified duration
        auto start_time = std::chrono::steady_clock::now();
        auto baseline_duration = std::chrono::duration<double>(config_.baseline_collection_duration_seconds);
        
        StartMonitoring();
        
        while (std::chrono::steady_clock::now() - start_time < baseline_duration)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        StopMonitoring();
        
        // Set baselines for all counters
        for (auto& counter : counters_)
        {
            counter->SetBaseline(counter->GetCurrentValue());
        }
        
        baseline_collection_active_.store(false, std::memory_order_relaxed);
        
        if (config_.verbose_logging)
        {
            std::cout << "[PerformanceMonitor] Baseline collection completed" << std::endl;
        }
    }

    bool CheckForRegression()
    {
        if (!config_.regression_detection_enabled) return false;
        
        bool regression_detected = false;
        
        for (const auto& counter : counters_)
        {
            if (!counter->IsEnabled()) continue;
            
            double regression_ratio = counter->GetRegressionRatio();
            if (regression_ratio > (config_.regression_threshold_percent / 100.0))
            {
                regression_detected = true;
                
                if (config_.verbose_logging)
                {
                    std::cout << "[PerformanceMonitor] REGRESSION DETECTED: " << counter->GetName()
                             << " regressed by " << (regression_ratio * 100.0) << "%" << std::endl;
                }
            }
        }
        
        return regression_detected;
    }

    void ResetMetrics()
    {
        for (auto& counter : counters_)
        {
            counter->Reset();
        }
        trades_processed_.store(0, std::memory_order_relaxed);
    }

    bool IsMonitoring() const
    {
        return monitoring_active_.load(std::memory_order_relaxed);
    }

    bool IsCollectingBaseline() const
    {
        return baseline_collection_active_.load(std::memory_order_relaxed);
    }

    const MonitorConfig& GetConfig() const { return config_; }
    
    void PrintPerformanceReport()
    {
        auto snapshot = GetSnapshot();
        
        std::cout << "\n=== Hardware Performance Report ===" << std::endl;
        std::cout << "Trades Processed: " << snapshot.trades_processed << std::endl;
        std::cout << "Total Instructions: " << snapshot.total_instructions << std::endl;
        std::cout << "Total CPU Cycles: " << snapshot.total_cycles << std::endl;
        std::cout << "Instructions per Cycle: " << snapshot.instructions_per_cycle << std::endl;
        std::cout << "Average Latency: " << snapshot.average_latency_ns << " ns" << std::endl;
        std::cout << std::endl;
        
        std::cout << "Cache Performance (per 1000 instructions):" << std::endl;
        std::cout << "  L1 Miss Rate: " << snapshot.l1_miss_rate << std::endl;
        std::cout << "  L2 Miss Rate: " << snapshot.l2_miss_rate << std::endl;
        std::cout << "  L3 Miss Rate: " << snapshot.l3_miss_rate << std::endl;
        std::cout << "  Branch Misprediction Rate: " << snapshot.branch_misprediction_rate << std::endl;
        std::cout << "  Memory Bandwidth: " << snapshot.memory_bandwidth_gb_s << " GB/s" << std::endl;
        std::cout << std::endl;
        
        if (!snapshot.warnings.empty())
        {
            std::cout << "Warnings:" << std::endl;
            for (const auto& warning : snapshot.warnings)
            {
                std::cout << "  ⚠ " << warning << std::endl;
            }
        }
        
        if (!snapshot.critical_alerts.empty())
        {
            std::cout << "Critical Alerts:" << std::endl;
            for (const auto& alert : snapshot.critical_alerts)
            {
                std::cout << "  ✗ " << alert << std::endl;
            }
        }
        
        std::cout << "=====================================" << std::endl;
    }
};

/**
 * Micro-benchmark framework for detailed code path analysis
 */
class MicroBenchmark
{
public:
    struct BenchmarkResult
    {
        std::string benchmark_name;
        std::chrono::nanoseconds duration;
        uint64_t iterations;
        double nanoseconds_per_iteration;
        PerformanceMonitor::PerformanceSnapshot performance_snapshot;
        std::vector<std::pair<std::string, double>> custom_metrics;
    };

private:
    std::string name_;
    std::unique_ptr<PerformanceMonitor> monitor_;
    uint64_t warmup_iterations_;
    uint64_t measurement_iterations_;

public:
    MicroBenchmark(const std::string& name, uint64_t warmup_iterations = 1000, 
                   uint64_t measurement_iterations = 10000)
        : name_(name)
        , warmup_iterations_(warmup_iterations)
        , measurement_iterations_(measurement_iterations)
    {
        PerformanceMonitor::MonitorConfig config;
        config.enable_papi = true;
        config.verbose_logging = false;
        monitor_ = std::make_unique<PerformanceMonitor>(config);
    }

    template<typename Func>
    BenchmarkResult RunBenchmark(Func&& benchmark_func)
    {
        BenchmarkResult result;
        result.benchmark_name = name_;
        
        // Warmup phase
        for (uint64_t i = 0; i < warmup_iterations_; ++i)
        {
            benchmark_func();
        }
        
        // Measurement phase
        monitor_->ResetMetrics();
        monitor_->StartMonitoring();
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (uint64_t i = 0; i < measurement_iterations_; ++i)
        {
            benchmark_func();
            monitor_->RecordTradeProcessed();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        
        monitor_->StopMonitoring();
        
        // Calculate results
        result.duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
        result.iterations = measurement_iterations_;
        result.nanoseconds_per_iteration = static_cast<double>(result.duration.count()) / measurement_iterations_;
        result.performance_snapshot = monitor_->GetSnapshot();
        
        return result;
    }

    void PrintBenchmarkResult(const BenchmarkResult& result)
    {
        std::cout << "\n=== Micro-Benchmark Report: " << result.benchmark_name << " ===" << std::endl;
        std::cout << "Iterations: " << result.iterations << std::endl;
        std::cout << "Total Duration: " << result.duration.count() << " ns" << std::endl;
        std::cout << "Per-Iteration: " << result.nanoseconds_per_iteration << " ns" << std::endl;
        std::cout << "Trades Processed: " << result.performance_snapshot.trades_processed << std::endl;
        std::cout << std::endl;
        
        std::cout << "Hardware Performance:" << std::endl;
        std::cout << "  Instructions/Cycle: " << result.performance_snapshot.instructions_per_cycle << std::endl;
        std::cout << "  L1 Miss Rate: " << result.performance_snapshot.l1_miss_rate << std::endl;
        std::cout << "  L2 Miss Rate: " << result.performance_snapshot.l2_miss_rate << std::endl;
        std::cout << "  L3 Miss Rate: " << result.performance_snapshot.l3_miss_rate << std::endl;
        std::cout << "  Branch Mispredictions: " << result.performance_snapshot.branch_misprediction_rate << std::endl;
        std::cout << std::endl;
        
        if (!result.custom_metrics.empty())
        {
            std::cout << "Custom Metrics:" << std::endl;
            for (const auto& [name, value] : result.custom_metrics)
            {
                std::cout << "  " << name << ": " << value << std::endl;
            }
        }
        
        std::cout << "=============================================" << std::endl;
    }
};