# Deep Dive into Thread Affinity (CPU Pinning): Mastering Performance Optimization

## Introduction

Welcome to this comprehensive exploration of **thread affinity (CPU pinning)** - a powerful technique for optimizing performance in multithreaded applications. While modern operating system schedulers do an excellent job of managing threads across CPU cores, there are scenarios where taking control of thread placement can yield significant performance improvements, particularly in low-latency and high-performance computing environments.

This 5000-word deep dive will unravel the complexities of thread affinity, exploring everything from fundamental concepts to advanced implementation techniques. We'll examine CPU topology, cache hierarchies, NUMA architectures, and practical cross-platform implementation strategies. We'll also analyze real-world performance implications and establish best practices for when and how to use thread affinity effectively.

---

## Part 1: Understanding Thread Affinity Fundamentals

### 1.1 What is Thread Affinity?

**Thread affinity** (also known as CPU pinning or processor affinity) is the practice of binding a thread to one or more specific CPU cores. By restricting which cores a thread can execute on, we can optimize cache utilization, reduce context switching overhead, and improve performance predictability.

**Key Concepts:**

- **Soft Affinity**: A preference for certain cores, but the scheduler can still migrate the thread if necessary
- **Hard Affinity**: A strict binding that prevents the thread from running on any other core
- **CPU Set**: A collection of CPU cores that a thread is allowed to run on

### 1.2 Why the OS Scheduler Isn't Always Optimal

Modern OS schedulers are sophisticated, but they operate with limited information:

**Scheduler Limitations:**

1. **No Application Knowledge**: The scheduler doesn't know which threads communicate frequently
2. **Cache Awareness Limited**: While some schedulers are cache-aware, they don't understand application-specific data sharing patterns
3. **Fairness Bias**: Schedulers prioritize fairness over peak performance
4. **Migration Cost**: Moving threads between cores incurs cache misses

### 1.3 The Cache Hierarchy Impact

Understanding cache behavior is crucial for appreciating thread affinity benefits:

```cpp
// Cache hierarchy on a modern CPU
struct CacheHierarchy {
    // L1 Cache: 32KB per core, ~1-2 cycles latency
    // L2 Cache: 256KB per core, ~10-20 cycles latency  
    // L3 Cache: 8-32MB shared, ~40-50 cycles latency
    // Main Memory: ~100-300 cycles latency
    
    // When a thread migrates between cores:
    // - L1 and L2 cache contents are lost (core-specific)
    // - L3 cache may still have data (shared)
    // - Significant performance penalty
};
```

---

## Part 2: CPU Topology and Core Architecture

### 2.1 Understanding Modern CPU Topology

Modern CPUs have complex topologies that affect thread placement decisions:

**Core Topology Example (AMD Ryzen 5900X):**

```
CCD0 (Core Complex Die 0)       CCD1 (Core Complex Die 1)
+---------------------------+   +---------------------------+
| Physical Cores: 0-5       |   | Physical Cores: 6-11      |
| SMT Siblings: 12-17       |   | SMT Siblings: 18-23       |
| L3 Cache: 32MB            |   | L3 Cache: 32MB            |
+---------------------------+   +---------------------------+
```

**Key Topology Concepts:**

- **CCD (Core Complex Die)**: Groups of cores sharing L3 cache
- **CCX (Core Complex)**: Sub-groups within a CCD
- **SMT (Simultaneous Multithreading)**: Logical cores per physical core (e.g., Intel Hyper-Threading)
- **NUMA (Non-Uniform Memory Access)**: Memory access times vary based on core location

### 2.2 Detecting CPU Topology in C++

Modern systems provide ways to detect CPU topology:

**Linux Topology Detection:**

```cpp
// Detecting available CPUs with sched_getaffinity
#include <sched.h>
#include <unistd.h>

std::vector<int> getAvailableCPUs() {
    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    sched_getaffinity(0, sizeof(cpuSet), &cpuSet);
    
    std::vector<int> cpus;
    for (int i = 0; i < CPU_SETSIZE; ++i) {
        if (CPU_ISSET(i, &cpuSet)) {
            cpus.push_back(i);
        }
    }
    return cpus;
}
```

**NUMA Topology Detection:**

