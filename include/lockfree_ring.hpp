#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>

namespace lf {

inline constexpr std::size_t kCacheLine = 64;

// Power-of-two capacity lock-free SPSC ring.
// Producer and consumer touch different cache lines.
template <typename T, std::size_t Capacity>
class SpscRing {
  static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of two");
  static_assert(Capacity >= 2);
  static_assert(std::is_nothrow_move_constructible_v<T>);

 public:
  SpscRing() {
    for (std::size_t i = 0; i < Capacity; ++i) {
      seq_[i].store(i, std::memory_order_relaxed);
    }
  }

  ~SpscRing() {
    T tmp;
    while (try_pop(tmp)) {
    }
  }

  SpscRing(const SpscRing&) = delete;
  SpscRing& operator=(const SpscRing&) = delete;

  template <typename U>
  bool try_push(U&& v) {
    const std::size_t pos = tail_.load(std::memory_order_relaxed);
    auto& cell = seq_[pos & (Capacity - 1)];
    const std::size_t s = cell.load(std::memory_order_acquire);
    if (s != pos) {
      return false;  // full
    }
    new (&slot_[pos & (Capacity - 1)]) T(std::forward<U>(v));
    cell.store(pos + 1, std::memory_order_release);
    tail_.store(pos + 1, std::memory_order_relaxed);
    return true;
  }

  bool try_pop(T& out) {
    const std::size_t pos = head_.load(std::memory_order_relaxed);
    auto& cell = seq_[pos & (Capacity - 1)];
    const std::size_t s = cell.load(std::memory_order_acquire);
    if (s != pos + 1) {
      return false;  // empty
    }
    T* p = reinterpret_cast<T*>(&slot_[pos & (Capacity - 1)]);
    out = std::move(*p);
    p->~T();
    cell.store(pos + Capacity, std::memory_order_release);
    head_.store(pos + 1, std::memory_order_relaxed);
    return true;
  }

 private:
  alignas(kCacheLine) std::atomic<std::size_t> head_{0};
  alignas(kCacheLine) std::atomic<std::size_t> tail_{0};
  alignas(kCacheLine) std::atomic<std::size_t> seq_[Capacity];
  alignas(kCacheLine) typename std::aligned_storage<sizeof(T), alignof(T)>::type slot_[Capacity];
};

// Bounded MPMC ring using per-slot sequence numbers (Vyukov style).
template <typename T, std::size_t Capacity>
class MpmcRing {
  static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of two");
  static_assert(Capacity >= 2);

 public:
  MpmcRing() {
    for (std::size_t i = 0; i < Capacity; ++i) {
      seq_[i].store(i, std::memory_order_relaxed);
    }
  }

  template <typename U>
  bool try_push(U&& v) {
    std::size_t pos = tail_.load(std::memory_order_relaxed);
    for (;;) {
      auto& cell = seq_[pos & (Capacity - 1)];
      const std::intptr_t dif =
          static_cast<std::intptr_t>(cell.load(std::memory_order_acquire)) -
          static_cast<std::intptr_t>(pos);
      if (dif == 0) {
        if (tail_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
          new (&slot_[pos & (Capacity - 1)]) T(std::forward<U>(v));
          cell.store(pos + 1, std::memory_order_release);
          return true;
        }
      } else if (dif < 0) {
        return false;
      } else {
        pos = tail_.load(std::memory_order_relaxed);
      }
    }
  }

  bool try_pop(T& out) {
    std::size_t pos = head_.load(std::memory_order_relaxed);
    for (;;) {
      auto& cell = seq_[pos & (Capacity - 1)];
      const std::intptr_t dif =
          static_cast<std::intptr_t>(cell.load(std::memory_order_acquire)) -
          static_cast<std::intptr_t>(pos + 1);
      if (dif == 0) {
        if (head_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
          T* p = reinterpret_cast<T*>(&slot_[pos & (Capacity - 1)]);
          out = std::move(*p);
          p->~T();
          cell.store(pos + Capacity, std::memory_order_release);
          return true;
        }
      } else if (dif < 0) {
        return false;
      } else {
        pos = head_.load(std::memory_order_relaxed);
      }
    }
  }

 private:
  alignas(kCacheLine) std::atomic<std::size_t> head_{0};
  alignas(kCacheLine) std::atomic<std::size_t> tail_{0};
  alignas(kCacheLine) std::atomic<std::size_t> seq_[Capacity];
  alignas(kCacheLine) typename std::aligned_storage<sizeof(T), alignof(T)>::type slot_[Capacity];
};

}  // namespace lf
