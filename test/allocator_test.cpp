#include "allocator.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <set>
#include <vector>

/*
 * The course's original test suite (it was written for GoogleTest).
 * It runs on a tiny self-contained harness, so the project has no
 * dependencies.
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
