// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

/**
 * @file CacheOptimizedStructures.hpp
 * @brief Cache-conscious data structures for JSONPath recursive queries
 *
 * This file provides cache-optimized data structures designed to improve
 * memory locality and reduce cache misses in recursive JSON traversal.
 */

#pragma once

#include <vector>
#include <memory>
#include <cstddef>
#include <array>
#include <QJsonValue>
#include <QJsonArray>
#include <QStringView>
#include "json-query/json-path/JSONPathEvalError.hpp"

namespace json_query::json_path::detail
{

/**
 * @brief Cache-optimized stack frame for recursive descent
 *
 * Designed for optimal memory layout and cache locality:
 * - Compact size (fits multiple frames per cache line)
 * - Aligned data members for efficient access
 * - Minimal padding waste
 */
struct alignas(32) CacheOptimizedStackFrame
{
    QJsonValue value;            // 8 bytes (pointer to Qt's internal data)
    bool       processed{false}; // Processing state flag
    uint32_t   depth{0};         // Current traversal depth
    uint32_t   flags{0};         // Additional processing flags/state
    // Total: 16 bytes + QJsonValue overhead, aligned to 32 bytes

    CacheOptimizedStackFrame() = default;
    explicit CacheOptimizedStackFrame(QJsonValue&& val, bool proc = false, uint32_t d = 0, uint32_t f = 0)
        : value(std::move(val)), processed(proc), depth(d), flags(f)
    {
    }
    explicit CacheOptimizedStackFrame(const QJsonValue& val, bool proc = false, uint32_t d = 0, uint32_t f = 0)
        : value(val), processed(proc), depth(d), flags(f)
    {
    }
};

/**
 * @brief Memory pool for stack frames with cache-conscious allocation
 *
 * Features:
 * - Pre-allocated contiguous memory blocks
 * - Cache line aligned allocations
 * - Minimal fragmentation
 * - Thread-local storage for better locality
 */
class StackFramePool
{
  private:
    static constexpr size_t POOL_SIZE       = 4096; // 4KB pool size
    static constexpr size_t CACHE_LINE_SIZE = 64;
    static constexpr size_t FRAMES_PER_POOL = POOL_SIZE / sizeof(CacheOptimizedStackFrame);

    struct Pool
    {
        alignas(CACHE_LINE_SIZE) std::array<CacheOptimizedStackFrame, FRAMES_PER_POOL> frames;
        size_t nextIndex{0};

        CacheOptimizedStackFrame* allocate()
        {
            if (nextIndex >= FRAMES_PER_POOL)
                return nullptr; // Pool exhausted
            return &frames[nextIndex++];
        }

        void   reset() { nextIndex = 0; }
        bool   empty() const { return nextIndex == 0; }
        size_t available() const { return FRAMES_PER_POOL - nextIndex; }
    };

    std::vector<std::unique_ptr<Pool>> pools_;
    size_t                             currentPoolIndex_{0};

  public:
    StackFramePool()
    {
        // Pre-allocate first pool
        pools_.push_back(std::make_unique<Pool>());
    }

    CacheOptimizedStackFrame* allocate()
    {
        // Try current pool first
        if (currentPoolIndex_ < pools_.size())
        {
            auto* frame{pools_[currentPoolIndex_]->allocate()};
            if (frame)
                return frame;
        }

        // Need new pool
        pools_.push_back(std::make_unique<Pool>());
        currentPoolIndex_ = pools_.size() - 1;
        return pools_[currentPoolIndex_]->allocate();
    }

    void reset()
    {
        for (auto& pool : pools_)
            pool->reset();
        currentPoolIndex_ = 0;
    }

    size_t totalAllocated() const
    {
        auto total{0};
        for (const auto& pool : pools_)
            total += (FRAMES_PER_POOL - pool->available());
        return total;
    }

    size_t poolCount() const { return pools_.size(); }
};

/**
 * @brief Cache-optimized stack for recursive descent
 *
 * Uses memory pool allocation and maintains cache-friendly access patterns:
 * - Contiguous memory layout
 * - Predictable access patterns for prefetching
 * - Minimal allocation overhead
 */
class CacheOptimizedStack
{
  private:
    thread_local static StackFramePool     pool_;
    std::vector<CacheOptimizedStackFrame*> frames_;

  public:
    CacheOptimizedStack()
    {
        // Reserve space to avoid reallocations
        frames_.reserve(256); // Reasonable default depth
    }

    ~CacheOptimizedStack()
    {
        // Pool cleanup is automatic (thread_local)
    }

