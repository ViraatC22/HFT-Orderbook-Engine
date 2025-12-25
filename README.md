# HFT Orderbook Engine

A high-performance, lock-free order matching engine designed for ultra-low latency trading systems. This C++20 implementation achieves microsecond-level processing times through advanced memory management, cache optimization, and single-threaded deterministic processing.

## 🚀 Performance Highlights

- **Throughput**: ~3,000,000 orders/second
- **Latency**: Sub-microsecond median processing time
- **Memory**: Zero-allocation hot path with object pooling
- **Architecture**: Lock-free, single-writer deterministic engine
- **Scalability**: NUMA-aware, CPU-pinned processing

## 🏗️ Architecture Overview

### Core Design Principles

**Single-Writer Principle & Determinism**
- Single-threaded consumer eliminates lock contention
- Deterministic event processing ensures predictable latency
- CPU core isolation prevents scheduler interference
- NUMA-local memory allocation minimizes interconnect latency

**Zero-Copy Memory Model**
- Object pooling eliminates dynamic allocation in hot path
- Pre-allocated memory blocks with stable layout
- Cache-line aligned structures for optimal CPU performance
- Memory-mapped ring buffers for zero-copy ingress

**Micro-Architectural Optimizations**
- 64-byte cache-line alignment prevents false sharing
- SIMD vectorization for bulk price level operations
- Branch prediction optimization through predictable code paths
- Memory prefetching for sequential data access patterns

### System Components

#### 1. Lock-Free Ingress (`LockFreeQueue.h`)
- Single-producer, single-consumer ring buffer
- Atomic head/tail indices with memory ordering guarantees
- Backpressure management at 80% queue depth threshold
- Zero-copy packet processing from NIC to application

#### 2. Object Pool Manager (`ObjectPool.h`)
- Pre-allocated order object pool (default: 10,000 objects)
- Thread-safe acquisition/release with minimal overhead
- Memory alignment for optimal CPU cache utilization
- Automatic expansion when pool exhaustion detected

#### 3. Order Matching Engine (`Orderbook.cpp`)
- Price-time priority matching algorithm
- Red-black tree price levels with FIFO order chains
- Support for multiple order types (GTC, IOC, FOK, Market)
- Real-time risk management integration

#### 4. Event Sourcing & Audit Trail (`Journaler.h`)
- Deterministic event replay capability
- Asynchronous I/O with io_uring for zero-jitter logging
- Perfect audit trail for regulatory compliance
- Crash recovery through event log replay

#### 5. Risk Management (`RiskManager.h`)
- Pre-trade risk checks with sub-microsecond latency
- Fat-finger protection and position limits
- Real-time exposure monitoring
- Kill switch functionality for emergency scenarios

## 🏛️ Professional Production Components

### Market Data Simulation Framework (`MarketDataSimulator.h`)
**Digital Twin for Real-World Validation**
- Replays historical `events.log` with configurable chaos injection
- **Chaos Injection Features:**
  - Latency spikes: Network jitter and kernel context switch simulation
  - Packet loss: 0.01% drop rate to test recovery logic
  - Sequence gaps: Out-of-sequence message injection with gap handling
  - Deterministic chaos seeding for reproducible test scenarios
- **Performance Metrics:** Real-time latency histograms, throughput monitoring
- **Integration:** Connects to `KernelBypassIngress` for realistic packet processing

### Hardware Performance Monitoring (`PerformanceMonitor.h`)
**CPU Performance Counter Integration (PAPI/PMU)**
- **Hardware Metrics Tracked:**
  - L1/L2/L3 cache misses per trade processed
  - Branch mispredictions per operation
  - Memory bandwidth utilization
  - CPU cycles per instruction (CPI)
  - Pipeline stall cycles
- **Micro-Benchmark Framework:** Code path analysis with hardware validation
- **Regression Detection:** Automatic performance degradation alerts
- **Cross-Platform:** PAPI library integration for PMU access

### Multi-Asset/Cross-Venue Architecture (`VenueManager.h`)
**Scalable Multi-Market Trading Platform**
- **Template-Based Design:** `Orderbook<AssetType>` for asset-specific logic
- **Asset Classes Supported:**
  - Equity: RegNMS compliance, penny increment support
  - Futures: Milli-based pricing, position limit management
  - FX: PIP-based pricing, large quantity handling
- **Venue Coordination:** Centralized `VenueManager` for multi-exchange operations
- **Smart Order Routing:** Cross-venue order splitting and execution
- **Risk Aggregation:** Unified position and exposure management across venues

### FIX Engine & Regulatory Compliance

