#include "allocator.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <random>
#include <set>
#include <vector>

/*
 * The first group of tests is the course's original test suite
 * (it was written for GoogleTest). It runs on a tiny self-contained
 * harness, so the project has no dependencies.
 *
 * The second group are regression tests for bugs found during
 * the 2026 review; the third covers the whole public API; the last
 * one is a randomized stress test against a byte-exact model.
 */

using namespace std;

//-----------------------------------------
namespace {

struct TestCase
{
    const char* name;
    void (*fn)();
};

vector<TestCase>& registry()
{
    static vector<TestCase> tests;
    return tests;
}

int g_failures = 0;

struct Registrar
{
    Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

} // namespace

#define TEST(suite, name)                                                                          \
    static void suite##_##name();                                                                  \
    static Registrar reg_##suite##_##name(#suite "." #name, suite##_##name);                       \
    static void suite##_##name()

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "  %s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);             \
            ++g_failures;                                                                          \
        }                                                                                          \
    } while (0)

#define CHECK_THROWS(expr, errtype)                                                                \
    do {                                                                                           \
        bool thrown_ = false;                                                                      \
        try {                                                                                      \
            expr;                                                                                  \
        } catch (AllocError & e_) {                                                                \
            thrown_ = (e_.getType() == errtype);                                                   \
        }                                                                                          \
        if (!thrown_) {                                                                            \
            fprintf(stderr, "  %s:%d: expected AllocError(%s): %s\n", __FILE__, __LINE__,          \
                    #errtype, #expr);                                                              \
            ++g_failures;                                                                          \
        }                                                                                          \
    } while (0)

//-----------------------------------------
alignas(std::max_align_t) static char buf[65536];

static void writeTo(Pointer& p, size_t size)
{
    char* v = reinterpret_cast<char*>(p.get());
    for (size_t i = 0; i < size; i++)
        v[i] = static_cast<char>(i % 31);
}
static bool isDataOk(Pointer& p, size_t size)
{
    char* v = reinterpret_cast<char*>(p.get());
    for (size_t i = 0; i < size; i++) {
        if (v[i] != static_cast<char>(i % 31))
            return false;
    }
    return true;
}
static bool isValidMemory(Pointer& p, size_t allocatedSize)
{
    char* v = reinterpret_cast<char*>(p.get());
    return (v >= buf && v + allocatedSize <= buf + sizeof(buf));
}
static bool fillUp(Allocator& a, size_t allocSize, vector<Pointer>& out)
{
    size_t Max = (2 * sizeof(buf) / allocSize); // To avoid creating an infinite loop.

    for (size_t i = 0; i < Max; i++)
        try {
            out.push_back(a.alloc(allocSize));
            writeTo(out.back(), allocSize);
        } catch (AllocError&) {
            return true;
        }

    return false;
}

//=========================================
// Original course tests
//=========================================
TEST(Allocator, AllocInRange)
{
    Allocator a(buf, sizeof(buf));
    size_t size = 500;

    Pointer p = a.alloc(size);
    char* v = reinterpret_cast<char*>(p.get());

    CHECK(v >= buf && (v + size) <= (buf + sizeof(buf)));

    a.free(p);
}

TEST(Allocator, AllocReadWrite)
{
    Allocator a(buf, sizeof(buf));

    vector<Pointer> ptr;
    size_t size = 300;
    for (int i = 0; i < 20; i++) {
        ptr.push_back(a.alloc(size));
        CHECK(isValidMemory(ptr.back(), size));
        writeTo(ptr.back(), size);
    }

    for (Pointer& p : ptr)
        CHECK(isDataOk(p, size));

    for (Pointer& p : ptr)
        a.free(p);
}

TEST(Allocator, AllocNoMem)
{
    Allocator a(buf, sizeof(buf));
    size_t size = sizeof(buf) / 5;

    vector<Pointer> ptr;
    bool thrown = false;
    try {
        for (int i = 0; i < 6; i++)
            ptr.push_back(a.alloc(size));
    } catch (AllocError& e) {
        thrown = true;
        CHECK(e.getType() == AllocErrorType::NoMemory);
    }
    CHECK(thrown);

    for (Pointer& p : ptr)
        a.free(p);
}

TEST(Allocator, AllocReuse)
{
    Allocator a(buf, sizeof(buf));

    vector<Pointer> ptrs;
    size_t size = 135;

    CHECK(fillUp(a, size, ptrs));
    a.free(ptrs[1]);

    CHECK(ptrs[1].get() == nullptr);
    ptrs[1] = a.alloc(size);

    CHECK(ptrs[1].get() != nullptr);
    writeTo(ptrs[1], size);

    for (Pointer& p : ptrs) {
        CHECK(isDataOk(p, size));
        a.free(p);
    }
}

TEST(Allocator, DefragMove)
{
    Allocator a(buf, sizeof(buf));

    set<void*> initialPtrs;
    vector<Pointer> ptrs;
    size_t size = 135;

    CHECK(fillUp(a, size, ptrs));
    a.free(ptrs[1]);
    a.free(ptrs[10]);
    a.free(ptrs[15]);

    ptrs.erase(ptrs.begin() + 15);
    ptrs.erase(ptrs.begin() + 10);
    ptrs.erase(ptrs.begin() + 1);

    for (Pointer& p : ptrs) {
        auto r = initialPtrs.insert(p.get());
        CHECK(r.second); // Ensure inserted a new element.
    }

    a.defrag();

    bool moved = false;
    for (Pointer& p : ptrs) {
        CHECK(isDataOk(p, size));
        moved = (moved || initialPtrs.find(p.get()) == initialPtrs.end());
    }
    CHECK(moved);

    for (Pointer& p : ptrs) {
        CHECK(isDataOk(p, size));
        a.free(p);
    }
}

TEST(Allocator, DefragMoveTwice)
{
    Allocator a(buf, sizeof(buf));

    vector<Pointer> ptrs;
    size_t size = 225;

    CHECK(fillUp(a, size, ptrs));

    a.free(ptrs[1]);
    a.free(ptrs[10]);
    ptrs.erase(ptrs.begin() + 10);
    ptrs.erase(ptrs.begin() + 1);

    a.defrag();

    a.free(ptrs[15]);
    ptrs.erase(ptrs.begin() + 15);

    a.defrag();

    ptrs.push_back(a.alloc(size));
    writeTo(ptrs.back(), size);

    for (Pointer& p : ptrs) {
        CHECK(isDataOk(p, size));
        a.free(p);
    }
}

TEST(Allocator, DefragAvailable)
{
    Allocator a(buf, sizeof(buf));

    vector<Pointer> ptrs;
    size_t size = 135;

    CHECK(fillUp(a, size, ptrs));

    a.free(ptrs[1]);
    a.free(ptrs[10]);
    a.free(ptrs[15]);

    ptrs.erase(ptrs.begin() + 15);
    ptrs.erase(ptrs.begin() + 10);
    ptrs.erase(ptrs.begin() + 1);

    try {
        Pointer p = a.alloc(size * 2);
        a.free(p);

        cerr << "  WARNING: Allocator not fragmented initially. Defrag tests are inconclusive."
             << endl;
    } catch (AllocError&) {
    }

    a.defrag();
    Pointer newPtr = a.alloc(size * 2);
    writeTo(newPtr, size * 2);

    for (Pointer& p : ptrs) {
        CHECK(isDataOk(p, size));
        a.free(p);
    }
}

TEST(Allocator, ReallocFromEmpty)
{
    Allocator a(buf, sizeof(buf));

    size_t size = 81;

    Pointer p;
    Pointer p1 = a.alloc(size);

    a.realloc(p, size);
    CHECK(p.get() != nullptr);

    Pointer p2 = a.alloc(size);

    writeTo(p, size);
    writeTo(p1, size);
    writeTo(p2, size);

    CHECK(isDataOk(p, size));
    CHECK(isDataOk(p1, size));
    CHECK(isDataOk(p2, size));

    a.free(p);
    a.free(p1);
    a.free(p2);
}

TEST(Allocator, ReallocGrowInplace)
{
    Allocator a(buf, sizeof(buf));

    size_t size = 135;
    Pointer p = a.alloc(size);
    writeTo(p, size);

    void* ptr = p.get();
    a.realloc(p, size * 2);

    CHECK(p.get() == ptr);
    CHECK(isDataOk(p, size));

    Pointer p2 = a.alloc(size);
    writeTo(p, size * 2);
    writeTo(p2, size);

    CHECK(isDataOk(p, size * 2));
    CHECK(isDataOk(p2, size));

    a.free(p);
    a.free(p2);
}

TEST(Allocator, ReallocShrink)
{
    Allocator a(buf, sizeof(buf));

    size_t size = 135;
    Pointer p = a.alloc(size);
    writeTo(p, size);

    void* ptr = p.get();
    a.realloc(p, size / 2);

    CHECK(p.get() == ptr);

    Pointer p2 = a.alloc(size);
    writeTo(p2, size);

    CHECK(isDataOk(p, size / 2));
    CHECK(isDataOk(p2, size));

    a.free(p);
    a.free(p2);
}

TEST(Allocator, ReallocGrow)
{
    Allocator a(buf, sizeof(buf));

    size_t size = 135;
    Pointer p = a.alloc(size);
    Pointer p2 = a.alloc(size);

    writeTo(p, size);
    writeTo(p2, size);

    void* ptr = p.get();
    a.realloc(p, size * 2);

    if (p.get() == ptr)
        cerr << "  WARNING: Reallocated chunk was not moved. realloc() grow tests inconclusive."
             << endl;

    CHECK(isDataOk(p, size));
    writeTo(p, size * 2);

    CHECK(isDataOk(p, size * 2));
    CHECK(isDataOk(p2, size));

    a.free(p);
    a.free(p2);
}

//=========================================
// Regression tests (2026 review)
//=========================================

// alloc() on an empty allocator did not check the size at all.
TEST(Regression, AllocTooLargeOnEmpty)
{
    Allocator a(buf, 1024);
    CHECK_THROWS(a.alloc(4096), NoMemory);
    CHECK_THROWS(a.alloc(0), NoMemory);
}

// The free-tail check underflowed when data reached the block
// descriptors, so new descriptors were written over user data.
TEST(Regression, FillToTheBrim)
{
    for (size_t size : {1, 7, 16, 24, 40, 100}) {
        Allocator a(buf, 2048);
        vector<Pointer> ptrs;
        CHECK(fillUp(a, size, ptrs));
        for (Pointer& p : ptrs)
            CHECK(isDataOk(p, size));
    }
}

// A block placed into a gap also needs room for one more
// descriptor below the data tail.
TEST(Regression, GapNeedsDescriptorRoom)
{
    for (size_t size : {16, 32, 48}) {
        Allocator a(buf, 2048);
        vector<Pointer> ptrs;
        CHECK(fillUp(a, size, ptrs));
        a.free(ptrs[0]);
        a.free(ptrs[2]);
        ptrs.erase(ptrs.begin() + 2);
        ptrs.erase(ptrs.begin());

        // fill both gaps and whatever is left
        fillUp(a, size, ptrs);
        for (Pointer& p : ptrs)
            CHECK(isDataOk(p, size));
    }
}

// Descriptors were placed at (base + size - sizeof (Block)) without
// alignment: misaligned access is UB (caught by UBSan).
TEST(Regression, UnalignedArena)
{
    Allocator a(buf + 1, 4093);
    vector<Pointer> ptrs;
    for (size_t size : {1, 3, 17, 33, 65}) {
        ptrs.push_back(a.alloc(size));
        writeTo(ptrs.back(), size);
        CHECK(reinterpret_cast<uintptr_t>(ptrs.back().get()) % alignof(std::max_align_t) == 0);
    }
    a.free(ptrs[1]);
    a.free(ptrs[3]);
    a.defrag();
    CHECK(isDataOk(ptrs[0], 1));
    CHECK(isDataOk(ptrs[2], 17));
    CHECK(isDataOk(ptrs[4], 65));
}

TEST(Regression, ArenaTooSmall)
{
    bool thrown = false;
    try {
        Allocator a(buf, 8);
    } catch (AllocError& e) {
        thrown = (e.getType() == NoMemory);
    }
    CHECK(thrown);
}

// Block indices were "reused" on free, so two live blocks could share
// an index and a smart pointer could follow the wrong descriptor.
TEST(Regression, IndicesStayUnique)
{
    Allocator a(buf, sizeof(buf));
    Pointer A = a.alloc(32), B = a.alloc(32), C = a.alloc(32);
    a.free(B);
    Pointer D = a.alloc(32); // goes into B's gap
    a.free(D);               // was: max_index_ dropped to A's index
    Pointer E = a.alloc(64);
    Pointer F = a.alloc(64); // was: got the same index as C
    Pointer G = a.alloc(64);
    a.free(E);
    a.free(G);

    memset(C.get(), 'c', 32);
    memset(F.get(), 'f', 64);
    CHECK(C.get() != F.get());
    CHECK(static_cast<char*>(C.get())[0] == 'c');
    CHECK(static_cast<char*>(F.get())[0] == 'f');
    (void)A;
}

// A copy of a freed smart pointer must resolve to nullptr.
TEST(Regression, StaleCopyIsNull)
{
    Allocator a(buf, sizeof(buf));
    Pointer p = a.alloc(64);
    Pointer copy = p;
    a.free(p);
    CHECK(copy.get() == nullptr);
    Pointer q = a.alloc(64);
    CHECK(copy.get() == nullptr);
    (void)q;
}

// A smart pointer remembers the descriptor slot it saw last. After frees
// that slot may lie below the descriptor area and be overwritten by user
// data; the search must not read it as a descriptor.
TEST(Regression, CachedSlotReusedByData)
{
    Allocator a(buf, 1024);
    Pointer A = a.alloc(64), B = a.alloc(64), X = a.alloc(64); // X: lowest descriptor
    writeTo(X, 64);
    void* x = X.get();
    a.free(A); // X's descriptor moves up into A's slot
    a.free(B); // X's cached slot is now two slots below the descriptor area

    // the largest block that still goes to the tail: it covers X's old slot
    Pointer big;
    size_t big_size = 1024;
    for (; big_size > 128; big_size -= 16)
        try {
            big = a.alloc(big_size);
            break;
        } catch (AllocError&) {
        }
    CHECK(big.get() != nullptr);
    uint32_t* w = static_cast<uint32_t*>(big.get());
    for (size_t i = 0; i < big_size / sizeof(uint32_t); ++i)
        w[i] = 3; // X's index everywhere

    CHECK(X.get() == x);
    CHECK(isDataOk(X, 64));
}

// defrag() dereferenced a null pointer on an empty allocator
// and never moved the first block down to the start of the arena.
TEST(Regression, DefragEmptyAndHead)
{
    Allocator a(buf, sizeof(buf));
    a.defrag();

    Pointer A = a.alloc(100);
    void* start = A.get();
    Pointer B = a.alloc(100);
    writeTo(B, 100);
    a.free(A);
    a.defrag();
    CHECK(B.get() == start);
    CHECK(isDataOk(B, 100));

    // the gap before the first block is reusable too
    a.free(B);
    Pointer C = a.alloc(100);
    CHECK(C.get() == start);
}

// Shrinking a block with an adjacent neighbour moved it and
// copied the old (larger) size into the new, smaller block.
TEST(Regression, ReallocShrinkWithNeighbour)
{
    Allocator a(buf, sizeof(buf));
    Pointer p = a.alloc(256);
    Pointer p2 = a.alloc(256);
    writeTo(p, 256);
    writeTo(p2, 256);

    void* ptr = p.get();
    a.realloc(p, 64);
    CHECK(p.get() == ptr);
    CHECK(isDataOk(p, 64));
    CHECK(isDataOk(p2, 256));
}

// Growing in place only needs (new_size - size) free bytes after the block.
TEST(Regression, ReallocGrowIntoExactGap)
{
    Allocator a(buf, sizeof(buf));
    Pointer p = a.alloc(64);
    Pointer g = a.alloc(64);
    Pointer p2 = a.alloc(64);
    writeTo(p, 64);
    a.free(g);

    void* ptr = p.get();
    a.realloc(p, 128);
    CHECK(p.get() == ptr);
    CHECK(isDataOk(p, 64));
    (void)p2;
}

TEST(Regression, ReallocToZeroFrees)
{
    Allocator a(buf, sizeof(buf));
    Pointer p = a.alloc(64);
    a.realloc(p, 0);
    CHECK(p.get() == nullptr);
}

// AllocError inherited std::runtime_error privately.
TEST(Regression, ErrorIsStdException)
{
    Allocator a(buf, 1024);
    bool caught = false;
    try {
        a.alloc(1u << 20);
    } catch (const std::exception&) {
        caught = true;
    }
    CHECK(caught);
}

//=========================================
// Unit tests: the whole public API
//=========================================

//---- Allocator::Allocator ---------------
TEST(Ctor, RejectsBadArenas)
{
    CHECK_THROWS(Allocator a(nullptr, 1024), NoMemory);
    CHECK_THROWS(Allocator a(buf, 0), NoMemory);
    CHECK_THROWS(Allocator a(buf, 16), NoMemory);            // smaller than a descriptor
    CHECK_THROWS(Allocator a(buf + 16, SIZE_MAX), NoMemory); // base + size wraps around
}

TEST(Ctor, SmallestUsableArena)
{
    // smallest arena: one aligned data unit plus the list head
    size_t n = 64;
    for (; n < 256; ++n)
        try {
            Allocator a(buf, n);
            break;
        } catch (AllocError&) {
        }
    Allocator a(buf, n);
    CHECK_THROWS(a.alloc(1), NoMemory); // no room for the block's own descriptor
}

//---- Allocator::alloc -------------------
TEST(Alloc, ReturnsAlignedDistinctBlocks)
{
    Allocator a(buf, sizeof(buf));
    set<void*> seen;
    for (size_t size = 1; size <= 300; size += 13) {
        Pointer p = a.alloc(size);
        void* v = p.get();
        CHECK(v != nullptr);
        CHECK(reinterpret_cast<uintptr_t>(v) % alignof(std::max_align_t) == 0);
        CHECK(seen.insert(v).second);
        CHECK(isValidMemory(p, size));
    }
}

TEST(Alloc, BlocksDoNotOverlap)
{
    Allocator a(buf, sizeof(buf));
    vector<Pointer> ptrs;
    vector<size_t> sizes;
    for (size_t i = 0; i < 50; ++i) {
        sizes.push_back(1 + (i * 37) % 200);
        ptrs.push_back(a.alloc(sizes.back()));
        memset(ptrs.back().get(), static_cast<int>(i), sizes.back());
    }
    for (size_t i = 0; i < ptrs.size(); ++i) {
        unsigned char* v = static_cast<unsigned char*>(ptrs[i].get());
        for (size_t j = 0; j < sizes[i]; ++j)
            if (v[j] != i) {
                CHECK(!"blocks overlap");
                return;
            }
    }
}

TEST(Alloc, RejectsZeroAndHugeSizes)
{
    Allocator a(buf, sizeof(buf));
    CHECK_THROWS(a.alloc(0), NoMemory);
    CHECK_THROWS(a.alloc(sizeof(buf)), NoMemory);
    CHECK_THROWS(a.alloc(SIZE_MAX), NoMemory);
    CHECK_THROWS(a.alloc(SIZE_MAX - 8), NoMemory); // must not wrap when rounded up
    // a failed alloc leaves the allocator usable
    Pointer p = a.alloc(100);
    CHECK(p.get() != nullptr);
}

TEST(Alloc, FirstFitReusesTheFirstGap)
{
    Allocator a(buf, sizeof(buf));
    Pointer p0 = a.alloc(64), p1 = a.alloc(64), p2 = a.alloc(64), p3 = a.alloc(64);
    void *first = p1.get(), *second = p3.get();
    a.free(p1);
    a.free(p3);
    Pointer q = a.alloc(32);
    CHECK(q.get() == first);
    Pointer r = a.alloc(64); // the rest of the first gap is too small
    CHECK(r.get() == second);
    (void)p0;
    (void)p2;
}

TEST(Alloc, FullCapacityAfterFreeingEverything)
{
    Allocator a(buf, 4096);
    vector<Pointer> first, second;
    CHECK(fillUp(a, 48, first));
    for (Pointer& p : first)
        a.free(p);
    CHECK(fillUp(a, 48, second));
    CHECK(first.size() == second.size());
}

//---- Allocator::free --------------------
TEST(Free, EmptyAndRepeatedFreeAreNoOps)
{
    Allocator a(buf, sizeof(buf));
    Pointer empty;
    a.free(empty);
    CHECK(empty.get() == nullptr);

    Pointer p = a.alloc(64), keep = a.alloc(64);
    writeTo(keep, 64);
    a.free(p);
    a.free(p);
    CHECK(p.get() == nullptr);
    CHECK(isDataOk(keep, 64));
}

TEST(Free, StaleCopyFreeDoesNotTouchOthers)
{
    Allocator a(buf, sizeof(buf));
    Pointer p = a.alloc(64);
    Pointer copy = p;
    a.free(p);
    Pointer other = a.alloc(64);
    writeTo(other, 64);
    a.free(copy); // stale: must not free `other`
    CHECK(other.get() != nullptr);
    CHECK(isDataOk(other, 64));
}

TEST(Free, PointersSurviveDescriptorMoves)
{
    // free() moves the lowest descriptor into the hole:
    // every other pointer (and its copies) must still resolve
    Allocator a(buf, sizeof(buf));
    vector<Pointer> ptrs;
    for (int i = 0; i < 20; ++i) {
        ptrs.push_back(a.alloc(40));
        writeTo(ptrs.back(), 40);
    }
    vector<Pointer> copies(ptrs);
    for (int i = 0; i < 20; i += 2)
        a.free(ptrs[i]);
    for (int i = 1; i < 20; i += 2) {
        CHECK(ptrs[i].get() == copies[i].get());
        CHECK(isDataOk(copies[i], 40));
    }
    for (int i = 0; i < 20; i += 2)
        CHECK(copies[i].get() == nullptr);
}

//---- Allocator::realloc -----------------
TEST(Realloc, EmptyPointerAndZero)
{
    Allocator a(buf, sizeof(buf));
    Pointer p;
    a.realloc(p, 0); // empty + 0: nothing happens
    CHECK(p.get() == nullptr);
    a.realloc(p, 50); // empty: like alloc
    CHECK(p.get() != nullptr);
    a.realloc(p, 0); // 0: like free
    CHECK(p.get() == nullptr);
}

TEST(Realloc, MoveKeepsDataAndFreesOldPlace)
{
    Allocator a(buf, sizeof(buf));
    Pointer p = a.alloc(64), wall = a.alloc(64);
    writeTo(p, 64);
    void* old = p.get();
    Pointer copy = p;

    a.realloc(p, 640);
    CHECK(p.get() != old);
    CHECK(isDataOk(p, 64));
    CHECK(copy.get() == nullptr); // copies do not follow a moving realloc

    Pointer reuse = a.alloc(64);
    CHECK(reuse.get() == old);
    (void)wall;
}

TEST(Realloc, FailureKeepsTheBlock)
{
    Allocator a(buf, 1024);
    Pointer p = a.alloc(128), wall = a.alloc(128);
    writeTo(p, 128);
    void* old = p.get();
    CHECK_THROWS(a.realloc(p, 4096), NoMemory);
    CHECK_THROWS(a.realloc(p, SIZE_MAX), NoMemory);
    CHECK(p.get() == old);
    CHECK(isDataOk(p, 128));
    (void)wall;
}

TEST(Realloc, GrowAtTheTailInPlace)
{
    Allocator a(buf, sizeof(buf));
    Pointer p = a.alloc(64);
    writeTo(p, 64);
    void* old = p.get();
    a.realloc(p, 30000);
    CHECK(p.get() == old);
    CHECK(isDataOk(p, 64));
    writeTo(p, 30000);
    CHECK(isDataOk(p, 30000));
}

//---- Allocator::defrag ------------------
TEST(Defrag, CompactsInAddressOrder)
{
    Allocator a(buf, sizeof(buf));
    vector<Pointer> ptrs;
    for (int i = 0; i < 10; ++i) {
        ptrs.push_back(a.alloc(48));
        writeTo(ptrs.back(), 48);
    }
    for (int i = 0; i < 10; i += 3)
        a.free(ptrs[i]);
    a.defrag();

    char* prev = nullptr;
    for (int i = 0; i < 10; ++i) {
        if (i % 3 == 0)
            continue;
        char* v = static_cast<char*>(ptrs[i].get());
        CHECK(isDataOk(ptrs[i], 48));
        if (prev)
            CHECK(v == prev + 48); // 48 is already aligned: no gaps
        prev = v;
    }
}

TEST(Defrag, MakesAllFreeSpaceContiguous)
{
    Allocator a(buf, 4096);
    vector<Pointer> ptrs;
    CHECK(fillUp(a, 64, ptrs));
    size_t freed = 0;
    for (size_t i = 0; i < ptrs.size(); i += 2, freed += 64)
        a.free(ptrs[i]);
    CHECK_THROWS(a.alloc(freed), NoMemory); // fragmented
    a.defrag();
    Pointer big = a.alloc(freed);
    CHECK(big.get() != nullptr);
    for (size_t i = 1; i < ptrs.size(); i += 2)
        CHECK(isDataOk(ptrs[i], 64));
}

TEST(Defrag, IdempotentAndCopiesFollow)
{
    Allocator a(buf, sizeof(buf));
    Pointer p = a.alloc(100), q = a.alloc(100);
    writeTo(q, 100);
    Pointer copy = q;
    a.free(p);
    a.defrag();
    void* once = q.get();
    a.defrag();
    CHECK(q.get() == once);
    CHECK(copy.get() == once);
    CHECK(isDataOk(copy, 100));
}

//---- Pointer -----------------------------
TEST(Pointer, DefaultIsNull)
{
    Pointer p;
    CHECK(p.get() == nullptr);
    Pointer copy = p;
    CHECK(copy.get() == nullptr);
}

TEST(Pointer, TwoAllocatorsAreIndependent)
{
    Allocator a(buf, 4096), b(buf + 8192, 4096);
    Pointer pa = a.alloc(64), pb = b.alloc(64);
    writeTo(pa, 64);
    writeTo(pb, 64);
    CHECK(static_cast<char*>(pa.get()) < buf + 4096);
    CHECK(static_cast<char*>(pb.get()) >= buf + 8192);
    a.free(pa);
    CHECK(pb.get() != nullptr);
    CHECK(isDataOk(pb, 64));
}

//---- AllocError --------------------------
TEST(AllocError, TypeAndMessage)
{
    AllocError e(InvalidFree, "bad free");
    CHECK(e.getType() == InvalidFree);
    CHECK(string(e.what()) == "bad free");

    Allocator a(buf, 1024);
    try {
        a.alloc(0);
        CHECK(!"no throw");
    } catch (const std::runtime_error& re) {
        CHECK(re.what() != nullptr);
    }
}

// Random alloc / free / realloc / defrag against a byte-exact model.
TEST(Stress, RandomOpsAgainstModel)
{
    struct Live
    {
        Pointer p;
        size_t size;
        unsigned char seed;
    };

    for (unsigned run = 0; run < 20; ++run) {
        mt19937 rng(run);
        const size_t arena = 1024 + 512 * run;
        Allocator a(buf + (run % 8), arena);
        vector<Live> live;

        auto fill = [](Live& l) {
            unsigned char* v = static_cast<unsigned char*>(l.p.get());
            for (size_t i = 0; i < l.size; ++i)
                v[i] = static_cast<unsigned char>(l.seed + i * 7);
        };
        auto ok = [](Live& l, size_t n) {
            unsigned char* v = static_cast<unsigned char*>(l.p.get());
            if (!v)
                return false;
            for (size_t i = 0; i < n; ++i)
                if (v[i] != static_cast<unsigned char>(l.seed + i * 7))
                    return false;
            return true;
        };

        for (int step = 0; step < 3000; ++step) {
            unsigned op = rng() % 10;
            size_t size = 1 + rng() % (arena / 8);
            if (op < 5 || live.empty()) {
                try {
                    Live l{a.alloc(size), size, static_cast<unsigned char>(rng())};
                    unsigned char* v = static_cast<unsigned char*>(l.p.get());
                    CHECK(v >= reinterpret_cast<unsigned char*>(buf) &&
                          v + size <= reinterpret_cast<unsigned char*>(buf) + run % 8 + arena);
                    fill(l);
                    live.push_back(l);
                } catch (AllocError&) {
                }
            } else if (op < 8) {
                size_t k = rng() % live.size();
                a.free(live[k].p);
                live.erase(live.begin() + k);
            } else if (op < 9) {
                size_t k = rng() % live.size();
                try {
                    a.realloc(live[k].p, size);
                    size_t kept = min(size, live[k].size);
                    CHECK(ok(live[k], kept));
                    live[k].size = size;
                    fill(live[k]);
                } catch (AllocError&) {
                    CHECK(ok(live[k], live[k].size));
                }
            } else
                a.defrag();

            for (Live& l : live)
                if (!ok(l, l.size)) {
                    CHECK(!"live block corrupted");
                    return;
                }
        }
    }
}

//-----------------------------------------
int main(int argc, char** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    const char* filter = (argc > 1) ? argv[1] : nullptr; // substring of a test name

    int failed_tests = 0, run_tests = 0;
    for (const TestCase& t : registry()) {
        if (filter && !strstr(t.name, filter))
            continue;
        ++run_tests;
        int before = g_failures;
        try {
            t.fn();
        } catch (const std::exception& e) {
            fprintf(stderr, "  unexpected exception: %s\n", e.what());
            ++g_failures;
        }
        bool ok = (g_failures == before);
        failed_tests += !ok;
        printf("[%s] %s\n", ok ? " OK " : "FAIL", t.name);
    }
    printf("\n%d tests, %d failed\n", run_tests, failed_tests);
    return (failed_tests || !run_tests) ? 1 : 0;
}
