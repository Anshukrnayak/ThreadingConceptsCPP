# Advanced OS Concepts: Filling the Gaps

## Introduction

This document addresses the gaps identified in the repository review, providing in-depth coverage of advanced operating system concepts that are essential for production-grade concurrent programming. While the existing material excellently covers fundamentals, these advanced topics represent the frontier of systems-level concurrency.

---

## Part 1: Lock-Free Data Structures

### 1.1 The ABA Problem

The ABA problem is a classic pitfall in lock-free programming using Compare-And-Swap (CAS):

```
+============================================================================+
|                    THE ABA PROBLEM                                         |
+============================================================================+
|                                                                             |
|  Scenario:                                                                  |
|  +------------------------------------------------------------------+     |
|  |  Thread 1 reads pointer P = address A                           |     |
|  |  Thread 2 modifies:                                             |     |
|  |    1. Reclaims memory at address A                             |     |
|  |    2. Allocates new memory at address A (same address!)        |     |
|  |    3. Sets pointer P to A again                                |     |
|  |  Thread 1 performs CAS(P, A, B): succeeds!                     |     |
|  |                                                                  |     |
|  |  ❌ Thread 1 incorrectly believes nothing changed                |     |
|  +------------------------------------------------------------------+     |
|                                                                             |
|  Solutions:                                                                 |
|  +------------------------------------------------------------------+     |
|  |  1. Tagged Pointers (ABA Prevention with Version Numbers)         |     |
|  |     +--------------------------------------------------+          |     |
|  |     | struct tagged_ptr {                               |          |     |
|  |     |     void* ptr;                                    |          |     |
|  |     |     uint32_t tag;  // Incremented on each change |          |     |
|  |     | };                                               |          |     |
|  |     +--------------------------------------------------+          |     |
|  |                                                                  |     |
|  |  2. Hazard Pointers                                          |     |
|  |     +--------------------------------------------------+          |     |
|  |     | Threads announce which pointers they're using    |          |     |
|  |     | Reclamation delayed until no thread uses pointer |          |     |
|  |     +--------------------------------------------------+          |     |
|  |                                                                  |     |
|  |  3. Epoch-Based Reclamation                                  |          |
|  |     +--------------------------------------------------+          |     |
|  |     | Global epoch counter                              |          |     |
|  |     | Threads increment on entry/exit                  |          |     |
|  |     | Memory freed only after safe epoch               |          |     |
|  |     +--------------------------------------------------+          |     |
|  +------------------------------------------------------------------+     |
|                                                                             |
+============================================================================+
```

### 1.2 Lock-Free Stack Implementation with Hazard Pointers

```cpp
template<typename T>
class LockFreeStack {
    struct Node {
        T data;
        std::atomic<Node*> next;
    };
    
    std::atomic<Node*> head{nullptr};
    thread_local std::vector<Node*> hazard_pointers;
    
public:
    void push(const T& value) {
        Node* new_node = new Node{value, head.load()};
        while (!head.compare_exchange_weak(new_node->next, new_node)) {
            // Retry until successful
        }
    }
    
    std::optional<T> pop() {
        // Register hazard pointer for current head
        Node* old_head = head.load();
        hazard_pointers.push_back(old_head);
        
        while (old_head && !head.compare_exchange_weak(old_head, old_head->next)) {
            // Retry
        }
        
        if (!old_head) return std::nullopt;
        
        T result = old_head->data;
        
        // Unregister hazard pointer
        hazard_pointers.pop_back();
        
        // Safe to delete? Check if any thread has hazard pointer to this node
        if (!is_hazard(old_head)) {
            delete old_head;
        } else {
            // Defer deletion
            defer_deletion(old_head);
        }
        
        return result;
    }
};
```

### 1.3 When Lock-Free is Slower

Lock-free structures are not universally faster:

```
+============================================================================+
|                    LOCK-FREE VS. MUTEX PERFORMANCE                         |
+============================================================================+
|                                                                             |
|  Lock-Free Overhead Sources:                                                |
|  +------------------------------------------------------------------+     |
|  |  • CAS Retry Loops: Spinning wastes CPU on contention              |     |
|  |  • Memory Reclamation: Hazard pointers add overhead               |     |
|  |  • Cache Line Contention: Multiple writers on same cache line     |     |
|  |  • Memory Ordering: Strong barriers on ARM/Power                  |     |
|  +------------------------------------------------------------------+     |
|                                                                             |
|  When to Use Mutex:                                                         |
|  +------------------------------------------------------------------+     |
|  |  1. Contention is high (> 10% thread blocking)                    |     |
|  |  2. Critical section is long (> 1000 cycles)                     |     |
|  |  3. Memory reclamation overhead is significant                  |     |
|  |  4. Complex data structures (trees, hash maps)                   |     |
|  +------------------------------------------------------------------+     |
|                                                                             |
|  When to Use Lock-Free:                                                     |
|  +------------------------------------------------------------------+     |
|  |  1. Contention is low (< 5% thread blocking)                     |     |
|  |  2. Critical section is short (< 100 cycles)                     |     |
|  |  3. Need real-time guarantees (no priority inversion)            |     |
|  |  4. Simple data structures (stacks, queues)                      |     |
|  +------------------------------------------------------------------+     |
|                                                                             |
+============================================================================+
```

### 1.4 Lock-Free Queue Implementation (Michael-Scott)

The Michael-Scott queue is the classic lock-free queue:

```cpp
template<typename T>
class MichaelScottQueue {
    struct Node {
        T data;
        std::atomic<Node*> next;
        Node(const T& val) : data(val), next(nullptr) {}
    };
    
    std::atomic<Node*> head;
    std::atomic<Node*> tail;
    
public:
    MichaelScottQueue() {
        Node* dummy = new Node(T{});
        head.store(dummy);
        tail.store(dummy);
    }
    
    void enqueue(const T& value) {
        Node* new_node = new Node(value);
        while (true) {
            Node* last = tail.load();
            Node* next = last->next.load();
            
            if (last == tail.load()) {
                if (next == nullptr) {
                    // Try to link new node
                    if (last->next.compare_exchange_weak(next, new_node)) {
                        // Success: advance tail
                        tail.compare_exchange_weak(last, new_node);
                        return;
                    }
                } else {
                    // Help advance tail
                    tail.compare_exchange_weak(last, next);
                }
            }
        }
    }
    
    std::optional<T> dequeue() {
        while (true) {
            Node* first = head.load();
            Node* last = tail.load();
            Node* next = first->next.load();
            
            if (first == head.load()) {
                if (first == last) {
                    if (next == nullptr) {
                        return std::nullopt;  // Empty
                    }
                    tail.compare_exchange_weak(last, next);
                } else {
                    T value = next->data;
                    if (head.compare_exchange_weak(first, next)) {
                        // Safe to delete? Need hazard pointers
                        delete first;
                        return value;
                    }
                }
            }
        }
    }
};
```

---

## Part 2: Production Profiling and Performance Analysis

### 2.1 Performance Measurement Framework

```cpp
class PerformanceProfiler {
    std::unordered_map<std::string, std::chrono::nanoseconds> timings;
    std::unordered_map<std::string, size_t> counters;
    std::mutex mtx;
    
public:
    class ScopedTimer {
        std::string name;
        std::chrono::steady_clock::time_point start;
        PerformanceProfiler& profiler;
        
    public:
        ScopedTimer(std::string name, PerformanceProfiler& p)
            : name(std::move(name)), profiler(p) {
            start = std::chrono::steady_clock::now();
        }
        
        ~ScopedTimer() {
            auto duration = std::chrono::steady_clock::now() - start;
            profiler.record(name, duration);
        }
    };
    
    void record(const std::string& name, std::chrono::nanoseconds duration) {
        std::lock_guard lock(mtx);
        timings[name] += duration;
        counters[name]++;
    }
    
    void report() const {
        for (const auto& [name, total] : timings) {
            auto count = counters.at(name);
            std::cout << name << ": " 
                      << std::chrono::duration_cast<std::chrono::microseconds>(total).count()
                      << " µs total, " << count << " calls, avg "
                      << std::chrono::duration_cast<std::chrono::nanoseconds>(total / count).count()
                      << " ns" << std::endl;
        }
    }
};
```