#### FIX Protocol Engine (`FixEngine.h`)
**Production-Grade Exchange Connectivity**
- **Protocol Support:** FIX 4.2 and 4.4 with full session management
- **Message Types:** Order routing, execution reports, market data
- **Session Management:** Heartbeat, sequence reset, gap fill, reconnect logic
- **Performance:** Zero-copy message parsing and serialization
- **Validation:** Comprehensive message validation and rejection handling

#### MiFID II Reporter (`MiFIDReporter.h`)
**European Regulatory Compliance (RTS 6)**
- **Transaction Reporting:** Real-time submission to National Competent Authorities
- **Reference Data Reporting:** Instrument classification and venue identification
- **Transparency Calculations:** Best execution and systematic internalizer reporting
- **Validation:** LEI format validation, country code verification
- **Output Formats:** XML/CSV for regulatory submission

#### CAT Reporter (`CATReporter.h`)
**US Consolidated Audit Trail (SEC Rule 613)**
- **Industry Member Reporting:** Complete order and trade event capture
- **Event Types:** Order events, quote events, trade events, customer accounts
- **Cross-Market Surveillance:** Comprehensive audit trail for market oversight
- **Validation:** CRD number validation, event type verification
- **Daily Reporting:** Automated end-of-day regulatory file generation

## 📋 Supported Order Types

| Order Type | Description | Behavior |
|------------|-------------|----------|
| **Good Till Cancel (GTC)** | Standard limit order | Remains in book until matched or cancelled |
| **Fill And Kill (FAK/IOC)** | Immediate or cancel | Matches immediately, cancels remainder |
| **Fill Or Kill (FOK)** | All or nothing | Executes entire quantity or cancels |
| **Market** | Best execution | Matches at best available prices |

## 🛠️ Build Instructions

### Prerequisites
- C++20 compatible compiler (Clang 13+, GCC 11+)
- CMake 3.20+ (optional)
- Linux kernel 5.10+ (for production deployment)

### Compilation
```bash
# Using direct compilation
clang++ -std=c++20 -O3 main.cpp Orderbook.cpp -o orderbook

# Using CMake (if CMakeLists.txt is present)
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Professional Components Build
```bash
# Build with all professional components
clang++ -std=c++20 -O3 -DPROFESSIONAL_BUILD \
  main.cpp Orderbook.cpp professional_hft_test.cpp \
  -o professional_hft -lpapi

# Build with market data simulation only
clang++ -std=c++20 -O3 market_data_simulation.cpp -o market_sim

# Build with performance monitoring
clang++ -std=c++20 -O3 performance_monitor.cpp -o perf_monitor -lpapi
```

### Execution
```bash
# Basic execution
./orderbook

# Production deployment with CPU isolation
sudo chrt -f 99 taskset -c 7 ./orderbook

# Professional HFT system with all components
./professional_hft --config=production.cfg

# With custom kernel parameters
sudo isolcpus=7 nohz_full=7 rcu_nocbs=7 ./orderbook
```

## 📊 Performance Benchmarking

### Test Configuration
- **Orders**: 1,000,000 synthetic orders
- **Pattern**: Alternating buy/sell at price 100
- **Hardware**: Standard x86_64 server
- **Compiler**: Clang 15 with -O3 optimization

### Expected Results
```
===================================================
   HFT Orderbook Engine (Lock-Free / Zero-Alloc)   
===================================================
[Info] Initializing Engine...
[Info] Warming up Object Pool...
[Test] Starting Load Generator (1 Producer -> 1 Consumer)...
[Test] Generating 1000000 orders...
---------------------------------------------------
Results:
  Count:      1000000 orders
  Time:       ~334 ms
  Throughput: ~3000000 ops/sec
---------------------------------------------------
Latency Metrics (Internal Processing):
  p50 (Median):   ~200 ns
  p99:            ~500 ns
  p99.9 (Tail):   ~1000 ns
  Max:            ~2000 ns