```cpp
// Reading NUMA topology from /sys filesystem
#include <fstream>
#include <string>

struct NumaNode {
    int nodeId;
    std::vector<int> cpuIds;
    size_t memorySize;
};

std::vector<NumaNode> detectNumaTopology() {
    // Read /sys/devices/system/node/online
    // For each node: /sys/devices/system/node/nodeX/cpulist
    // Implementation details vary by platform
    // Modern libraries provide abstraction for this
}
```

---

## Part 3: Platform-Specific Implementation

### 3.1 Linux Implementation

Linux provides `sched_setaffinity` and `pthread_setaffinity_np` for thread affinity control.

**Setting Thread Affinity (Linux):**

```cpp
#include <pthread.h>
#include <sched.h>

bool setThreadAffinity(std::thread& thread, int cpuCore) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpuCore, &cpuset);
    
    pthread_t nativeHandle = thread.native_handle();
    return pthread_setaffinity_np(nativeHandle, sizeof(cpu_set_t), &cpuset) == 0;
}

bool setCurrentThreadAffinity(int cpuCore) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpuCore, &cpuset);
    return sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) == 0;
}
```

**Linux SMT and Core Awareness:**

```cpp
// Detecting hyperthreading siblings on Linux
int getSmtSibling(int core) {
    std::string path = "/sys/devices/system/cpu/cpu" + 
                       std::to_string(core) + "/topology/thread_siblings_list";
    std::ifstream file(path);
    std::string siblings;
    if (std::getline(file, siblings)) {
        // Parse sibling list - first entry is the core itself
        // Second entry is the SMT sibling
        // Implementation depends on format
    }
    return -1;
}
```

### 3.2 Windows Implementation

Windows provides `SetThreadAffinityMask` and related functions.

**Setting Thread Affinity (Windows):**

```cpp
#include <windows.h>
#include <processthreadsapi.h>

bool setThreadAffinity(std::thread& thread, DWORD_PTR affinityMask) {
    HANDLE handle = thread.native_handle();
    DWORD_PTR result = SetThreadAffinityMask(handle, affinityMask);
    return result != 0;
}

bool setCurrentThreadAffinity(DWORD_PTR affinityMask) {
    HANDLE handle = GetCurrentThread();
    DWORD_PTR result = SetThreadAffinityMask(handle, affinityMask);
    return result != 0;
}

// For systems with > 64 cores, use processor groups
bool setThreadAffinityGroup(std::thread& thread, WORD groupId, BYTE coreInGroup) {
    HANDLE handle = thread.native_handle();
    GROUP_AFFINITY affinity;
    affinity.Mask = 1ULL << coreInGroup;
    affinity.Group = groupId;
    affinity.Reserved[0] = affinity.Reserved[1] = affinity.Reserved[2] = 0;
    return SetThreadGroupAffinity(handle, &affinity, nullptr) != 0;
}
```

**Windows Performance Considerations:**

According to Windows development guidance, affinity should be used judiciously: "It is better not to strongly fix priority or affinity from the start. The Windows scheduler does a rather good job most of the time" . For soft real-time work, consider priorities before affinity .

### 3.3 Cross-Platform Abstraction

Creating a cross-platform abstraction layer:

