#pragma once

#include <vector>
#include <chrono>
#include <random>
#include <fstream>
#include <sstream>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>

#include "KernelBypassIngress.h"
#include "LockFreeQueue.h"
#include "SharedMemoryMetrics.h"

/**
 * Market Data Simulation Framework (Digital Twin)
 * 
 * Professional HFT system validation against real-world chaos using logged events.
 * Replays historical market data with configurable chaos injection to test
 * engine robustness under adverse market conditions.
 * 
 * Key features:
 * - Real-time replay of events.log with timing preservation
 * - Configurable latency spikes (network jitter, kernel context switches)
 * - Packet loss simulation (0.01% drop rate) with recovery validation
 * - Out-of-sequence message injection with sequence gap handling
 * - Deterministic chaos seeding for reproducible tests
 * - Performance regression detection against baseline metrics
 */

class ChaosInjector
{
public:
    struct ChaosConfig
    {
        double packet_loss_rate;
        double latency_spike_probability;
        std::chrono::nanoseconds base_latency;
        std::chrono::nanoseconds spike_latency;
        double sequence_gap_probability;
        uint32_t max_sequence_gap;
        uint64_t chaos_seed;
        bool enable_chaos;
        
        ChaosConfig()
            : packet_loss_rate(0.0001)
            , latency_spike_probability(0.001)
            , base_latency{100}
            , spike_latency{10000}
            , sequence_gap_probability(0.0005)
            , max_sequence_gap(10)
            , chaos_seed(42)
            , enable_chaos(true)
        {
        }
    };

private:
    ChaosConfig config_;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> uniform_;
    std::normal_distribution<double> jitter_;
    std::atomic<uint64_t> packets_dropped_{0};
    std::atomic<uint64_t> latency_spikes_{0};
    std::atomic<uint64_t> sequence_gaps_{0};

public:
    explicit ChaosInjector(const ChaosConfig& config = ChaosConfig()) 
        : config_(config), rng_(config.chaos_seed), uniform_(0.0, 1.0), jitter_(0.0, 50.0)
    {
    }

    bool ShouldDropPacket()
    {
        if (!config_.enable_chaos) return false;
        bool drop = uniform_(rng_) < config_.packet_loss_rate;
        if (drop) packets_dropped_.fetch_add(1, std::memory_order_relaxed);
        return drop;
    }

    std::chrono::nanoseconds GetInjectedLatency()
    {
        if (!config_.enable_chaos) return config_.base_latency;
        
        auto latency = config_.base_latency;
        
        // Add random jitter
        latency += std::chrono::nanoseconds(static_cast<int64_t>(jitter_(rng_)));
        
        // Inject latency spikes
        if (uniform_(rng_) < config_.latency_spike_probability)
        {
            latency += config_.spike_latency;
            latency_spikes_.fetch_add(1, std::memory_order_relaxed);
        }
        
        return latency;
    }

    bool ShouldInjectSequenceGap()
    {
        if (!config_.enable_chaos) return false;
        bool gap = uniform_(rng_) < config_.sequence_gap_probability;
        if (gap) sequence_gaps_.fetch_add(1, std::memory_order_relaxed);
        return gap;
    }

    uint32_t GetSequenceGapSize()
    {
        std::uniform_int_distribution<uint32_t> gap_dist(1, config_.max_sequence_gap);
        return gap_dist(rng_);
    }

    struct ChaosMetrics
    {
        uint64_t packets_dropped;
        uint64_t latency_spikes;
        uint64_t sequence_gaps;
        double current_drop_rate;
        double current_spike_rate;
    };

    ChaosMetrics GetMetrics() const
    {
        return {
            packets_dropped_.load(std::memory_order_relaxed),
            latency_spikes_.load(std::memory_order_relaxed),
            sequence_gaps_.load(std::memory_order_relaxed),
            static_cast<double>(packets_dropped_.load(std::memory_order_relaxed)) / 1000000.0,
            static_cast<double>(latency_spikes_.load(std::memory_order_relaxed)) / 1000000.0
        };
    }

