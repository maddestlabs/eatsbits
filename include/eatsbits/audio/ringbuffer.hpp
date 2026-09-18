#ifndef EATS_RINGBUFFER_HPP
#define EATS_RINGBUFFER_HPP

#include <atomic>
#include <cstddef>
#include <vector>
#include <type_traits>
#include <cassert>

namespace eatsbits {

/**
 * Single-Producer Single-Consumer (SPSC) Lock-Free Circular Ringbuffer.
 * Guarantees zero memory allocation and wait-free operations on both
 * push (producer) and pop (consumer) threads. Ideal for real-time audio.
 */
template <typename T, size_t Capacity = 1024>
class SpscRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for real-time safety");

public:
    SpscRingBuffer() : head_(0), tail_(0) {}

    bool push(const T& item) noexcept {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t current_head = head_.load(std::memory_order_acquire);

        if ((current_tail - current_head) >= Capacity) {
            return false; // Buffer full
        }

        buffer_[current_tail & (Capacity - 1)] = item;
        tail_.store(current_tail + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& item) noexcept {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        const size_t current_tail = tail_.load(std::memory_order_acquire);

        if (current_head == current_tail) {
            return false; // Buffer empty
        }

        item = buffer_[current_head & (Capacity - 1)];
        head_.store(current_head + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] size_t size() const noexcept {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        return (current_tail >= current_head) ? (current_tail - current_head) : 0;
    }

    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed);
    }

    void reset() noexcept {
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

private:
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
    alignas(64) T buffer_[Capacity];
};

/**
 * Event structures passed between UI/Host and Audio Thread
 */
enum class AudioEventType : uint8_t {
    NoteOn,
    NoteOff,
    SetParameter,
    AllNotesOff,
    BpmChange
};

struct AudioEvent {
    AudioEventType type;
    uint8_t channel;
    uint8_t note;
    float velocity;
    uint32_t paramId;
    float paramValue;
};

struct MeterFeedback {
    float peakLeft;
    float peakRight;
    float rmsLeft;
    float rmsRight;
};

} // namespace eatsbits

#endif // EATS_RINGBUFFER_HPP