```cpp
// Cross-platform thread affinity manager
class ThreadAffinityManager {
public:
    enum class Platform { Linux, Windows, MacOS, Unknown };
    
    static bool pinToCore(std::thread& thread, int core) {
        #ifdef __linux__
            return pinToCoreLinux(thread, core);
        #elif _WIN32
            return pinToCoreWindows(thread, core);
        #else
            return false; // Unsupported platform
        #endif
    }
    
    static bool pinCurrentThreadToCore(int core) {
        #ifdef __linux__
            return pinCurrentThreadLinux(core);
        #elif _WIN32
            return pinCurrentThreadWindows(core);
        #else
            return false;
        #endif
    }
    
    static std::vector<int> getAvailableCores() {
        std::vector<int> cores;
        #ifdef __linux__
            cpu_set_t cpuSet;
            CPU_ZERO(&cpuSet);
            sched_getaffinity(0, sizeof(cpuSet), &cpuSet);
            for (int i = 0; i < CPU_SETSIZE; ++i) {
                if (CPU_ISSET(i, &cpuSet)) {
                    cores.push_back(i);
                }
            }
        #elif _WIN32
            DWORD_PTR mask = GetProcessAffinityMask(GetCurrentProcess(), nullptr, nullptr);
            // Count bits in mask
            for (int i = 0; i < sizeof(DWORD_PTR) * 8; ++i) {
                if (mask & (1ULL << i)) {
                    cores.push_back(i);
                }
            }
        #endif
        return cores;
    }
    
private:
    #ifdef __linux__
    static bool pinToCoreLinux(std::thread& thread, int core) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core, &cpuset);
        return pthread_setaffinity_np(thread.native_handle(), 
                                      sizeof(cpu_set_t), &cpuset) == 0;
    }
    
    static bool pinCurrentThreadLinux(int core) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core, &cpuset);
        return sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) == 0;
    }
    #endif
    
    #ifdef _WIN32
    static bool pinToCoreWindows(std::thread& thread, int core) {
        HANDLE handle = thread.native_handle();
        DWORD_PTR mask = 1ULL << core;
        return SetThreadAffinityMask(handle, mask) != 0;
    }
    
    static bool pinCurrentThreadWindows(int core) {
        HANDLE handle = GetCurrentThread();
        DWORD_PTR mask = 1ULL << core;
        return SetThreadAffinityMask(handle, mask) != 0;
    }
    #endif
};
```

---

## Part 4: Real-World Performance Implications

### 4.1 Performance Benchmarks

Low-latency benchmarks demonstrate significant performance improvements with affinity:

**Single Thread Microbenchmark Results** :
- Min Latency: 57ns
- Average Latency: 71ns
- p50: 63ns
- p99: 102ns

**Producer-Consumer with CPU Affinity** :
- Producer pinned to CPU 2
- Consumer pinned to CPU 3
- Data transferred via L3 cache, reducing latency

**Performance Impact Summary** :

| Configuration | Average Latency | Cache Hit Rate |
|---------------|-----------------|----------------|
| No Affinity | 18.7 μs | 82% |
| Pinned to Single Core | 9.3 μs | 94% |

### 4.2 MCTS Optimization: A Real-World Case Study

In Monte Carlo Tree Search (MCTS) optimization on AMD Ryzen processors:

**Performance Improvements** :
- 1.15× speedup from reduced cross-CCD traffic
- 20-30% reduction in L3 cache misses
- 50% reduction in inter-core data movement

**Optimization Strategy** :
- ≤6 threads: Pin to single CCD for optimal cache sharing
- 7-12 threads: Use physical cores (avoid SMT overhead)
- >12 threads: Include SMT siblings (diminishing returns)

### 4.3 The Cost of Thread Migration

Without affinity, threads can migrate between cores:

```cpp
// Measuring thread migration impact
class MigrationBenchmark {
    std::atomic<long long> counter{0};
    
public:
    void run() {
        const int ITERATIONS = 10000000;
        auto start = std::chrono::high_resolution_clock::now();
        
        // Force migration by yielding frequently
        for (int i = 0; i < ITERATIONS; ++i) {
            counter.fetch_add(1);
            if (i % 1000 == 0) {
                std::this_thread::yield(); // May trigger migration
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        // Migration causes cache misses and increased latency
    }
};
```

---

## Part 5: Advanced Affinity Strategies

### 5.1 CCD-Aware Thread Placement

For multi-CCD processors, strategic placement reduces cross-CCD traffic:

```cpp
class CC DAwareScheduler {
    struct Topology {
        int ccdCount;
        std::vector<int> ccdCores; // Cores per CCD
        std::vector<int> smtSiblings;
    };
    
    Topology detectTopology() {
        // Detect from /sys/devices/system/cpu/cpuX/topology/
        // For AMD Ryzen: detect cluster_id
        // For Intel: detect physical_package_id and core_id
    }
    
public:
    std::vector<int> getOptimalCores(int threadCount) {
        auto topo = detectTopology();
        std::vector<int> cores;
        
        if (threadCount <= topo.ccdCores[0]) {
            // Single CCD: use only first CCD's physical cores
            for (int i = 0; i < threadCount; ++i) {
                cores.push_back(topo.ccdCores[i]);
            }
        } else if (threadCount <= topo.ccdCores.size()) {
            // Multiple CCDs: use physical cores only
            for (int ccd = 0; ccd < topo.ccdCount; ++ccd) {
                int coresPerCCD = threadCount / topo.ccdCount;
                for (int i = 0; i < coresPerCCD; ++i) {
                    cores.push_back(topo.ccdCores[ccd * coresPerCCD + i]);
                }
            }
        } else {
            // Include SMT siblings
            // ... complex strategy for many threads
        }
        return cores;
    }
};
```

