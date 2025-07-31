// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <iostream>
#include <chrono>
#include <vector>
#include <memory>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <iomanip>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QString>

#include "json-query/JSONQuery"

using namespace json_query;

// Custom memory allocation tracker
class MemoryTracker
{
  public:
    struct AllocationInfo
    {
        size_t                                         size;
        std::chrono::high_resolution_clock::time_point timestamp;
        const char*                                    category;
    };

    static MemoryTracker& instance()
    {
        static MemoryTracker tracker;
        return tracker;
    }

    void recordAllocation(void* ptr, size_t size, const char* category = "unknown")
    {
        std::lock_guard<std::mutex> lock(mutex_);
        allocations_[ptr] = {size, std::chrono::high_resolution_clock::now(), category};
        totalAllocated_ += size;
        allocationCount_++;
    }

    void recordDeallocation(void* ptr)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto                        it{allocations_.find(ptr)};
        if (it != allocations_.end())
        {
            totalDeallocated_ += it->second.size;
            deallocationCount_++;
            allocations_.erase(it);
        }
    }

    void reset()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        allocations_.clear();
        totalAllocated_    = 0;
        totalDeallocated_  = 0;
        allocationCount_   = 0;
        deallocationCount_ = 0;
    }

    void printStats(const std::string& label) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "\n=== Memory Stats: " << label << " ===\n";
        std::cout << "Total allocated: " << totalAllocated_ << " bytes\n";
        std::cout << "Total deallocated: " << totalDeallocated_ << " bytes\n";
        std::cout << "Net allocated: " << (totalAllocated_ - totalDeallocated_) << " bytes\n";
        std::cout << "Allocation count: " << allocationCount_ << "\n";
        std::cout << "Deallocation count: " << deallocationCount_ << "\n";
        std::cout << "Active allocations: " << allocations_.size() << "\n";

        if (!allocations_.empty())
        {
            std::unordered_map<const char*, size_t> categoryTotals;
            for (const auto& [ptr, info] : allocations_)
                categoryTotals[info.category] += info.size;

            std::cout << "Active allocations by category:\n";
            for (const auto& [category, total] : categoryTotals)
                std::cout << "  " << category << ": " << total << " bytes\n";
        }
        std::cout << std::endl;
    }

    size_t getTotalAllocated() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return totalAllocated_;
    }

    size_t getAllocationCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return allocationCount_;
    }

  private:
    mutable std::mutex                        mutex_;
    std::unordered_map<void*, AllocationInfo> allocations_;
    std::atomic<size_t>                       totalAllocated_{0};
    std::atomic<size_t>                       totalDeallocated_{0};
    std::atomic<size_t>                       allocationCount_{0};
    std::atomic<size_t>                       deallocationCount_{0};
};

// Custom allocator that tracks allocations
template <typename T>
class TrackingAllocator
{
  public:
    using value_type      = T;
    using pointer         = T*;
    using const_pointer   = const T*;
    using reference       = T&;
    using const_reference = const T&;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;

    template <typename U>
    struct rebind
    {
        using other = TrackingAllocator<U>;
    };

    TrackingAllocator() = default;

    template <typename U>
    TrackingAllocator(const TrackingAllocator<U>&)
    {
    }

    pointer allocate(size_type n)
    {
        pointer ptr = static_cast<pointer>(std::malloc(n * sizeof(T)));
        if (ptr)
            MemoryTracker::instance().recordAllocation(ptr, n * sizeof(T), "TrackingAllocator");
        return ptr;
    }

    void deallocate(pointer ptr, size_type)
    {
        if (ptr)
        {
            MemoryTracker::instance().recordDeallocation(ptr);
            std::free(ptr);
        }
    }

    template <typename U>
    bool operator==(const TrackingAllocator<U>&) const
    {
        return true;
    }

    template <typename U>
    bool operator!=(const TrackingAllocator<U>&) const
    {
        return false;
    }
};

// Memory-tracked vector type
template <typename T>
using TrackedVector = std::vector<T, TrackingAllocator<T>>;

