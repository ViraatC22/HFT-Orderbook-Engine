#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <format>
#include <iostream>
#include <cerrno>

#ifdef __linux__
#include <sched.h>
#include <pthread.h>
#include <sys/resource.h>
#include <unistd.h>
#include <errno.h>
#endif

#include "Usings.h"

/**
 * OS & Hardware Tuning Validation
 * 
 * Production-grade system validation that ensures optimal configuration
 * for ultra-low latency trading systems. Validates CPU isolation,
 * memory allocation, and system tuning parameters.
 * 
 * Key features:
 * - CPU core isolation validation (isolcpus)
 * - CPU governor performance mode verification
 * - NUMA topology and memory allocation checks
 * - Real-time priority validation
 * - Memory huge pages configuration
 * - Hardware performance counter access
 */

class SystemValidator
{
public:
    struct SystemConfig
    {
        int target_cpu_core = 7;           // CPU core to isolate for engine
        bool require_cpu_isolation = true;  // Fatal error if not isolated
        bool require_performance_governor = true;
        bool require_huge_pages = false;    // Optional but recommended
        bool require_realtime_priority = true;
        bool require_numa_awareness = true;
        size_t min_huge_pages_mb = 1024;   // Minimum 1GB huge pages
        size_t max_memory_bandwidth_gb = 50; // GB/s for memory validation
        
        // Tuning recommendations
        bool auto_tune_cpu = false;         // Attempt automatic tuning
        bool auto_tune_memory = false;
        bool verbose_validation = true;
    };
    
    struct ValidationResult
    {
        bool is_valid;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
        std::vector<std::string> recommendations;
        
        void AddWarning(const std::string& msg) { warnings.push_back(msg); }
        void AddError(const std::string& msg) { errors.push_back(msg); is_valid = false; }
        void AddRecommendation(const std::string& msg) { recommendations.push_back(msg); }
    };
    
    SystemValidator() : config_{} {}
    
    explicit SystemValidator(const SystemConfig& config)
        : config_(config)
    {
    }
    
    // Main validation entry point
    ValidationResult ValidateSystem()
    {
        ValidationResult result;
        result.is_valid = true;
        
        if (config_.verbose_validation)
        {
            std::cout << "[SystemValidator] Starting system validation..." << std::endl;
        }
        
        // Validate CPU configuration
        ValidateCPUConfiguration(result);
        
        // Validate memory configuration
        ValidateMemoryConfiguration(result);
        
        // Validate real-time capabilities
        ValidateRealtimeCapabilities(result);
        
        // Validate NUMA topology
        ValidateNUMATopology(result);
        
        // Validate performance counters
        ValidatePerformanceCounters(result);
        
        // Generate recommendations
        GenerateRecommendations(result);
        
        if (config_.verbose_validation)
        {
            PrintValidationResults(result);
        }
        
        return result;
    }
    
    // Apply automatic tuning where possible
    bool ApplyAutomaticTuning()
    {
        bool success = true;
        
        if (config_.auto_tune_cpu)
        {
            success &= TuneCPU();
        }
        
        if (config_.auto_tune_memory)
        {
            success &= TuneMemory();
        }
        
        return success;
    }
    