### 5.2 NUMA-Aware Memory Allocation

When using thread affinity on NUMA systems, memory allocation should match core placement:

```cpp
#include <numa.h> // Linux-specific

class NU MAAwareMemory {
public:
    void* allocateOnNode(int node, size_t size) {
        #ifdef __linux__
            return numa_alloc_onnode(size, node);
        #else
            return malloc(size);
        #endif
    }
    
    void bindCurrentThreadToNode(int node) {
        #ifdef __linux__
            numa_bind(numa_parse_nodestring(std::to_string(node).c_str()));
        #endif
    }
};
```

### 5.3 Power-Aware Affinity

Considering power efficiency with hybrid architectures (P-cores/E-cores):

```cpp
class HybridAffinityManager {
    enum CoreType { PERFORMANCE, EFFICIENCY, ANY };
    
    std::vector<int> performanceCores;
    std::vector<int> efficiencyCores;
    
public:
    HybridAffinityManager() {
        #ifdef __linux__
            // Detect P-cores and E-cores from ACPI or CPUID
            detectCoreTypes();
        #endif
    }
    
    void pinToPerformanceCore(std::thread& thread) {
        if (!performanceCores.empty()) {
            pinToCore(thread, performanceCores[0]);
        }
    }
    
    void pinToEfficiencyCore(std::thread& thread) {
        if (!efficiencyCores.empty()) {
            pinToCore(thread, efficiencyCores[0]);
        }
    }
    
    void balanceThreads(std::vector<std::thread>& threads) {
        // Assign critical threads to P-cores
        // Background threads to E-cores
        // Balance based on workload characteristics
    }
};
```

---

## Part 6: Best Practices and Guidelines

### 6.1 When to Use Thread Affinity

**Use Affinity For:**
- High-frequency trading and low-latency applications 
- Real-time systems with strict timing requirements
- CPU-bound workloads with predictable data sharing
- Performance-critical producer-consumer patterns
- Applications sensitive to cache misses

**Don't Use Affinity For:**
- General-purpose applications
- Systems with variable workloads
- Customer-facing applications running on diverse hardware
- When you haven't measured performance issues first

### 6.2 The "Don't Pin First" Principle

According to Windows performance guidance :

> "It is better not to strongly fix priority or affinity from the start. The Windows scheduler does a rather good job most of the time. Imposing needless constraints from the app side can make things worse."

**Recommended Approach:**
1. Build and measure baseline performance
2. Identify specific bottlenecks
3. Form hypothesis about affinity benefits
4. Apply affinity in small, measured increments
5. Re-measure and verify improvements
6. Adjust or revert as needed

### 6.3 Avoiding Hard-Coded Core Numbers

Never hardcode specific core numbers :

```cpp
// BAD: Hardcoded core numbers
setAffinity(0); // Core 0 on THIS machine
setAffinity(1); // Core 1 on THIS machine

// GOOD: Detect available cores and choose strategically
auto cores = getAvailableCores();
if (cores.size() >= 2) {
    setAffinity(cores[0]); // First available core
    setAffinity(cores[1]); // Second available core
}
```

### 6.4 Thread Pool Affinity Patterns

Modern thread pools implement sophisticated affinity strategies:

```cpp
// Example from citor thread pool: per-CCD arenas
class PoolGroup {
    // Lazy construction of one ThreadPool arena per CCD
    // Cross-arena calls fall through to inline path
    // TLS participant token prevents blocking on other arenas
    
    // Whichever CCD the caller is pinned to is used
    // Arena 0 used for non-worker threads
};
```

### 6.5 The Speedb Approach: Thread Start Callbacks

Speedb's database allows configuring thread affinity via callbacks :

```cpp
// Pin all database threads to the first core
auto affinityCallback = [](std::thread::native_handle_type thr) {
    #ifdef _WIN32
        SetThreadAffinityMask(thr, 1);
    #else
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(0, &cpuset);
        pthread_setaffinity_np(thr, sizeof(cpu_set_t), &cpuset);
    #endif
};

options.on_thread_start_callback = 
    std::make_shared<std::function<void(std::thread::native_handle_type)>>(
        affinityCallback
    );
```