---------------------------------------------------
```

## 🏛️ Project Structure

```
Orderbook/
├── Core Engine Files
│   ├── Orderbook.h/cpp          # Main matching engine
│   ├── Order.h                  # Order data structures
│   ├── OrderType.h             # Order type definitions
│   ├── Side.h                  # Buy/Side enums
│   ├── Trade.h                 # Trade execution records
│   └── Constants.h             # System constants
│
├── Memory Management
│   ├── ObjectPool.h            # Zero-allocation object pool
│   ├── LockFreeQueue.h         # SPSC ring buffer
│   └── Usings.h                # Type aliases
│
├── Performance Components
│   ├── SimdPriceMatcher.h      # SIMD price matching
│   ├── FlatPriceMap.h          # O(1) price lookup
│   ├── PriceIndexedOrderbook.h  # O(1) price-indexed orderbook
│   └── MetricsPublisher.h      # Real-time metrics
│
├── Professional Production Components
│   ├── MarketDataSimulator.h    # Digital twin with chaos injection
│   ├── PerformanceMonitor.h     # Hardware PMU/PAPI counters
│   ├── VenueManager.h          # Multi-asset/cross-venue architecture
│   ├── FixEngine.h             # FIX protocol exchange connectivity
│   ├── MiFIDReporter.h         # European regulatory reporting
│   ├── CATReporter.h           # US Consolidated Audit Trail
│   ├── ProductionOrderbook.h    # Production wrapper engine
│   ├── SharedMemoryMetrics.h    # Shared memory metrics
│   ├── SystemValidator.h        # OS/hardware tuning validator
│   ├── KernelBypassIngress.h    # Optional kernel bypass ingress
│   ├── IoUringJournaler.h       # Linux io_uring journaling backend
│   ├── AdvancedOrderTypes.h     # Iceberg, Stop, OCO order types
│   └── ProfessionalHFTSystem.h    # Unified professional system
│
├── Risk & Compliance
│   ├── RiskManager.h           # Pre-trade risk checks
│   ├── Journaler.h             # Event sourcing
│   └── RateLimiter.h           # Throughput limiting
│
├── Infrastructure
│   ├── main.cpp                # Application entry point
│   ├── professional_hft_test.cpp # Professional system integration test
│   ├── ARCHITECTURE.md         # Detailed architecture docs
│   └── README.md               # This file
│
└── Testing
    └── OrderbookTest/           # Unit test suite
        ├── test.cpp
        └── TestFiles/           # Test scenarios
```

## ⚙️ Production Deployment

### Kernel Boot Parameters
```bash
# /etc/default/grub
GRUB_CMDLINE_LINUX="isolcpus=7 nohz_full=7 rcu_nocbs=7 intel_pstate=disable"
```

### CPU Affinity and Real-Time Priority
```bash
# Pin to isolated core with FIFO scheduling
sudo chrt -f 99 taskset -c 7 ./orderbook

# Verify CPU isolation
cat /proc/cmdline | grep isolcpus
```

### Memory Configuration
```bash
# Disable transparent huge pages
echo never > /sys/kernel/mm/transparent_hugepage/enabled

# Set CPU governor to performance
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo performance > $cpu
done
```

### Network Optimization
```bash
# Increase network buffer sizes
echo 'net.core.rmem_max = 134217728' >> /etc/sysctl.conf
echo 'net.core.wmem_max = 134217728' >> /etc/sysctl.conf
sysctl -p
```

## 🔧 Configuration Options

### Object Pool Size
```cpp
// In ObjectPool.h constructor
ObjectPool(size_t initialSize = 10000)  // Adjust based on expected order volume
```

### Ring Buffer Capacity
```cpp
// In LockFreeQueue.h
static constexpr size_t Size = 65536;  // Power of 2 for performance
```

### Risk Limits
```cpp
// In RiskManager.h
static constexpr Quantity MaxOrderSize = 10000;
static constexpr Price MaxPriceDeviation = 0.1;  // 10% from market
```

### Professional System Configuration
```cpp
// In ProfessionalHFTSystem.h
ProfessionalHFTConfig config;
config.enable_market_data_simulation = true;
config.enable_performance_monitoring = true;
config.enable_multi_venue_trading = true;
config.enable_fix_engine = true;
config.enable_regulatory_compliance = true;
```

## 🧪 Testing and Validation

### Unit Tests
```bash
cd OrderbookTest
clang++ -std=c++20 -O3 test.cpp -o test
./test
```

### Professional System Integration Test
```bash
# Run comprehensive professional system test
clang++ -std=c++20 -O3 professional_hft_test.cpp -o professional_test
./professional_test

# Test with hardware performance counters
clang++ -std=c++20 -O3 professional_hft_test.cpp -o professional_test -lpapi
./professional_test --enable-papi=true
```

### Performance Regression Testing
```bash
# Run with performance profiling
perf record -g ./orderbook
perf report

# Memory profiling
valgrind --tool=massif ./orderbook

# Hardware counter analysis (requires PAPI)
./professional_test --perf-baseline=true
```

### Latency Measurement
```bash
# Hardware timestamping (requires specialized NIC)
sudo ethtool -T eth0  # Check capabilities
sudo phc2sys -s eth0 -c CLOCK_REALTIME
```

### Market Data Simulation Testing
```bash
# Test with chaos injection
./market_sim --chaos-packet-loss=0.0001 --chaos-latency-spikes=0.001