    // Get current system information
    [[nodiscard]] std::string GetSystemInfo() const
    {
        std::ostringstream info;
        
        info << "=== System Information ===" << std::endl;
        info << "CPU Cores: " << std::thread::hardware_concurrency() << std::endl;
        info << "CPU Core " << config_.target_cpu_core << " isolated: " 
             << (IsCPUIsolated(config_.target_cpu_core) ? "YES" : "NO") << std::endl;
        info << "Performance Governor: " << GetCPUGovernor() << std::endl;
        info << "Huge Pages Available: " << GetHugePagesInfo() << std::endl;
        info << "NUMA Nodes: " << GetNUMANodes() << std::endl;
        info << "Real-time Priority: " << (HasRealtimePriority() ? "AVAILABLE" : "UNAVAILABLE") 
             << std::endl;
        
        return info.str();
    }
    
private:
    void ValidateCPUConfiguration(ValidationResult& result)
    {
        if (config_.verbose_validation)
        {
            std::cout << "[SystemValidator] Validating CPU configuration..." << std::endl;
        }
        
        // Check CPU isolation
        if (!IsCPUIsolated(config_.target_cpu_core))
        {
            if (config_.require_cpu_isolation)
            {
                result.AddError(std::format(
                    "CPU core {} is not isolated. Add 'isolcpus={}' to kernel boot parameters.",
                    config_.target_cpu_core, config_.target_cpu_core));
            }
            else
            {
                result.AddWarning(std::format(
                    "CPU core {} is not isolated. Performance may be degraded.",
                    config_.target_cpu_core));
            }
        }
        
        // Check CPU governor
        std::string governor = GetCPUGovernor(config_.target_cpu_core);
        if (governor != "performance")
        {
            if (config_.require_performance_governor)
            {
                result.AddError(std::format(
                    "CPU governor is '{}' instead of 'performance'. Set: echo performance > /sys/devices/system/cpu/cpu{}/cpufreq/scaling_governor",
                    governor, config_.target_cpu_core));
            }
            else
            {
                result.AddWarning(std::format(
                    "CPU governor is '{}' instead of 'performance'. This may cause latency jitter.",
                    governor));
            }
        }
        
        // Check for hyper-threading (should be disabled for HFT)
        if (HasHyperthreading())
        {
            result.AddWarning(
                "Hyper-threading is enabled. Consider disabling for better cache performance.");
        }
        
        // Check CPU frequency
        int freq = GetCPUFrequency(config_.target_cpu_core);
        if (freq < 2000) // Less than 2GHz
        {
            result.AddWarning(std::format(
                "CPU frequency is {} MHz, which is low for HFT workloads.", freq));
        }
    }
    
    void ValidateMemoryConfiguration(ValidationResult& result)
    {
        if (config_.verbose_validation)
        {
            std::cout << "[SystemValidator] Validating memory configuration..." << std::endl;
        }
        
        // Check huge pages
        size_t huge_pages_kb = GetHugePagesFree();
        if (huge_pages_kb < config_.min_huge_pages_mb * 1024)
        {
            if (config_.require_huge_pages)
            {
                result.AddError(std::format(
                    "Insufficient huge pages: {} MB available, {} MB required.",
                    huge_pages_kb / 1024, config_.min_huge_pages_mb));
            }
            else
            {
                result.AddWarning(std::format(
                    "Low huge pages: {} MB available. Consider increasing for better TLB performance.",
                    huge_pages_kb / 1024));
            }
        }
        
        // Check transparent huge pages (should be disabled)
        if (IsTransparentHugePagesEnabled())
        {
            result.AddWarning(
                "Transparent huge pages are enabled. Consider disabling: echo never > /sys/kernel/mm/transparent_hugepage/enabled");
        }
        
        // Check memory bandwidth
        double bandwidth = GetMemoryBandwidth();
        if (bandwidth < config_.max_memory_bandwidth_gb)
        {
            result.AddWarning(std::format(
                "Memory bandwidth is {:.1f} GB/s, which may be limiting for high-frequency workloads.",
                bandwidth));
        }
    }
    
    void ValidateRealtimeCapabilities(ValidationResult& result)
    {
        if (config_.verbose_validation)
        {
            std::cout << "[SystemValidator] Validating real-time capabilities..." << std::endl;
        }
        
        // Check if we can set real-time priority
        if (!HasRealtimePriority())
        {
            if (config_.require_realtime_priority)
            {
                result.AddError(
                    "Real-time priority is not available. Run with appropriate privileges or configure PAM limits.");
            }
            else
            {
                result.AddWarning(
                    "Real-time priority is not available. Thread scheduling may cause latency jitter.");
            }
        }
        
        // Check for CPU frequency scaling issues
        if (HasCPUFrequencyScalingIssues())
        {
            result.AddWarning(
                "CPU frequency scaling detected. This may cause performance variability.");
        }
    }
    
    void ValidateNUMATopology(ValidationResult& result)
    {
        if (config_.verbose_validation)
        {
            std::cout << "[SystemValidator] Validating NUMA topology..." << std::endl;
        }
        
        int numa_nodes = GetNUMANodes();
        if (numa_nodes > 1)
        {
            int target_node = GetNUMANodeForCPU(config_.target_cpu_core);
            if (target_node < 0)
            {
                result.AddWarning("Unable to determine NUMA node for target CPU.");
            }
            else
            {
                result.AddRecommendation(std::format(
                    "System has {} NUMA nodes. Ensure memory is allocated on NUMA node {} for CPU {}.",
                    numa_nodes, target_node, config_.target_cpu_core));
            }
        }
    }
    
