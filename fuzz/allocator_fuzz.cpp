/*
 * libFuzzer target: the input is decoded into an arena and a sequence of
 * alloc / free / realloc / defrag operations, checked against a model.
 * Any corrupted byte, a lost block, a stale copy that still resolves or
 * a block outside the arena aborts; ASan and UBSan catch the rest.
 *
 * Build and run: `make fuzz` (clang++), FUZZ_SECONDS=<n>.
 */
#include "allocator.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>

namespace {

alignas(std::max_align_t) unsigned char arena[32 + 64 + 8192]; // offset + the largest size

struct Live
{
    Pointer p, copy;
    std::size_t size;
    unsigned char seed;
};

struct Input
{
    const std::uint8_t* data;
    std::size_t size;

    std::uint8_t byte() { return size ? (--size, *data++) : 0; }
    std::size_t word() { return byte() | std::size_t(byte()) << 8; }
};

void check(bool ok)
{
    if (!ok)
        std::abort();
}

void fill(const Live& l)
{
    unsigned char* v = static_cast<unsigned char*>(l.p.get());
    for (std::size_t i = 0; i < l.size; ++i)
        v[i] = static_cast<unsigned char>(l.seed + i);
}

bool intact(const Live& l, std::size_t n)
{
    const unsigned char* v = static_cast<const unsigned char*>(l.p.get());
    if (!v)
        return false;
    for (std::size_t i = 0; i < n; ++i)
        if (v[i] != static_cast<unsigned char>(l.seed + i))
            return false;
    return true;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    Input in{data, size};
    unsigned char* const begin = arena + in.byte() % 32;
    const std::size_t arena_size = 64 + in.word() % 8192;

    std::unique_ptr<Allocator> a;
    try {
        a = std::make_unique<Allocator>(begin, arena_size);
    } catch (AllocError&) {
        return 0;
    }

    std::vector<Live> live;
    std::vector<Pointer> dead; // copies of freed or moved blocks
    while (in.size) {
        const std::uint8_t op = in.byte();
        const std::size_t n = 1 + in.word() % 1024;
        const std::size_t k = live.empty() ? 0 : in.byte() % live.size();
        switch (op % 4) {
        case 0:
            try {
                Pointer p = a->alloc(n);
                Live l{p, p, n, op};
                const unsigned char* v = static_cast<const unsigned char*>(p.get());
                check(v >= begin && v + n <= begin + arena_size);
                check(reinterpret_cast<std::uintptr_t>(v) % alignof(std::max_align_t) == 0);
                fill(l);
                live.push_back(l);
            } catch (AllocError&) {
            }
            break;
        case 1:
            if (live.empty())
                break;
            dead.push_back(live[k].copy);
            a->free(live[k].p);
            check(live[k].p.get() == nullptr);
            live.erase(live.begin() + static_cast<std::ptrdiff_t>(k));
            break;
        case 2: {
            if (live.empty())
                break;
            Live& l = live[k];
            void* before = l.p.get();
            try {
                a->realloc(l.p, n);
            } catch (AllocError&) {
                check(l.p.get() == before && intact(l, l.size));
                break;
            }
            check(intact(l, n < l.size ? n : l.size));
            if (l.p.get() != before) {
                check(l.copy.get() == nullptr);
                dead.push_back(l.copy);
                l.copy = l.p;
            }
            l.size = n;
            fill(l);
            break;
        }
        default:
            a->defrag();
        }

        for (const Live& l : live)
            check(intact(l, l.size) && l.copy.get() == l.p.get());
        for (const Pointer& d : dead)
            check(d.get() == nullptr);
    }
    return 0;
}