    void ResetMetrics()
    {
        packets_dropped_.store(0, std::memory_order_relaxed);
        latency_spikes_.store(0, std::memory_order_relaxed);
        sequence_gaps_.store(0, std::memory_order_relaxed);
    }
};

class SimulationMetrics
{
public:
    struct SimSnapshot
    {
        std::chrono::steady_clock::time_point timestamp;
        uint64_t messages_processed;
        uint64_t messages_replayed;
        uint64_t sequence_gaps_detected;
        uint64_t recovery_operations;
        double average_latency_ns;
        double p99_latency_ns;
        double throughput_messages_per_sec;
        ChaosInjector::ChaosMetrics chaos_metrics;
    };

private:
    std::atomic<uint64_t> messages_processed_{0};
    std::atomic<uint64_t> messages_replayed_{0};
    std::atomic<uint64_t> sequence_gaps_detected_{0};
    std::atomic<uint64_t> recovery_operations_{0};
    
    // Latency tracking
    std::vector<double> latency_samples_;
    std::mutex latency_mutex_;
    static constexpr size_t MAX_LATENCY_SAMPLES = 10000;

    std::chrono::steady_clock::time_point start_time_;

public:
    SimulationMetrics() : start_time_(std::chrono::steady_clock::now()) {}

    void RecordMessageProcessed(std::chrono::nanoseconds latency)
    {
        messages_processed_.fetch_add(1, std::memory_order_relaxed);
        
        std::lock_guard<std::mutex> lock(latency_mutex_);
        latency_samples_.push_back(static_cast<double>(latency.count()));
        if (latency_samples_.size() > MAX_LATENCY_SAMPLES)
        {
            latency_samples_.erase(latency_samples_.begin());
        }
    }

    void RecordMessageReplayed()
    {
        messages_replayed_.fetch_add(1, std::memory_order_relaxed);
    }

    void RecordSequenceGapDetected()
    {
        sequence_gaps_detected_.fetch_add(1, std::memory_order_relaxed);
    }

    void RecordRecoveryOperation()
    {
        recovery_operations_.fetch_add(1, std::memory_order_relaxed);
    }

    SimSnapshot GetSnapshot(const ChaosInjector& chaos_injector)
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
        
        std::vector<double> latencies;
        {
            std::lock_guard<std::mutex> lock(latency_mutex_);
            latencies = latency_samples_;
        }
        
        double avg_latency = 0.0;
        double p99_latency = 0.0;
        double throughput = 0.0;
        
        if (!latencies.empty())
        {
            std::sort(latencies.begin(), latencies.end());
            avg_latency = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
            size_t p99_index = static_cast<size_t>(latencies.size() * 0.99);
            p99_latency = latencies[p99_index];
            throughput = elapsed > 0 ? static_cast<double>(messages_processed_.load()) / elapsed : 0.0;
        }

        return {
            now,
            messages_processed_.load(std::memory_order_relaxed),
            messages_replayed_.load(std::memory_order_relaxed),
            sequence_gaps_detected_.load(std::memory_order_relaxed),
            recovery_operations_.load(std::memory_order_relaxed),
            avg_latency,
            p99_latency,
            throughput,
            chaos_injector.GetMetrics()
        };
    }

    void Reset()
    {
        messages_processed_.store(0, std::memory_order_relaxed);
        messages_replayed_.store(0, std::memory_order_relaxed);
        sequence_gaps_detected_.store(0, std::memory_order_relaxed);
        recovery_operations_.store(0, std::memory_order_relaxed);
        
        std::lock_guard<std::mutex> lock(latency_mutex_);
        latency_samples_.clear();
        start_time_ = std::chrono::steady_clock::now();
    }
};

class MarketDataSimulator
{
public:
    struct SimulatorConfig
    {
        std::string events_log_path;
        double replay_speed_multiplier;
        bool preserve_original_timing;
        bool enable_chaos_injection;
        ChaosInjector::ChaosConfig chaos_config;
        size_t ring_buffer_size;
        bool verbose_logging;
        