# Test sequence gap handling
./market_sim --chaos-sequence-gaps=0.0005 --max-gap-size=10

# Performance regression testing
./market_sim --replay-speed=2.0 --baseline-collection=true
```

## 🐛 Troubleshooting

### Common Issues

**Segmentation Fault on Startup**
- Check stack size: `ulimit -s`
- Verify object pool alignment requirements
- Ensure sufficient system memory

**Low Throughput**
- Verify CPU isolation is working
- Check for thermal throttling
- Ensure compiler optimization flags (-O3)
- Validate PAPI library installation for performance monitoring

**High Tail Latency**
- Monitor for NUMA effects
- Check for memory bandwidth saturation
- Verify no background processes on isolated core
- Analyze cache miss rates using PerformanceMonitor

**Market Data Simulation Issues**
- Verify events.log file exists and is readable
- Check chaos injection parameter ranges
- Validate sequence number continuity
- Monitor packet loss recovery mechanisms

**Regulatory Reporting Errors**
- Verify LEI format compliance (20 characters, alphanumeric)
- Check country code validation (ISO 3166-1 alpha-2)
- Validate currency codes (ISO 4217)
- Ensure proper timestamp formatting

### Debug Mode
```bash
# Compile with debug symbols and sanitizers
clang++ -std=c++20 -g -O0 -fsanitize=address,undefined main.cpp Orderbook.cpp -o orderbook_debug

# Run with gdb
gdb ./orderbook_debug
(gdb) run
(gdb) bt  # Backtrace on crash

# Professional system debug mode
./professional_test --debug=true --verbose=true
```

## 📈 Monitoring and Observability

### Key Metrics
- **Orders/Second**: Real-time throughput
- **p50/p99/p99.9 Latency**: Processing time percentiles
- **Queue Depth**: Ring buffer utilization
- **Memory Usage**: Object pool efficiency
- **CPU Utilization**: Core-specific performance
- **Cache Performance**: L1/L2/L3 miss rates (requires PAPI)
- **Branch Prediction**: Misprediction rates per operation
- **Market Data Quality**: Packet loss, sequence gaps, latency spikes

### Professional System Monitoring
```cpp
// Real-time system status
ProfessionalHFTSystem system(config);
system.PrintSystemStatus();

// Performance monitoring
auto* monitor = system.GetPerformanceMonitor();
auto snapshot = monitor->GetSnapshot();
std::cout << "L1 Miss Rate: " << snapshot.l1_miss_rate << std::endl;

// Market data simulation metrics
auto* simulator = system.GetMarketDataSimulator();
auto metrics = simulator->GetMetrics();
std::cout << "Packets Dropped: " << metrics.chaos_metrics.packets_dropped << std::endl;

// Multi-venue risk monitoring
auto* venue_manager = system.GetVenueManager();
auto risk_metrics = venue_manager->GetGlobalRiskMetrics();
std::cout << "Total Exposure: " << risk_metrics.total_notional_exposure << std::endl;
```

### Monitoring Integration
```cpp
// Metrics are published to shared memory
// External agents can read without impacting performance
// See SharedMemoryMetrics.h for implementation details
// PerformanceMonitor provides hardware-level counters
// MarketDataSimulator provides chaos injection metrics
// Regulatory reporters provide compliance status
```

## 🔗 Related Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md) - Detailed technical architecture
- [LICENSE.txt](LICENSE.txt) - MIT License
- Project wiki (if available) - Additional technical deep-dives

## 🤝 Contributing

This is a reference implementation for educational purposes. For production use:

1. **Security Review**: Implement proper input validation and bounds checking
2. **Regulatory Compliance**: Add comprehensive audit logging and reporting
3. **Scalability**: Consider multi-engine partitioning for different symbols
4. **Monitoring**: Integrate with your observability stack
5. **Testing**: Add comprehensive fuzzing and chaos testing
6. **Hardware Optimization**: Validate with your specific CPU architecture
7. **Network Integration**: Test with your specific exchange connectivity

## 📄 License

This project is licensed under the MIT License - see the [LICENSE.txt](LICENSE.txt) file for details.

## ⚠️ Disclaimer

This is a high-performance trading system reference implementation. Use in production environments requires:
- Comprehensive security auditing
- Regulatory compliance validation  
- Extensive stress testing with market data simulation
- Professional risk management review
- Proper monitoring and alerting systems
- Exchange connectivity certification
- Hardware performance validation

The authors are not responsible for any financial losses or system failures resulting from the use of this code in production trading environments.