    void ValidatePerformanceCounters(ValidationResult& result)
    {
        if (config_.verbose_validation)
        {
            std::cout << "[SystemValidator] Validating performance counters..." << std::endl;
        }
        
        if (!HasPerformanceCounters())
        {
            result.AddWarning(
                "Hardware performance counters are not available. Performance monitoring will be limited.");
        }
    }
    
    void GenerateRecommendations(ValidationResult& result)
    {
        if (result.is_valid)
        {
            result.AddRecommendation("System configuration appears optimal for HFT workloads.");
        }
        
        // Always provide general recommendations
        result.AddRecommendation(std::format(
            "Consider running the engine with: taskset -c {} chrt -f 99 ./orderbook",
            config_.target_cpu_core));
        result.AddRecommendation("Monitor system performance with: perf stat -e cache-misses,cache-references,instructions,cycles");
        const int numa_node = GetNUMANodeForCPU(config_.target_cpu_core);
        result.AddRecommendation(std::format(
            "Use numactl --cpunodebind={} --membind={} for NUMA-aware execution",
            numa_node,
            numa_node));
    }
    
    void PrintValidationResults(const ValidationResult& result) const
    {
        std::cout << "\n=== System Validation Results ===" << std::endl;
        std::cout << "Status: " << (result.is_valid ? "✓ VALID" : "✗ INVALID") << std::endl;
        
        if (!result.errors.empty())
        {
            std::cout << "\nErrors:" << std::endl;
            for (const auto& error : result.errors)
            {
                std::cout << "  ✗ " << error << std::endl;
            }
        }
        
        if (!result.warnings.empty())
        {
            std::cout << "\nWarnings:" << std::endl;
            for (const auto& warning : result.warnings)
            {
                std::cout << "  ⚠ " << warning << std::endl;
            }
        }
        
        if (!result.recommendations.empty())
        {
            std::cout << "\nRecommendations:" << std::endl;
            for (const auto& rec : result.recommendations)
            {
                std::cout << "  → " << rec << std::endl;
            }
        }
        
        std::cout << std::endl;
    }
    
    // System information getters
    [[nodiscard]] bool IsCPUIsolated(int cpu) const
    {
#ifdef __linux__
        std::ifstream cmdline("/proc/cmdline");
        if (!cmdline.is_open()) return false;
        
        std::string line;
        std::getline(cmdline, line);
        
        // Look for isolcpus parameter
        size_t pos = line.find("isolcpus=");
        if (pos == std::string::npos) return false;
        
        // Parse isolcpus value
        pos += 9; // Skip "isolcpus="
        size_t end = line.find(' ', pos);
        std::string isolcpus = (end == std::string::npos) ? 
                               line.substr(pos) : line.substr(pos, end - pos);
        
        // Check if our target CPU is in the isolated list
        return isolcpus.find(std::to_string(cpu)) != std::string::npos;
#else
        (void)cpu;
        return false; // Not supported on non-Linux
#endif
    }
    
    [[nodiscard]] std::string GetCPUGovernor(int cpu = -1) const
    {
#ifdef __linux__
        std::string path = "/sys/devices/system/cpu/cpu" + 
                          (cpu >= 0 ? std::to_string(cpu) : "0") + 
                          "/cpufreq/scaling_governor";
        
        std::ifstream gov(path);
        if (!gov.is_open()) return "unknown";
        
        std::string governor;
        std::getline(gov, governor);
        return governor;
#else
        (void)cpu;
        return "unknown";
#endif
    }
    
    [[nodiscard]] int GetCPUFrequency(int cpu = 0) const
    {
#ifdef __linux__
        std::string path = "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + 
                          "/cpufreq/scaling_cur_freq";
        
        std::ifstream freq(path);
        if (!freq.is_open()) return 0;
        
        int frequency;
        freq >> frequency;
        return frequency / 1000; // Convert to MHz
#else
        (void)cpu;
        return 0;
#endif
    }
    