### 2.2 Using perf for Contention Analysis

```bash
# Record performance events
perf record -e cache-misses,cache-references,L2_cache_miss,cycles -g ./my_program

# Generate flamegraph
perf script | stackcollapse-perf.pl | flamegraph.pl > flamegraph.svg

# Analyze lock contention
perf stat -e lock:lock_acquire,lock:lock_release,lock:lock_contended ./my_program
```

### 2.3 Contention Metrics

Understanding lock contention requires multiple metrics:

```
+============================================================================+
|                    LOCK CONTENTION METRICS                                 |
+============================================================================|
|                                                                             |
|  1. Lock Attempt Rate                                                       |
|     • Number of lock() attempts per second                                |
|     • High rate → potential contention                                    |
|                                                                             |
|  2. Lock Success Rate                                                       |
|     • % of attempts that succeed immediately                              |
|     • Low success rate → high contention                                  |
|                                                                             |
|  3. Average Contention Duration                                             |
|     • How long threads wait for locks                                    |
|     • High duration → expensive critical sections                         |
|                                                                             |
|  4. Lock Holding Time                                                       |
|     • How long threads hold locks                                       |
|     • High holding time → blocks other threads                           |
|                                                                             |
|  5. Lock Spinning Count                                                    |
|     • Number of spin iterations before blocking                         |
|     • High spinning → contention but short critical sections              |
|                                                                             |
+============================================================================+
```

---

## Part 3: Advanced Patterns and Algorithms

### 3.1 Read-Copy-Update (RCU)

RCU is a synchronization mechanism used heavily in the Linux kernel:

```
+============================================================================+
|                    READ-COPY-UPDATE (RCU)                                 |
+============================================================================|
|                                                                             |
|  Principle:                                                                 |
|  +------------------------------------------------------------------+     |
|  |  Readers: No locks! Simply read the data                         |     |
|  |  Writers:                                                         |     |
|  |    1. Copy the data                                            |     |
|  |    2. Modify the copy                                          |     |
|  |    3. Atomically publish the new copy                          |     |
|  |    4. Wait for all readers to finish with old copy            |     |
|  |    5. Free the old copy                                        |     |
|  +------------------------------------------------------------------+     |
|                                                                             |
|  RCU Implementation:                                                         |
|  +------------------------------------------------------------------+     |
|  |  std::shared_ptr<int> data = std::make_shared<int>(42);           |     |
|  |                                                                  |     |
|  |  // Reader                                                        |     |
|  |  std::shared_ptr<int> read() {                                  |     |
|  |      return std::atomic_load(&data);                            |     |
|  |  }                                                               |     |
|  |                                                                  |     |
|  |  // Writer                                                        |     |
|  |  void update(int new_value) {                                    |     |
|  |      auto old = std::atomic_load(&data);                       |     |
|  |      auto new_copy = std::make_shared<int>(*old);              |     |
|  |      *new_copy = new_value;                                    |     |
|  |      std::atomic_store(&data, new_copy);                       |     |
|  |      // Wait for readers to finish                             |     |
|  |      std::this_thread::sleep_for(std::chrono::milliseconds(1));|     |
|  |  }                                                               |     |
|  +------------------------------------------------------------------+     |
|                                                                             |
|  Benefits:                                                                  |
|  +------------------------------------------------------------------+     |
|  |  • Readers never block                                        |     |
|  |  • Writers rarely block                                       |     |
|  |  • No locks in read path (extremely fast)                    |     |
|  |  • Used in Linux kernel for directory lookups, network stack |     |
|  +------------------------------------------------------------------+     |
|                                                                             |
+============================================================================+
```

### 3.2 Work-Stealing Thread Pools

Work-stealing addresses the load imbalance problem in thread pools:

```
+============================================================================+
|                    WORK-STEALING THREAD POOL                               |
+============================================================================|
|                                                                             |
|  Architecture:                                                              |
|  +------------------------------------------------------------------+     |
|  |                                                                  |     |
|  |   Worker 1          Worker 2          Worker 3                   |     |
|  |   +--------+        +--------+        +--------+                |     |
|  |   | LIFO   |        | LIFO   |        | LIFO   |                |     |
|  |   | Queue  |        | Queue  |        | Queue  |                |     |
|  |   +--------+        +--------+        +--------+                |     |
|  |       |                  |                  |                   |     |
|  |       +-------- Steal ---------+            |                   |     |
|  |                |                  |          |                   |     |
|  |                +-------- Steal ----+          |                   |     |
|  |                         |                     |                   |     |
|  |                         v                     v                   |     |
|  |   +------------------------------------------------+            |     |
|  |   |                 Stealing Algorithm              |            |     |
|  |   |  1. If local queue empty, scan other queues    |            |     |
|  |   |  2. Steal from the tail (FIFO)                 |            |     |
|  |   |  3. Random victim selection                    |            |     |
|  |   +------------------------------------------------+            |     |
|  |                                                                  |     |
|  +------------------------------------------------------------------+     |
|                                                                             |
|  Implementation:                                                             |
|  +------------------------------------------------------------------+     |
|  |  class WorkStealingPool {                                       |     |
|  |      struct Worker {                                            |     |
|  |          std::deque<std::function<void()>> deque;              |     |
|  |          std::thread thread;                                   |     |
|  |          std::mutex mutex;                                     |     |
|  |      };                                                         |     |
|  |                                                                  |     |
|  |      std::vector<std::unique_ptr<Worker>> workers;              |     |
|  |      std::atomic<size_t> next_worker{0};                       |     |
|  |                                                                  |     |
|  |      void submit(std::function<void()> task) {                 |     |
|  |          // Try to push to current thread's local queue        |     |
|  |          // Otherwise, push to a random worker's queue         |     |
|  |      }                                                           |     |
|  |                                                                  |     |
|  |      void worker_loop(size_t id) {                              |     |
|  |          while (true) {                                         |     |
|  |              std::function<void()> task;                       |     |
|  |              {                                                  |     |
|  |                  std::unique_lock lock(workers[id]->mutex);    |     |
|  |                  if (!workers[id]->deque.empty()) {           |     |
|  |                      task = std::move(workers[id]->deque.back()); |     |
|  |                      workers[id]->deque.pop_back();          |     |
|  |                  }                                              |     |
|  |              }                                                  |     |
|  |                                                                  |     |
|  |              if (!task) {                                      |     |
|  |                  // Steal from another worker                   |     |
|  |                  task = steal(id);                             |     |
|  |              }                                                  |     |
|  |                                                                  |     |
|  |              if (task) task();                                 |     |
|  |          }                                                       |     |
|  |      }                                                           |     |
|  |  };                                                              |     |
|  +------------------------------------------------------------------+     |
|                                                                             |
+============================================================================+
```

### 3.3 Readers-Writer with Writer Starvation Prevention

The classic solution prevents writer starvation:

```cpp
class WriterPreferenceRWLock {
    std::mutex mtx;
    std::condition_variable read_cv;
    std::condition_variable write_cv;
    int reader_count = 0;
    bool writer_active = false;
    int writer_waiting = 0;
    
public:
    void read_lock() {
        std::unique_lock<std::mutex> lock(mtx);
        // Block readers if writers are waiting
        read_cv.wait(lock, [this] {
            return !writer_active && writer_waiting == 0;
        });
        reader_count++;
    }
    
    void read_unlock() {
        std::unique_lock<std::mutex> lock(mtx);
        reader_count--;
        if (reader_count == 0 && writer_waiting > 0) {
            write_cv.notify_one();
        }
    }
    
    void write_lock() {
        std::unique_lock<std::mutex> lock(mtx);
        writer_waiting++;
        write_cv.wait(lock, [this] {
            return reader_count == 0 && !writer_active;
        });
        writer_waiting--;
        writer_active = true;
    }
    
    void write_unlock() {
        std::unique_lock<std::mutex> lock(mtx);
        writer_active = false;
        if (writer_waiting > 0) {
            write_cv.notify_one();
        } else {
            read_cv.notify_all();
        }
    }
};
```

