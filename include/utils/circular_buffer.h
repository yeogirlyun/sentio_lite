#pragma once
#include <vector>
#include <stdexcept>

namespace trading {

/**
 * Circular Buffer - Fixed-size ring buffer with O(1) operations
 *
 * Efficiently stores most recent N elements with:
 * - O(1) push_back
 * - O(1) indexed access
 * - Cache-friendly contiguous storage
 * - Automatic wraparound
 *
 * Used for price history, feature windows, etc.
 */
template<typename T>
class CircularBuffer {
private:
    std::vector<T> buffer_;
    size_t capacity_;
    size_t size_;
    size_t head_;  // Index of oldest element
    size_t tail_;  // Index where next element will be inserted

public:
    /**
     * Construct circular buffer with fixed capacity
     * @param capacity Maximum number of elements to store
     */
    explicit CircularBuffer(size_t capacity)
        : buffer_(capacity), capacity_(capacity), size_(0), head_(0), tail_(0) {}

    /**
     * Add element to buffer (overwrites oldest if full)
     */
    void push_back(const T& item) {
        buffer_[tail_] = item;
        tail_ = (tail_ + 1) % capacity_;
        if (size_ < capacity_) {
            size_++;
        } else {
            head_ = (head_ + 1) % capacity_;
        }
    }

    /**
     * Access element by index (0 = oldest, size-1 = newest)
     */
    T& operator[](size_t idx) {
        if (idx >= size_) {
            throw std::out_of_range("Index out of range");
        }
        return buffer_[(head_ + idx) % capacity_];
    }

    const T& operator[](size_t idx) const {
        if (idx >= size_) {
            throw std::out_of_range("Index out of range");
        }
        return buffer_[(head_ + idx) % capacity_];
    }

    /**
     * Number of elements currently in buffer
     */
    size_t size() const { return size_; }

    /**
     * Check if buffer is empty
     */
    bool empty() const { return size_ == 0; }

    /**
     * Check if buffer is at full capacity
     */
    bool full() const { return size_ == capacity_; }

    /**
     * Access most recent element
     */
    T& back() {
        if (empty()) throw std::runtime_error("Buffer is empty");
        return buffer_[(tail_ + capacity_ - 1) % capacity_];
    }

    const T& back() const {
        if (empty()) throw std::runtime_error("Buffer is empty");
        return buffer_[(tail_ + capacity_ - 1) % capacity_];
    }

    /**
     * Convert to vector (useful for bulk operations)
     * Returns elements in order from oldest to newest
     */
    std::vector<T> to_vector() const {
        std::vector<T> result;
        result.reserve(size_);
        for (size_t i = 0; i < size_; ++i) {
            result.push_back((*this)[i]);
        }
        return result;
    }

    /**
     * Clear all elements
     */
    void clear() {
        size_ = 0;
        head_ = 0;
        tail_ = 0;
    }
};

} // namespace trading