    [[nodiscard]] bool HasHyperthreading() const
    {
#ifdef __linux__
        std::ifstream cpuinfo("/proc/cpuinfo");
        if (!cpuinfo.is_open()) return false;
        
        std::string line;
        int siblings = 0;
        int cpu_cores = 0;
        
        while (std::getline(cpuinfo, line))
        {
            if (line.find("siblings") == 0)
            {
                std::istringstream iss(line);
                std::string key, value;
                iss >> key >> value >> siblings;
            }
            else if (line.find("cpu cores") == 0)
            {
                std::istringstream iss(line);
                std::string key, value;
                iss >> key >> value >> cpu_cores;
            }
        }
        
        return siblings > cpu_cores;
#else
        return false;
#endif
    }
    
    [[nodiscard]] size_t GetHugePagesFree() const
    {
#ifdef __linux__
        std::ifstream meminfo("/proc/meminfo");
        if (!meminfo.is_open()) return 0;
        
        std::string line;
        while (std::getline(meminfo, line))
        {
            if (line.find("HugePages_Free") == 0)
            {
                std::istringstream iss(line);
                std::string key;
                size_t value;
                iss >> key >> value;
                return value * 2048; // Convert to KB (2MB huge pages)
            }
        }
#endif
        return 0;
    }
    
    [[nodiscard]] std::string GetHugePagesInfo() const
    {
        size_t free = GetHugePagesFree();
        return std::format("{} MB free", free / 1024);
    }
    
    [[nodiscard]] bool IsTransparentHugePagesEnabled() const
    {
#ifdef __linux__
        std::ifstream thp("/sys/kernel/mm/transparent_hugepage/enabled");
        if (!thp.is_open()) return false;
        
        std::string status;
        std::getline(thp, status);
        return status.find("[never]") == std::string::npos;
#else
        return false;
#endif
    }
    
    [[nodiscard]] double GetMemoryBandwidth() const
    {
        // This is a simplified estimation
        // Real implementation would use performance counters
        return 25.0; // GB/s - placeholder
    }
    
    [[nodiscard]] bool HasRealtimePriority() const
    {
#ifdef __linux__
        // Check if we can set real-time priority
        struct sched_param param{};
        param.sched_priority = sched_get_priority_max(SCHED_FIFO);
        
        int ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
        if (ret == 0)
        {
            // Reset to normal priority
            param.sched_priority = 0;
            pthread_setschedparam(pthread_self(), SCHED_OTHER, &param);
            return true;
        }
        return false;
#else
        return false;
#endif
    }
    
    [[nodiscard]] bool HasCPUFrequencyScalingIssues() const
    {
        // Check for Intel P-state driver issues
#ifdef __linux__
        std::ifstream cmdline("/proc/cmdline");
        if (!cmdline.is_open()) return false;
        
        std::string line;
        std::getline(cmdline, line);
        return line.find("intel_pstate=disable") == std::string::npos;
#else
        return false;
#endif
    }
    
    [[nodiscard]] int GetNUMANodes() const
    {
#ifdef __linux__
        return std::distance(std::filesystem::directory_iterator("/sys/devices/system/node"),
                            std::filesystem::directory_iterator());
#else
        return 1;
#endif
    }
    
    [[nodiscard]] int GetNUMANodeForCPU(int cpu) const
    {
#ifdef __linux__
        std::string path = "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/topology/physical_package_id";
        std::ifstream pkg(path);
        if (!pkg.is_open()) return -1;
        
        int node;
        pkg >> node;
        return node;
#else
        (void)cpu;
        return 0;
#endif
    }
    
    [[nodiscard]] bool HasPerformanceCounters() const
    {
        // Check if perf_event_open is available
#ifdef __linux__
        return access("/proc/sys/kernel/perf_event_paranoid", F_OK) == 0;
#else
        return false;
#endif
    }
    
    // Automatic tuning functions
    bool TuneCPU()
    {
#ifdef __linux__
        try
        {
            // Set CPU governor to performance
            std::string governor_path = "/sys/devices/system/cpu/cpu" + 
                                       std::to_string(config_.target_cpu_core) + 
                                       "/cpufreq/scaling_governor";
            
            std::ofstream gov(governor_path);
            if (gov.is_open())
            {
                gov << "performance";
                gov.close();
                return true;
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to tune CPU: " << e.what() << std::endl;
        }
#endif
        return false;
    }
    
    bool TuneMemory()
    {
        // Memory tuning would require root privileges
        // This is a placeholder for actual implementation
        return false;
    }
    
private:
    SystemConfig config_;
};