### 3.4 C++20 Barriers and Semaphores

Modern C++ provides high-level synchronization primitives:

```cpp
#include <barrier>
#include <semaphore>
#include <latch>

// Barrier: N threads synchronize at a point
std::barrier sync_point(10, []() {
    std::cout << "All threads reached barrier" << std::endl;
});

void worker() {
    // Do work...
    sync_point.arrive_and_wait();
    // All threads proceed together
}

// Latch: One-time synchronization
std::latch completion(10);
void worker() {
    // Do work...
    completion.count_down();
}
completion.wait();  // Wait for all workers

// Counting Semaphore
std::counting_semaphore<10> pool(5);  // 5 resources

void use_resource() {
    pool.acquire();  // Wait for available resource
    // Use resource...
    pool.release();  // Return resource
}
```

---

## Part 4: Real-Time Systems

### 4.1 Scheduling Policies Comparison

```
+============================================================================+
|                    REAL-TIME SCHEDULING POLICIES                           |
+============================================================================|
|                                                                             |
|  SCHED_FIFO                                                                |
|  +------------------------------------------------------------------+     |
|  |  • First-In, First-Out within priority                          |     |
|  |  • No time slices (runs until blocked or preempted)            |     |
|  |  • Highest priority thread runs indefinitely                    |     |
|  |  • Use case: Critical real-time tasks with bounded execution   |     |
|  +------------------------------------------------------------------+     |
|                                                                             |
|  SCHED_RR                                                                  |
|  +------------------------------------------------------------------+     |
|  |  • Round-robin within priority                                 |     |
|  |  • Time slice (typically 100ms)                               |     |
|  |  • Preempted after time slice                                 |     |
|  |  • Use case: Multiple real-time tasks with similar priority   |     |
|  +------------------------------------------------------------------+     |
|                                                                             |
|  SCHED_DEADLINE                                                              |
|  +------------------------------------------------------------------+     |
|  |  • Earliest Deadline First (EDF)                               |     |
|  |  • Parameters: runtime, deadline, period                      |     |
|  |  • Guarantees: RT throttling & overrun protection             |     |
|  |  • Use case: Hard real-time systems (robotics, avionics)      |     |
|  +------------------------------------------------------------------+     |
|                                                                             |
|  SCHED_OTHER (CFS)                                                          |
|  +------------------------------------------------------------------+     |
|  |  • Completely Fair Scheduler                                    |     |
|  |  • Fairness over responsiveness                                |     |
|  |  • Use case: General-purpose computing                        |     |
|  +------------------------------------------------------------------+     |
|                                                                             |
+============================================================================+
```

### 4.2 Priority Boosting and Inheritance

```cpp
// Priority boosting implementation
class BoostingMutex {
    struct Waiter {
        std::thread::id tid;
        int priority;
    };
    
    std::mutex mtx;
    int current_priority = 0;
    std::vector<Waiter> waiters;
    std::thread::id owner;
    
public:
    void lock(int priority) {
        std::unique_lock lock(mtx);
        
        if (owner != std::thread::id{}) {
            // Boost owner's priority
            waiters.push_back({std::this_thread::get_id(), priority});
            boost_owner();
            cv.wait(lock, [this] { return owner == std::thread::id{}; });
        }
        
        owner = std::this_thread::get_id();
        current_priority = priority;
    }
    
    void unlock() {
        std::unique_lock lock(mtx);
        // Restore priority
        current_priority = 0;
        owner = std::thread::id{};
        cv.notify_one();
    }
};
```

---

## Conclusion

These advanced topics represent the frontier of concurrent programming. Understanding lock-free programming, performance profiling, advanced patterns like RCU, and real-time scheduling is essential for building production-grade systems that operate efficiently under real-world workloads.

The gaps identified in the original repository have been filled with practical implementations, performance analysis techniques, and a deeper understanding of when and how to apply these advanced concepts. This material, combined with the fundamentals from the original repository, provides a comprehensive education in systems-level concurrency.
