// Micro-benchmark of the allocator against malloc/free.
// Usage: allocator_bench [blocks]   (default: 4000)
//
// Every scenario runs several times; the best wall time is reported.
// The allocator is O(blocks) per alloc() by design (first fit over the list,
// no extra memory), so the per-operation cost grows with the block count.
#include "allocator.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <optional>
#include <random>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

constexpr int kRuns = 5;
constexpr std::size_t kMinSize = 16;
constexpr std::size_t kMaxSize = 256;

volatile std::uintptr_t g_sink = 0; // keeps the compiler from dropping the work

// best of kRuns: `setup` is not timed, `fn` is
double measure(const char* name, std::size_t ops, const std::function<void()>& setup,
               const std::function<void()>& fn)
{
    double best = 1e300;
    for (int r = 0; r < kRuns; ++r) {
        setup();
        const auto t0 = Clock::now();
        fn();
        const auto t1 = Clock::now();
        best = std::min(best, std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    std::printf("%-34s %9.3f ms %10.1f ns/op\n", name, best, best * 1e6 / double(ops));
    return best;
}

} // namespace

int main(int argc, char** argv)
{
    const std::size_t blocks = (argc > 1) ? std::strtoul(argv[1], nullptr, 10) : 4000;
    if (blocks < 2) {
        std::fprintf(stderr, "usage: %s [blocks >= 2]\n", argv[0]);
        return 2;
    }

    std::mt19937 rng(42);
    std::vector<std::size_t> sizes(blocks);
    std::size_t total = 0;
    for (std::size_t& s : sizes) {
        s = kMinSize + rng() % (kMaxSize - kMinSize + 1);
        total += s;
    }
    std::vector<std::size_t> order(blocks); // a random free order
    for (std::size_t i = 0; i < blocks; ++i)
        order[i] = i;
    std::shuffle(order.begin(), order.end(), rng);

    // data rounded up + one 32-byte descriptor per block, with a margin
    std::vector<std::max_align_t> arena((total + blocks * 64) * 2 / sizeof(std::max_align_t));
    const std::size_t arena_bytes = arena.size() * sizeof(std::max_align_t);
    std::printf("blocks: %zu, sizes %zu..%zu bytes, arena %zu KiB, best of %d runs\n\n", blocks,
                kMinSize, kMaxSize, arena_bytes / 1024, kRuns);

    std::vector<Pointer> ptrs(blocks);
    std::vector<void*> raw(blocks);
    std::optional<Allocator> a;
    auto fresh = [&] {
        a.emplace(arena.data(), arena_bytes);
    };
    auto fill = [&] {
        fresh();
        for (std::size_t i = 0; i < blocks; ++i)
            ptrs[i] = a->alloc(sizes[i]);
    };
    auto fragment = [&] { // fill, then free every second block
        fill();
        for (std::size_t i = 0; i < blocks; i += 2)
            a->free(ptrs[i]);
    };

    std::puts("-- alloc N blocks, then free them in reverse order");
    measure(
        "malloc/free", 2 * blocks, [] {},
        [&] {
            for (std::size_t i = 0; i < blocks; ++i)
                raw[i] = std::malloc(sizes[i]);
            for (std::size_t i = blocks; i-- > 0;) {
                g_sink = g_sink + reinterpret_cast<std::uintptr_t>(raw[i]);
                std::free(raw[i]);
            }
        });
    measure("Allocator alloc/free", 2 * blocks, fresh, [&] {
        for (std::size_t i = 0; i < blocks; ++i)
            ptrs[i] = a->alloc(sizes[i]);
        for (std::size_t i = blocks; i-- > 0;)
            a->free(ptrs[i]);
    });

    std::puts("\n-- free N blocks in random order");
    measure(
        "free (malloc)", blocks,
        [&] {
            for (std::size_t i = 0; i < blocks; ++i)
                raw[i] = std::malloc(sizes[i]);
        },
        [&] {
            for (std::size_t i : order)
                std::free(raw[i]);
        });
    measure("Allocator::free", blocks, fill, [&] {
        for (std::size_t i : order)
            a->free(ptrs[i]);
    });

    std::puts("\n-- refill a fragmented arena (every second block freed)");
    measure("Allocator::alloc into gaps", blocks / 2, fragment, [&] {
        for (std::size_t i = 0; i < blocks; i += 2)
            ptrs[i] = a->alloc(sizes[i]);
    });

    std::puts("\n-- defrag a fragmented arena");
    const double ms = measure("Allocator::defrag", blocks / 2, fragment, [&] { a->defrag(); });
    std::printf("%-34s %9.1f MB/s of live data\n", "", double(total / 2) / 1e3 / ms);

    std::puts("\n-- Pointer::get");
    measure("get, slot cached", blocks, fill, [&] {
        for (std::size_t i = 0; i < blocks; ++i)
            g_sink = g_sink + reinterpret_cast<std::uintptr_t>(ptrs[i].get());
    });
    std::vector<Pointer> copies;
    measure(
        "get, first call after frees", blocks / 2,
        [&] {
            fill();
            copies = ptrs; // copies remember the old slots
            for (std::size_t i = 0; i < blocks; i += 2)
                a->free(ptrs[i]);
        },
        [&] {
            for (std::size_t i = 1; i < blocks; i += 2)
                g_sink = g_sink + reinterpret_cast<std::uintptr_t>(copies[i].get());
        });

    return g_sink == 1 ? 1 : 0;
}
