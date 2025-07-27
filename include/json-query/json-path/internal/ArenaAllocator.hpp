#pragma once

#include <memory>
#include <vector>
#include <cstddef>
#include <new>
#include <type_traits>

namespace json_query::json_path::internal {

/**
 * @brief High-performance arena allocator for temporary objects in JSONPath evaluation
 * 
 * This allocator provides fast allocation of temporary objects by allocating from
 * pre-allocated memory blocks. All allocations are freed together when the arena
 * is destroyed, eliminating individual deallocation overhead.
 */
class ArenaAllocator {
public:
    /**
     * @brief Create an arena with specified initial capacity
     * 
     * @param initialCapacity Initial memory block size in bytes
     */
    explicit ArenaAllocator(size_t initialCapacity = 64 * 1024) // 64KB default
        : blockSize_(initialCapacity) {
        allocateNewBlock();
    }
    
    ~ArenaAllocator() {
        for (auto& block : blocks_) {
            std::free(block);
        }
    }
    
    // Non-copyable, but movable
    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;
    
    ArenaAllocator(ArenaAllocator&& other) noexcept
        : blockSize_(other.blockSize_), blocks_(std::move(other.blocks_)),
          currentBlock_(other.currentBlock_), currentPos_(other.currentPos_),
          currentEnd_(other.currentEnd_), totalAllocated_(other.totalAllocated_) {
        other.currentBlock_ = nullptr;
        other.currentPos_ = 0;
        other.currentEnd_ = 0;
    }
    
    ArenaAllocator& operator=(ArenaAllocator&& other) noexcept {
        if (this != &other) {
            // Clean up current blocks
            for (auto& block : blocks_) {
                std::free(block);
            }
            
            blockSize_ = other.blockSize_;
            blocks_ = std::move(other.blocks_);
            currentBlock_ = other.currentBlock_;
            currentPos_ = other.currentPos_;
            currentEnd_ = other.currentEnd_;
            totalAllocated_ = other.totalAllocated_;
            
            other.currentBlock_ = nullptr;
            other.currentPos_ = 0;
            other.currentEnd_ = 0;
        }
        return *this;
    }
    
    /**
     * @brief Allocate memory with proper alignment
     * 
     * @param size Number of bytes to allocate
     * @param alignment Required alignment (default: alignof(std::max_align_t))
     * @return Pointer to allocated memory
     */
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
        // Align the current position
        size_t alignedPos = alignUp(currentPos_, alignment);
        size_t newPos = alignedPos + size;
        
        // Check if we need a new block
        if (newPos > currentEnd_) {
            // If the allocation is larger than our block size, allocate a dedicated block
            if (size > blockSize_ / 2) {
                return allocateLargeObject(size, alignment);
            }
            
            allocateNewBlock();
            alignedPos = alignUp(currentPos_, alignment);
            newPos = alignedPos + size;
        }
        
        auto result = reinterpret_cast<void*>(currentBlock_ + alignedPos);
        currentPos_ = newPos;
        totalAllocated_ += size;
        
        return result;
    }
    
    /**
     * @brief Construct an object in the arena
     * 
     * @tparam T Type of object to construct
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Pointer to constructed object
     */
    template<typename T, typename... Args>
    T* construct(Args&&... args) {
        void* memory = allocate(sizeof(T), alignof(T));
        return new(memory) T(std::forward<Args>(args)...);
    }
    
    /**
     * @brief Construct an array of objects in the arena
     * 
     * @tparam T Type of objects to construct
     * @param count Number of objects
     * @return Pointer to first object in array
     */
    template<typename T>
    T* constructArray(size_t count) {
        void* memory = allocate(sizeof(T) * count, alignof(T));
        auto array = reinterpret_cast<T*>(memory);
        
        // Default construct each element
        for (size_t i = 0; i < count; ++i) {
            new(array + i) T();
        }
        
        return array;
    }
    