---

## Part 7: C++26 and the Future of Affinity

### 7.1 C++26 Standard Affinity Support

C++26 is introducing standardized CPU affinity support :

**New Interfaces:**
- `std::cpu_set_t`: Describes available core sets
- `std::this_thread::set_affinity(cpu_ids)`: Bind current thread
- `std::thread::get_affinity()`: Query affinity mask
- Cross-platform abstraction for Linux and Windows

**Example (C++26):**

```cpp
#include <thread>
#include <iostream>

int main() {
    std::cpu_set_t cpus;
    cpus.set(0); // Enable core 0
    cpus.set(1); // Enable core 1
    
    // Bind current thread to cores 0 and 1
    std::this_thread::set_affinity(cpus);
    
    std::cout << "Thread bound to CPU 0 and 1\n";
    return 0;
}
```

### 7.2 Benefits of Standardization

- **Portability**: Write once, run on Linux, Windows, and MacOS
- **Performance**: Native support optimizes for platform
- **Maintainability**: No platform-specific #ifdefs needed
- **Future-proofing**: Standard evolves with new architectures

---

## Part 8: Testing and Debugging Affinity

### 8.1 Validating Affinity Settings

Always verify that affinity settings applied correctly:

```cpp
bool verifyAffinity(std::thread& thread, int expectedCore) {
    #ifdef __linux__
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        if (pthread_getaffinity_np(thread.native_handle(), 
                                   sizeof(cpu_set_t), &cpuset) != 0) {
            return false;
        }
        return CPU_ISSET(expectedCore, &cpuset);
    #elif _WIN32
        HANDLE handle = thread.native_handle();
        DWORD_PTR mask = SetThreadAffinityMask(handle, 0);
        // Restore original mask
        SetThreadAffinityMask(handle, mask);
        return (mask & (1ULL << expectedCore)) != 0;
    #endif
    return false;
}
```

### 8.2 Monitoring Affinity in Production

Linux tools for monitoring affinity:

```bash
# View thread affinity
taskset -p <pid>

# View NUMA topology
numactl --hardware

# Monitor CPU usage per core
mpstat -P ALL 1

# Stress test with specific affinity
stress-ng --taskset 0,2-3 --cpu 3 --timeout 1m
```

### 8.3 Windows Performance Monitoring 

```powershell
# View process affinity
Get-Process -Name MyApp | Select-Object Id, ProcessName, ProcessorAffinity

# ETW capture with Windows Performance Recorder
# Track: cycle jitter, context switches, executing CPU, DPC/ISR
```

---

## Conclusion

Thread affinity is a powerful performance optimization tool, but it requires careful application and thorough understanding of the underlying hardware. The key is to measure first, then apply affinity strategically where it provides clear benefits.

**Key Takeaways:**

1. **Understand Your Hardware**: Know your CPU topology - CCDs, SMT siblings, cache hierarchy, and NUMA nodes

2. **Measure Before Affinity**: Don't apply affinity blindly. Profile your application and identify real bottlenecks

3. **Test on Target Hardware**: Customer environments vary widely. Test on the hardware your application will run on 

4. **Respect the Scheduler**: The OS scheduler is sophisticated. Affinity is a tool for specific scenarios, not a universal optimization

5. **Use Standard Interfaces When Possible**: C++26 will provide standardized affinity support - adopt it when available

6. **Consider Power Implications**: On hybrid architectures and battery-powered devices, affinity can affect power consumption

7. **Test Under Real Conditions**: Test with power-saving settings, thermal throttling, and other real-world conditions

**When to Use Affinity (Summary):**

| Scenario | Recommendation |
|----------|----------------|
| High-frequency trading | Use affinity for critical threads  |
| MCTS/Game AI | Use CCD-aware affinity  |
| Producer-consumer patterns | Pin producer and consumer to same L3 cache  |
| Database threads | Use callback-based affinity  |
| General-purpose apps | Let OS handle scheduling  |
| Customer-facing apps | Avoid hard-coded affinity  |

Thread affinity remains one of the most effective tools for achieving predictable, low-latency performance in C++ applications. With the advent of C++26's standardized support, applying affinity will become more accessible and portable than ever before.