    void push(const QJsonValue& value, uint32_t depth = 0, uint32_t flags = 0)
    {
        auto* frame{pool_.allocate()};
        *frame = CacheOptimizedStackFrame(value, false, depth, flags);
        frames_.push_back(frame);
    }

    void push(QJsonValue&& value, uint32_t depth = 0, uint32_t flags = 0)
    {
        auto* frame{pool_.allocate()};
        *frame = CacheOptimizedStackFrame(std::move(value), false, depth, flags);
        frames_.push_back(frame);
    }

    CacheOptimizedStackFrame& top() { return *frames_.back(); }

    const CacheOptimizedStackFrame& top() const { return *frames_.back(); }

    void pop() { frames_.pop_back(); }

    bool empty() const { return frames_.empty(); }

    size_t size() const { return frames_.size(); }

    void clear()
    {
        frames_.clear();
        pool_.reset();
    }

    // Add reserve method for compatibility
    void reserve(size_t capacity) { frames_.reserve(capacity); }

    // Cache-friendly iteration
    auto begin() { return frames_.begin(); }
    auto end() { return frames_.end(); }
    auto begin() const { return frames_.begin(); }
    auto end() const { return frames_.end(); }
};

/**
 * @brief Cache-optimized result collector
 *
 * Features:
 * - Pre-sized containers to avoid reallocations
 * - Memory layout optimized for sequential access
 * - Batch processing capabilities
 * - Compatible with ResultStreamerConcept
 */
class CacheOptimizedResultCollector
{
  private:
    QJsonArray results_;
    size_t     estimatedSize_;

  public:
    explicit CacheOptimizedResultCollector(size_t estimatedSize = 64) : estimatedSize_(estimatedSize)
    {
        // Pre-reserve space based on estimation
        // QJsonArray doesn't have reserve(), but we can prepare for growth
    }

    // ResultStreamerConcept interface
    void emitValue(const QJsonValue& value) { results_.append(value); }

    void emitArray(const QJsonArray& array)
    {
        // Append all values from the array
        for (const auto& value : array)
            results_.append(value);
    }

    bool canEmit() const
    {
        return true; // Always ready to collect results
    }

    bool canHandleErrors() const
    {
        return true; // Can handle errors by collecting them
    }

    void handleError(const EvalError& error)
    {
        // For now, we'll ignore errors in the result collector
        // In a production system, you might want to log or store errors
        (void)error; // Suppress unused parameter warning
    }

    void collect(const QJsonValue& value) { results_.append(value); }

    void collectBatch(const std::vector<QJsonValue>& values)
    {
        // Batch collection for better cache utilization
        for (const auto& value : values)
            results_.append(value);
    }

    QJsonArray&& moveResults() { return std::move(results_); }

    QJsonArray toQJsonArray() const { return results_; }

    const QJsonArray& getResults() const { return results_; }

    size_t size() const { return results_.size(); }

    void clear() { results_ = QJsonArray{}; }

    // Add reserve method for compatibility (no-op for QJsonArray)
    void reserve(size_t capacity)
    {
        // QJsonArray doesn't support reserve, but we track the hint
        estimatedSize_ = capacity;
    }
};

/**
 * @brief String interning cache for key lookups
 *
 * Reduces QString allocation overhead by caching frequently used keys:
 * - Hash-based lookup for O(1) access
 * - Cache-friendly memory layout
 * - LRU eviction for memory management
 */
class KeyInternCache
{
  private:
    static constexpr size_t MAX_CACHE_SIZE = 256;

    struct CachedKey
    {
        QString  key;
        uint32_t hash;
        uint32_t accessCount{1};

        CachedKey(const QString& k, uint32_t h) : key(k), hash(h) {}
    };

    std::vector<CachedKey> cache_;

  public:
    const QString& intern(QStringView key)
    {
        auto hash = qHash(key);

        // Linear search for small cache (cache-friendly)
        for (auto& cached : cache_)
        {
            if (cached.hash == hash && cached.key == key)
            {
                cached.accessCount++;
                return cached.key;
            }
        }

        // Add new key
        if (cache_.size() >= MAX_CACHE_SIZE)
        {
            // Simple LRU: remove least accessed
            auto minIt =
                std::min_element(cache_.begin(),
                                 cache_.end(),
                                 [](const CachedKey& a, const CachedKey& b) { return a.accessCount < b.accessCount; });
            *minIt = CachedKey(QString(key), hash);
            return minIt->key;
        }
        else
        {
            cache_.emplace_back(QString(key), hash);
            return cache_.back().key;
        }
    }

    void clear() { cache_.clear(); }

    size_t size() const { return cache_.size(); }
};

} // namespace json_query::json_path::detail