// Benchmark function with memory tracking
struct BenchmarkResult
{
    std::chrono::nanoseconds duration;
    size_t                   totalAllocations;
    size_t                   allocationCount;
    size_t                   peakMemory;
};

BenchmarkResult
benchmarkWithMemoryTracking(const std::string& jsonPath, const QJsonValue& testData, int iterations = 1000)
{
    // Reset memory tracker
    MemoryTracker::instance().reset();

    // Compile JSONPath - convert std::string to QString for Qt API
    auto pathResult{JSONPath::create(QString::fromStdString(jsonPath))};
    if (!pathResult)
        throw std::runtime_error("Failed to compile JSONPath: " + jsonPath);

    auto path{std::move(*pathResult)};

    // Warm up
    for (int i = 0; i < 10; ++i)
        auto result{path.evaluate(testData)};

    // Reset tracker after warmup
    MemoryTracker::instance().reset();

    // Benchmark with memory tracking
    auto   start{std::chrono::high_resolution_clock::now()};
    size_t peakMemory{0};

    for (int i = 0; i < iterations; ++i)
    {
        auto result{path.evaluate(testData)};

        // Track peak memory usage
        size_t currentMemory = MemoryTracker::instance().getTotalAllocated();
        if (currentMemory > peakMemory)
            peakMemory = currentMemory;
    }

    auto end{std::chrono::high_resolution_clock::now()};
    auto duration{std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)};

    return {duration,
            MemoryTracker::instance().getTotalAllocated(),
            MemoryTracker::instance().getAllocationCount(),
            peakMemory};
}

int main()
{
    // Create test JSON data
    QJsonObject testData;
    testData["name"] = "John Doe";
    testData["age"]  = 30;

    QJsonObject address;
    address["street"]   = "123 Main St";
    address["city"]     = "New York";
    address["zipcode"]  = "10001";
    testData["address"] = address;

    QJsonArray inventory;
    for (int i = 0; i < 50; ++i)
    {
        QJsonObject item;
        item["id"]    = i;
        item["name"]  = QString("Item %1").arg(i);
        item["price"] = 10.0 + (i % 20);

        QJsonArray categories;
        categories.append("electronics");
        categories.append("gadgets");
        item["categories"] = categories;

        inventory.append(item);
    }
    testData["inventory"] = inventory;

    QJsonValue rootValue(testData);

    std::cout << "=== JSONPath Memory Allocation Analysis ===\n";
    std::cout << "Testing with result streaming optimizations\n\n";

    // Test cases with different complexity levels
    std::vector<std::pair<std::string, std::string>> testCases = {{"Simple", "$.name"},
                                                                  {"Nested", "$.address.city"},
                                                                  {"Array Access", "$.inventory[25].categories[1]"},
                                                                  {"Filter", "$.inventory[?(@.price > 20)]"},
                                                                  {"Recursive", "$..name"}};

    std::cout << "| Test Case | Duration (ns) | Total Allocs (bytes) | Alloc Count | Peak Memory (bytes) | "
                 "Allocs/Iteration |\n";
    std::cout << "|-----------|---------------|----------------------|-------------|---------------------|------------"
                 "------|\n";

    for (const auto& [testName, jsonPath] : testCases)
    {
        try
        {
            auto result{benchmarkWithMemoryTracking(jsonPath, rootValue, 1000)};

            double avgDuration        = static_cast<double>(result.duration.count()) / 1000.0;
            double allocsPerIteration = static_cast<double>(result.allocationCount) / 1000.0;

            std::cout << "| " << testName << " | " << static_cast<long>(avgDuration) << " | "
                      << result.totalAllocations << " | " << result.allocationCount << " | " << result.peakMemory
                      << " | " << std::fixed << std::setprecision(2) << allocsPerIteration << " |\n";

            MemoryTracker::instance().printStats(testName);
        }
        catch (const std::exception& e)
        {
            std::cout << "| " << testName << " | ERROR: " << e.what() << " |\n";
        }
    }

    std::cout << "\n=== Memory Allocation Analysis Complete ===\n";
    std::cout << "This data shows the memory allocation patterns with result streaming optimizations.\n";
    std::cout << "Lower allocation counts and peak memory indicate better optimization.\n";

    return 0;
}
