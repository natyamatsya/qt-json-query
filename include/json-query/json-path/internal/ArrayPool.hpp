// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include <QJsonArray>
#include <vector>
#include <memory>
#include <json-query/Common.h>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_path::internal
{

/**
 * @brief Per-thread object pool for QJsonArray instances to reduce allocations
 *
 * This pool maintains a collection of pre-allocated QJsonArray objects that can
 * be reused across JSONPath evaluations, significantly reducing memory
 * allocation overhead in hot paths like recursive descent and fanOut.
 *
 * One pool exists per thread (see threadLocalArrayPool()), so acquire/return
 * are lock-free. Constraint: a PooledArray must be destroyed on the thread it
 * was acquired on (internal evaluation code never migrates it; the arrays
 * returned to callers are plain QJsonArray values, unaffected).
 */
class ArrayPool
{
  public:
    /**
     * @brief RAII wrapper for pooled QJsonArray instances
     *
     * Automatically returns the array to the pool when destroyed, ensuring
     * proper resource management and preventing memory leaks.
     */
    class PooledArray
    {
      public:
        QT_QUERY_JSON_ALWAYS_INLINE explicit PooledArray(ArrayPool& pool, std::unique_ptr<QJsonArray> array)
            : pool_(pool), array_(std::move(array))
        {
            if (QT_QUERY_JSON_LIKELY(array_))
                *array_ = QJsonArray{}; // Ensure clean state - QJsonArray doesn't have clear()
        }

        QT_QUERY_JSON_ALWAYS_INLINE ~PooledArray()
        {
            if (QT_QUERY_JSON_LIKELY(array_))
                pool_.returnArray(std::move(array_));
        }

        // Move-only semantics
        PooledArray(const PooledArray&)            = delete;
        PooledArray& operator=(const PooledArray&) = delete;

        PooledArray(PooledArray&& other) noexcept : pool_(other.pool_), array_(std::move(other.array_)) {}

        PooledArray& operator=(PooledArray&& other) noexcept
        {
            if (this != &other)
            {
                if (array_)
                    pool_.returnArray(std::move(array_));
                array_ = std::move(other.array_);
            }
            return *this;
        }

        QJsonArray&       operator*() { return *array_; }
        const QJsonArray& operator*() const { return *array_; }

        QJsonArray*       operator->() { return array_.get(); }
        const QJsonArray* operator->() const { return array_.get(); }

        QJsonArray*       get() { return array_.get(); }
        const QJsonArray* get() const { return array_.get(); }

        bool isValid() const { return array_ != nullptr; }

      private:
        ArrayPool&                  pool_;
        std::unique_ptr<QJsonArray> array_;
    };

    /**
     * @brief Acquire a pooled QJsonArray instance
     *
     * Returns a clean, empty QJsonArray wrapped in a RAII container.
     * If no pooled arrays are available, creates a new one.
     */
    QT_QUERY_JSON_ALWAYS_INLINE PooledArray acquire() { return acquireImpl(); }

    /**
     * @brief Get pool statistics for monitoring and optimization
     */
    struct Stats
    {
        size_t poolSize;
        size_t totalCreated;
        size_t acquisitions;
        size_t returns;
        double hitRate; // Percentage of acquisitions served from pool
    };

    Stats getStats() const
    {
        return {pool_.size(),
                totalCreated_,
                acquisitions_,
                returns_,
                acquisitions_ > 0 ? (static_cast<double>(returns_) / acquisitions_ * 100.0) : 0.0};
    }

    /**
     * @brief Clear the pool and reset statistics (for testing)
     */
    void clear()
    {
        pool_.clear();
        totalCreated_ = 0;
        acquisitions_ = 0;
        returns_      = 0;
    }

    // Non-copyable, non-movable
    ArrayPool(const ArrayPool&)            = delete;
    ArrayPool& operator=(const ArrayPool&) = delete;
    ArrayPool(ArrayPool&&)                 = delete;
    ArrayPool& operator=(ArrayPool&&)      = delete;

    ArrayPool()  = default;
    ~ArrayPool() = default;

  private:
    QT_QUERY_JSON_ALWAYS_INLINE PooledArray acquireImpl()
    {
        std::unique_ptr<QJsonArray> array;
        if (QT_QUERY_JSON_LIKELY(!pool_.empty()))
        {
            array = std::move(pool_.back());
            pool_.pop_back();
            *array = QJsonArray{}; // Ensure clean state - QJsonArray doesn't have clear()
        }
        else
        {
            array = std::make_unique<QJsonArray>();
            ++totalCreated_;
        }

        ++acquisitions_;
        return PooledArray(*this, std::move(array));
    }

    void returnArray(std::unique_ptr<QJsonArray> array)
    {
        if (!array)
            return;

        // Limit pool size to prevent unbounded growth
        constexpr size_t MAX_POOL_SIZE = 32;
        if (pool_.size() < MAX_POOL_SIZE)
        {
            *array = QJsonArray{}; // Ensure clean state - QJsonArray doesn't have clear()
            pool_.push_back(std::move(array));
            ++returns_;
        }
        // If pool is full, let the array be destroyed naturally
    }

    friend class PooledArray;

    std::vector<std::unique_ptr<QJsonArray>> pool_;

    // Statistics
    size_t totalCreated_{0};
    size_t acquisitions_{0};
    size_t returns_{0};
};

/// The calling thread's pool (lock-free; constructed on first use)
inline ArrayPool& threadLocalArrayPool()
{
    thread_local ArrayPool pool;
    return pool;
}

/**
 * @brief Convenience function to acquire a pooled array from this thread's pool
 */
[[nodiscard]] inline ArrayPool::PooledArray acquirePooledArray() { return threadLocalArrayPool().acquire(); }

/**
 * @brief Returns an empty QJsonArray optimized for common empty result cases
 *
 * Uses Qt's copy-on-write (COW) mechanism to efficiently share empty arrays.
 * This is more efficient than using ArrayPool for results that will remain empty.
 */
[[nodiscard]] inline QJsonArray emptyResult()
{
    static const QJsonArray EMPTY_RESULT{};
    return EMPTY_RESULT; // COW makes this copy very cheap
}

} // namespace json_query::json_path::internal