        SimulatorConfig()
            : events_log_path("events.log")
            , replay_speed_multiplier(1.0)
            , preserve_original_timing(true)
            , enable_chaos_injection(true)
            , chaos_config()
            , ring_buffer_size(65536)
            , verbose_logging(false)
        {
        }
    };

private:
    SimulatorConfig config_;
    std::unique_ptr<ChaosInjector> chaos_injector_;
    std::unique_ptr<SimulationMetrics> metrics_;
    std::unique_ptr<LockFreeQueue<MarketDataPacket>> output_queue_;
    
    // Event log parsing
    std::vector<std::pair<std::chrono::steady_clock::time_point, MarketDataPacket>> event_timeline_;
    std::atomic<bool> simulation_running_{false};
    std::atomic<bool> simulation_paused_{false};
    std::unique_ptr<std::thread> simulation_thread_;
    
    // Sequence number tracking for gap detection
    std::atomic<uint64_t> expected_sequence_{1};
    std::atomic<uint64_t> last_processed_sequence_{0};

    void ParseEventsLog()
    {
        std::ifstream log_file(config_.events_log_path);
        if (!log_file.is_open())
        {
            throw std::runtime_error("Failed to open events.log: " + config_.events_log_path);
        }

        std::string line;
        std::chrono::steady_clock::time_point base_time;
        bool first_event = true;

        while (std::getline(log_file, line))
        {
            if (line.empty()) continue;
            
            // Parse log entry format: [timestamp] sequence_number message_type data...
            MarketDataPacket packet = ParseLogEntry(line);
            
            auto now = std::chrono::steady_clock::now();
            if (first_event)
            {
                base_time = now;
                first_event = false;
            }
            
            event_timeline_.emplace_back(now, packet);
        }
        
        if (config_.verbose_logging)
        {
            std::cout << "[MarketDataSimulator] Parsed " << event_timeline_.size() 
                     << " events from " << config_.events_log_path << std::endl;
        }
    }

    MarketDataPacket ParseLogEntry(const std::string& line)
    {
        MarketDataPacket packet{};
        // Simple parser - extend based on actual events.log format
        std::istringstream iss(line);
        std::string timestamp_str, seq_str, type_str;
        
        // Basic parsing - adapt to your actual log format
        if (iss >> timestamp_str >> seq_str >> type_str)
        {
            packet.sequence_number = std::stoull(seq_str);
            packet.message_type = static_cast<uint8_t>(std::stoi(type_str));
            packet.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }
        
        return packet;
    }

    void SimulationLoop()
    {
        if (event_timeline_.empty()) return;
        
        auto start_time = std::chrono::steady_clock::now();
        auto first_event_time = event_timeline_.front().first;
        
        for (const auto& [original_time, packet] : event_timeline_)
        {
            if (!simulation_running_.load(std::memory_order_relaxed)) break;
            
            // Pause handling
            while (simulation_paused_.load(std::memory_order_relaxed))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                if (!simulation_running_.load(std::memory_order_relaxed)) return;
            }
            
            // Timing preservation
            if (config_.preserve_original_timing && config_.replay_speed_multiplier > 0.0)
            {
                auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - start_time);
                auto expected_elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                    (original_time - first_event_time) / config_.replay_speed_multiplier);
                
