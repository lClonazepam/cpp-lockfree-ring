# cpp-lockfree-ring

C++20 header-only lock-free rings:

- `lf::SpscRing<T, Cap>` — single-producer / single-consumer, cache-line isolated head/tail
- `lf::MpmcRing<T, Cap>` — multi-producer / multi-consumer (Dmitry Vyukov sequence-number design)

Capacity must be a power of two. Slots use explicit lifetime (`new` / destructor) so `T` need not be default-constructible.

## Why this is a high-signal project

Interviewers care about: false sharing, memory-order choice (`acquire`/`release` vs `seq_cst`), ABA via monotonic sequences, and bounded vs unbounded queues. This repo is a compact, readable reference.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/bench
```

Requires a C++20 compiler. Linux/macOS.