    /**
     * @brief Reset the arena, making all memory available for reuse
     * 
     * This does not call destructors - only use for trivially destructible types
     * or when you've manually managed object lifetimes.
     */
    void reset() {
        if (!blocks_.empty()) {
            currentBlock_ = reinterpret_cast<char*>(blocks_[0]);
            currentPos_ = 0;
            currentEnd_ = blockSize_;
        }
        totalAllocated_ = 0;
    }
    
    /**
     * @brief Get total memory allocated by this arena
     */
    size_t getTotalAllocated() const { return totalAllocated_; }
    
    /**
     * @brief Get number of memory blocks allocated
     */
    size_t getBlockCount() const { return blocks_.size(); }
    
    /**
     * @brief Get current memory usage statistics
     */
    struct Stats {
        size_t totalAllocated;
        size_t blockCount;
        size_t currentBlockUsed;
        size_t currentBlockRemaining;
        double utilizationPercent;
    };
    
    Stats getStats() const {
        auto currentBlockUsed = currentPos_;
        auto currentBlockRemaining = currentEnd_ - currentPos_;
        double utilization = totalAllocated_ > 0 ? 
            (static_cast<double>(totalAllocated_) / (blocks_.size() * blockSize_) * 100.0) : 0.0;
        
        return {
            totalAllocated_,
            blocks_.size(),
            currentBlockUsed,
            currentBlockRemaining,
            utilization
        };
    }

private:
    void allocateNewBlock() {
        void* newBlock = std::malloc(blockSize_);
        if (!newBlock) {
            throw std::bad_alloc();
        }
        
        blocks_.push_back(newBlock);
        currentBlock_ = reinterpret_cast<char*>(newBlock);
        currentPos_ = 0;
        currentEnd_ = blockSize_;
    }
    
    void* allocateLargeObject(size_t size, size_t alignment) {
        // Allocate a dedicated block for large objects
        auto alignedSize = alignUp(size, alignment);
        void* largeBlock = std::malloc(alignedSize);
        if (!largeBlock) {
            throw std::bad_alloc();
        }
        
        blocks_.push_back(largeBlock);
        totalAllocated_ += size;
        
        return largeBlock;
    }
    
    static size_t alignUp(size_t value, size_t alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    }
    
    size_t blockSize_;
    std::vector<void*> blocks_;
    char* currentBlock_ = nullptr;
    size_t currentPos_ = 0;
    size_t currentEnd_ = 0;
    size_t totalAllocated_ = 0;
};

/**
 * @brief STL-compatible allocator using ArenaAllocator
 * 
 * This allows using ArenaAllocator with STL containers like std::vector.
 */
template<typename T>
class ArenaSTLAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    
    template<typename U>
    struct rebind {
        using other = ArenaSTLAllocator<U>;
    };
    
    explicit ArenaSTLAllocator(ArenaAllocator& arena) : arena_(&arena) {}
    
    template<typename U>
    ArenaSTLAllocator(const ArenaSTLAllocator<U>& other) : arena_(other.arena_) {}
    
    pointer allocate(size_type n) {
        return reinterpret_cast<pointer>(arena_->allocate(n * sizeof(T), alignof(T)));
    }
    
    void deallocate(pointer, size_type) {
        // Arena allocator doesn't support individual deallocation
    }
    
    template<typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        new(p) U(std::forward<Args>(args)...);
    }
    
    template<typename U>
    void destroy(U* p) {
        p->~U();
    }
    
    bool operator==(const ArenaSTLAllocator& other) const {
        return arena_ == other.arena_;
    }
    
    bool operator!=(const ArenaSTLAllocator& other) const {
        return !(*this == other);
    }
    
    ArenaAllocator* arena_;
};

/**
 * @brief Thread-local arena allocator for JSONPath evaluation
 * 
 * Provides a convenient way to get a thread-local arena for temporary allocations
 * during JSONPath evaluation.
 */
class ThreadLocalArena {
public:
    static ArenaAllocator& get() {
        thread_local ArenaAllocator arena(32 * 1024); // 32KB per thread
        return arena;
    }
    
    static void reset() {
        get().reset();
    }
    
    static ArenaAllocator::Stats getStats() {
        return get().getStats();
    }
};

} // namespace json_query::json_path::internal