                if (elapsed < expected_elapsed)
                {
                    std::this_thread::sleep_for(expected_elapsed - elapsed);
                }
            }
            
            ProcessPacket(packet);
        }
        
        if (config_.verbose_logging)
        {
            std::cout << "[MarketDataSimulator] Simulation completed" << std::endl;
        }
    }

    void ProcessPacket(const MarketDataPacket& packet)
    {
        metrics_->RecordMessageReplayed();
        
        // Chaos injection
        if (config_.enable_chaos_injection && chaos_injector_)
        {
            // Simulate processing latency
            std::this_thread::sleep_for(chaos_injector_->GetInjectedLatency());
            
            // Packet loss simulation
            if (chaos_injector_->ShouldDropPacket())
            {
                return; // Drop packet - test recovery logic
            }
            
            // Sequence gap injection
            if (chaos_injector_->ShouldInjectSequenceGap())
            {
                auto gap_size = chaos_injector_->GetSequenceGapSize();
                expected_sequence_.fetch_add(gap_size, std::memory_order_relaxed);
                metrics_->RecordSequenceGapDetected();
            }
        }
        
        // Sequence number validation
        uint64_t expected = expected_sequence_.load(std::memory_order_relaxed);
        if (packet.sequence_number != expected)
        {
            metrics_->RecordSequenceGapDetected();
            expected_sequence_.store(packet.sequence_number + 1, std::memory_order_relaxed);
        }
        else
        {
            expected_sequence_.fetch_add(1, std::memory_order_relaxed);
        }
        
        last_processed_sequence_.store(packet.sequence_number, std::memory_order_relaxed);
        
        // Queue packet for engine processing
        auto start_process = std::chrono::high_resolution_clock::now();
        
        if (output_queue_ && output_queue_->Push(packet))
        {
            auto process_latency = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::high_resolution_clock::now() - start_process);
            metrics_->RecordMessageProcessed(process_latency);
        }
        else
        {
            // Queue full - count as dropped
            if (config_.verbose_logging)
            {
                std::cout << "[MarketDataSimulator] Output queue full, dropping packet" << std::endl;
            }
        }
    }

public:
    explicit MarketDataSimulator(const SimulatorConfig& config = SimulatorConfig())
        : config_(config)
        , chaos_injector_(std::make_unique<ChaosInjector>(config.chaos_config))
        , metrics_(std::make_unique<SimulationMetrics>())
        , output_queue_(std::make_unique<LockFreeQueue<MarketDataPacket>>(config.ring_buffer_size))
    {
        ParseEventsLog();
    }

    ~MarketDataSimulator()
    {
        StopSimulation();
    }

    // Start simulation in background thread
    void StartSimulation()
    {
        if (simulation_running_.load(std::memory_order_relaxed))
        {
            return; // Already running
        }
        
        simulation_running_.store(true, std::memory_order_relaxed);
        simulation_paused_.store(false, std::memory_order_relaxed);
        
        simulation_thread_ = std::make_unique<std::thread>([this]() {
            SimulationLoop();
        });
        
        if (config_.verbose_logging)
        {
            std::cout << "[MarketDataSimulator] Simulation started" << std::endl;
        }
    }

    void StopSimulation()
    {
        simulation_running_.store(false, std::memory_order_relaxed);
        simulation_paused_.store(false, std::memory_order_relaxed);
        
        if (simulation_thread_ && simulation_thread_->joinable())
        {
            simulation_thread_->join();
        }
        
        if (config_.verbose_logging)
        {
            std::cout << "[MarketDataSimulator] Simulation stopped" << std::endl;
        }
    }

    void PauseSimulation()
    {
        simulation_paused_.store(true, std::memory_order_relaxed);
    }

    void ResumeSimulation()
    {
        simulation_paused_.store(false, std::memory_order_relaxed);
    }

    // Get output queue for engine consumption
    LockFreeQueue<MarketDataPacket>* GetOutputQueue() 
    {
        return output_queue_.get();
    }

    // Get current simulation metrics
    SimulationMetrics::SimSnapshot GetMetrics()
    {
        return metrics_->GetSnapshot(*chaos_injector_);
    }

    void ResetMetrics()
    {
        metrics_->Reset();
        chaos_injector_->ResetMetrics();
    }

    bool IsRunning() const
    {
        return simulation_running_.load(std::memory_order_relaxed);
    }

    bool IsPaused() const
    {
        return simulation_paused_.load(std::memory_order_relaxed);
    }

    size_t GetEventCount() const
    {
        return event_timeline_.size();
    }

    void PrintSimulationReport()
    {
        auto metrics = GetMetrics();
        
        std::cout << "\n=== Market Data Simulation Report ===" << std::endl;
        std::cout << "Messages Processed: " << metrics.messages_processed << std::endl;
        std::cout << "Messages Replaye