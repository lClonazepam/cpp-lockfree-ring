#include "include/lockfree_ring.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

int main() {
  constexpr std::size_t kCap = 1 << 16;
  lf::SpscRing<std::uint64_t, kCap> q;
  constexpr std::uint64_t N = 5'000'000;

  auto t0 = std::chrono::steady_clock::now();
  std::thread prod([&] {
    for (std::uint64_t i = 0; i < N;) {
      if (q.try_push(i)) ++i;
    }
  });
  std::uint64_t sum = 0;
  std::thread cons([&] {
    std::uint64_t v;
    for (std::uint64_t i = 0; i < N;) {
      if (q.try_pop(v)) {
        sum += v;
        ++i;
      }
    }
  });
  prod.join();
  cons.join();
  auto t1 = std::chrono::steady_clock::now();
  const double sec = std::chrono::duration<double>(t1 - t0).count();
  const std::uint64_t expect = N * (N - 1) / 2;
  std::cout << "SPSC ops=" << N << " sum_ok=" << (sum == expect)
            << " Mops=" << (2.0 * N / sec) / 1e6 << "\n";

  lf::MpmcRing<std::uint64_t, kCap> mq;
  constexpr int P = 4, C = 4;
  constexpr std::uint64_t per = 200'000;
  std::atomic<std::uint64_t> msum{0};
  t0 = std::chrono::steady_clock::now();
  std::vector<std::thread> ts;
  for (int i = 0; i < P; ++i) {
    ts.emplace_back([&] {
      for (std::uint64_t k = 0; k < per;) {
        if (mq.try_push(1)) ++k;
      }
    });
  }
  for (int i = 0; i < C; ++i) {
    ts.emplace_back([&] {
      std::uint64_t v;
      for (std::uint64_t k = 0; k < per;) {
        if (mq.try_pop(v)) {
          msum.fetch_add(v, std::memory_order_relaxed);
          ++k;
        }
      }
    });
  }
  for (auto& t : ts) t.join();
  t1 = std::chrono::steady_clock::now();
  sec = std::chrono::duration<double>(t1 - t0).count();
  std::cout << "MPMC sum=" << msum.load() << " expect=" << (P * per)
            << " Mops=" << (2.0 * P * per / sec) / 1e6 << "\n";
  return 0;
}
