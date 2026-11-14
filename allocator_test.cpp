#include "stdafx.h"
#include "allocator.h"

#include <cstdio>
#include <cstddef>
#include <random>
#include <set>
#include <vector>
#include <iostream>

/*
 * The first group of tests is the course's original test suite
 * (it was written for GoogleTest). It runs on a tiny self-contained
 * harness, so the project has no dependencies.
 *
 * The second group are regression tests for bugs found during
 * the 2026 review, plus a randomized stress test against a model.
 */

using namespace std;

//-----------------------------------------
namespace {

struct TestCase { const char *name; void (*fn) (); };

vector<TestCase>& registry ()
{ static vector<TestCase> tests; return tests; }

int  g_failures = 0;

struct Registrar
{ Registrar (const char *name, void (*fn) ()) { registry ().push_back ({ name, fn }); } };

} // namespace

#define TEST(suite, name)                                               \
  static void suite##_##name ();                                        \
  static Registrar reg_##suite##_##name (#suite "." #name, suite##_##name); \
  static void suite##_##name ()

#define CHECK(cond)                                                     \
  do { if ( !(cond) ) {                                                 \
    fprintf (stderr, "  %s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
    ++g_failures; } } while ( 0 )

#define CHECK_THROWS(expr, errtype)                                     \
  do { bool thrown_ = false;                                            \
    try { expr; } catch ( AllocError &e_ ) { thrown_ = (e_.getType () == errtype); } \
    if ( !thrown_ ) {                                                   \
      fprintf (stderr, "  %s:%d: expected AllocError(%s): %s\n", __FILE__, __LINE__, #errtype, #expr); \
      ++g_failures; } } while ( 0 )

//-----------------------------------------
alignas (std::max_align_t) static char buf[65536];

static void  writeTo      (Pointer   &p, size_t size)
{
 char *v = reinterpret_cast<char*>(p.get ());
 for ( size_t i = 0; i < size; i++ )
  v[i] = static_cast<char> (i % 31);
}
static bool isDataOk      (Pointer   &p, size_t size)
{
 char *v = reinterpret_cast<char*>(p.get ());
 for ( size_t i = 0; i < size; i++ )
 {
  if ( v[i] != static_cast<char> (i % 31) )
   return false;
 }
 return true;
}
static bool isValidMemory (Pointer   &p, size_t allocatedSize)
{
 char *v = reinterpret_cast<char*>(p.get ());
 return (v >= buf && v + allocatedSize <= buf + sizeof (buf));
}
static bool fillUp        (Allocator &a, size_t allocSize, vector<Pointer> &out)
{
 size_t Max = (2 * sizeof (buf) / allocSize); // To avoid creating an infinite loop.

 for ( size_t i = 0; i < Max; i++ ) try
 {
  out.push_back (a.alloc (allocSize));
  writeTo (out.back (), allocSize);
 }
 catch ( AllocError & )
 {
  return true;
 }

 return false;
}

//=========================================
// Original course tests
//=========================================
TEST (Allocator, AllocInRange)
{
 Allocator a (buf, sizeof (buf));
 size_t size = 500;

 Pointer p = a.alloc (size);
 char *v = reinterpret_cast<char*>(p.get ());

 CHECK (v >= buf && (v + size) <= (buf + sizeof (buf)));

 a.free (p);
}

TEST (Allocator, AllocReadWrite)
{
 Allocator a (buf, sizeof (buf));

 vector<Pointer> ptr;
 size_t size = 300;
 for ( int i = 0; i < 20; i++ )
 {
  ptr.push_back (a.alloc (size));
  CHECK (isValidMemory (ptr.back (), size));
  writeTo (ptr.back (), size);
 }

 for ( Pointer &p : ptr )
  CHECK (isDataOk (p, size));

 for ( Pointer &p : ptr )
  a.free (p);
}

TEST (Allocator, AllocNoMem)
{
 Allocator a (buf, sizeof (buf));
 size_t size = sizeof (buf) / 5;

 vector<Pointer> ptr;
 bool thrown = false;
 try
 {
  for ( int i = 0; i < 6; i++ )
   ptr.push_back (a.alloc (size));
 }
 catch ( AllocError &e )
 {
  thrown = true;
  CHECK (e.getType () == AllocErrorType::NoMemory);
 }
 CHECK (thrown);

 for ( Pointer &p : ptr )
  a.free (p);
}

TEST (Allocator, AllocReuse)
{
 Allocator a (buf, sizeof (buf));

 vector<Pointer> ptrs;
 size_t size = 135;

 CHECK (fillUp (a, size, ptrs));
 a.free (ptrs[1]);

 CHECK (ptrs[1].get () == nullptr);
 ptrs[1] = a.alloc (size);

 CHECK (ptrs[1].get () != nullptr);
 writeTo (ptrs[1], size);

 for ( Pointer &p : ptrs )
 {
  CHECK (isDataOk (p, size));
  a.free (p);
 }
}

TEST (Allocator, DefragMove)
{
 Allocator a (buf, sizeof (buf));

 set<void *> initialPtrs;
 vector<Pointer> ptrs;
 size_t size = 135;

 CHECK (fillUp (a, size, ptrs));
 a.free (ptrs[1]);
 a.free (ptrs[10]);
 a.free (ptrs[15]);

 ptrs.erase (ptrs.begin () + 15);
 ptrs.erase (ptrs.begin () + 10);
 ptrs.erase (ptrs.begin () + 1);

 for ( Pointer &p : ptrs )
 {
  auto r = initialPtrs.insert (p.get ());
  CHECK (r.second); // Ensure inserted a new element.
 }

 a.defrag ();

 bool moved = false;
 for ( Pointer &p : ptrs )
 {
  CHECK (isDataOk (p, size));
  moved = (moved || initialPtrs.find (p.get ()) == initialPtrs.end ());
 }
 CHECK (moved);

 for ( Pointer &p : ptrs )
 {
  CHECK (isDataOk (p, size));
  a.free (p);
 }
}

TEST (Allocator, DefragMoveTwice)
{
 Allocator a (buf, sizeof (buf));

 vector<Pointer> ptrs;
 size_t size = 225;

 CHECK (fillUp (a, size, ptrs));

 a.free (ptrs[1]);
 a.free (ptrs[10]);
 ptrs.erase (ptrs.begin () + 10);
 ptrs.erase (ptrs.begin () + 1);

 a.defrag ();

 a.free (ptrs[15]);
 ptrs.erase (ptrs.begin () + 15);

 a.defrag ();

 ptrs.push_back (a.alloc (size));
 writeTo (ptrs.back (), size);

 for ( Pointer &p : ptrs )
 {
  CHECK (isDataOk (p, size));
  a.free (p);
 }
}

TEST (Allocator, DefragAvailable)
{
 Allocator a (buf, sizeof (buf));

 vector<Pointer> ptrs;
 size_t size = 135;

 CHECK (fillUp (a, size, ptrs));

 a.free (ptrs[1]);
 a.free (ptrs[10]);
 a.free (ptrs[15]);

 ptrs.erase (ptrs.begin () + 15);
 ptrs.erase (ptrs.begin () + 10);
 ptrs.erase (ptrs.begin () + 1);

 try
 {
  Pointer p = a.alloc (size * 2);
  a.free (p);

  cerr << "  WARNING: Allocator not fragmented initially. Defrag tests are inconclusive." << endl;
 }
 catch ( AllocError & ) {}

 a.defrag ();
 Pointer newPtr = a.alloc (size * 2);
 writeTo (newPtr, size * 2);

 for ( Pointer &p : ptrs )
 {
  CHECK (isDataOk (p, size));
  a.free (p);
 }
}

TEST (Allocator, ReallocFromEmpty)
{
 Allocator a (buf, sizeof (buf));

 size_t size = 81;

 Pointer p;
 Pointer p1 = a.alloc (size);

 a.realloc (p, size);
 CHECK (p.get () != nullptr);

 Pointer p2 = a.alloc (size);

 writeTo (p, size);
 writeTo (p1, size);
 writeTo (p2, size);

 CHECK (isDataOk (p, size));
 CHECK (isDataOk (p1, size));
 CHECK (isDataOk (p2, size));

 a.free (p);
 a.free (p1);
 a.free (p2);
}

TEST (Allocator, ReallocGrowInplace)
{
 Allocator a (buf, sizeof (buf));

 size_t size = 135;
 Pointer p = a.alloc (size);
 writeTo (p, size);

 void *ptr = p.get ();
 a.realloc (p, size * 2);

 CHECK (p.get () == ptr);
 CHECK (isDataOk (p, size));

 Pointer p2 = a.alloc (size);
 writeTo (p, size * 2);
 writeTo (p2, size);

 CHECK (isDataOk (p, size * 2));
 CHECK (isDataOk (p2, size));

 a.free (p);
 a.free (p2);
}

TEST (Allocator, ReallocShrink)
{
 Allocator a (buf, sizeof (buf));

 size_t size = 135;
 Pointer p = a.alloc (size);
 writeTo (p, size);

 void *ptr = p.get ();
 a.realloc (p, size / 2);

 CHECK (p.get () == ptr);

 Pointer p2 = a.alloc (size);
 writeTo (p2, size);

 CHECK (isDataOk (p, size / 2));
 CHECK (isDataOk (p2, size));

 a.free (p);
 a.free (p2);
}

TEST (Allocator, ReallocGrow)
{
 Allocator a (buf, sizeof (buf));

 size_t size = 135;
 Pointer p = a.alloc (size);
 Pointer p2 = a.alloc (size);

 writeTo (p, size);
 writeTo (p2, size);

 void *ptr = p.get ();
 a.realloc (p, size * 2);

 if ( p.get () == ptr )
  cerr << "  WARNING: Reallocated chunk was not moved. realloc() grow tests inconclusive." << endl;

 CHECK (isDataOk (p, size));
 writeTo (p, size * 2);

 CHECK (isDataOk (p, size * 2));
 CHECK (isDataOk (p2, size));

 a.free (p);
 a.free (p2);
}

//=========================================
// Regression tests (2026 review)
//=========================================

// alloc() on an empty allocator did not check the size at all.
TEST (Regression, AllocTooLargeOnEmpty)
{
 Allocator a (buf, 1024);
 CHECK_THROWS (a.alloc (4096), NoMemory);
 CHECK_THROWS (a.alloc (0), NoMemory);
}

// The free-tail check underflowed when data reached the block
// descriptors, so new descriptors were written over user data.
TEST (Regression, FillToTheBrim)
{
 for ( size_t size : { 1, 7, 16, 24, 40, 100 } )
 {
  Allocator a (buf, 2048);
  vector<Pointer> ptrs;
  CHECK (fillUp (a, size, ptrs));
  for ( Pointer &p : ptrs )
   CHECK (isDataOk (p, size));
 }
}

// A block placed into a gap also needs room for one more
// descriptor below the data tail.
TEST (Regression, GapNeedsDescriptorRoom)
{
 for ( size_t size : { 16, 32, 48 } )
 {
  Allocator a (buf, 2048);
  vector<Pointer> ptrs;
  CHECK (fillUp (a, size, ptrs));
  a.free (ptrs[0]);
  a.free (ptrs[2]);
  ptrs.erase (ptrs.begin () + 2);
  ptrs.erase (ptrs.begin ());

  // fill both gaps and whatever is left
  fillUp (a, size, ptrs);
  for ( Pointer &p : ptrs )
   CHECK (isDataOk (p, size));
 }
}

// Block indices were "reused" on free, so two live blocks could share
// an index and a smart pointer could follow the wrong descriptor.
TEST (Regression, IndicesStayUnique)
{
 Allocator a (buf, sizeof (buf));
 Pointer A = a.alloc (32), B = a.alloc (32), C = a.alloc (32);
 a.free (B);
 Pointer D = a.alloc (32);     // goes into B's gap
 a.free (D);                   // was: max_index_ dropped to A's index
 Pointer E = a.alloc (64);
 Pointer F = a.alloc (64);     // was: got the same index as C
 Pointer G = a.alloc (64);
 a.free (E);
 a.free (G);

 memset (C.get (), 'c', 32);
 memset (F.get (), 'f', 64);
 CHECK (C.get () != F.get ());
 CHECK (static_cast<char*> (C.get ())[0] == 'c');
 CHECK (static_cast<char*> (F.get ())[0] == 'f');
 (void) A;
}

// defrag() dereferenced a null pointer on an empty allocator
// and never moved the first block down to the start of the arena.
TEST (Regression, DefragEmptyAndHead)
{
 Allocator a (buf, sizeof (buf));
 a.defrag ();

 Pointer A = a.alloc (100);
 void *start = A.get ();
 Pointer B = a.alloc (100);
 writeTo (B, 100);
 a.free (A);
 a.defrag ();
 CHECK (B.get () == start);
 CHECK (isDataOk (B, 100));

 // the gap before the first block is reusable too
 a.free (B);
 Pointer C = a.alloc (100);
 CHECK (C.get () == start);
}

// Shrinking a block with an adjacent neighbour moved it and
// copied the old (larger) size into the new, smaller block.
TEST (Regression, ReallocShrinkWithNeighbour)
{
 Allocator a (buf, sizeof (buf));
 Pointer p  = a.alloc (256);
 Pointer p2 = a.alloc (256);
 writeTo (p, 256);
 writeTo (p2, 256);

 void *ptr = p.get ();
 a.realloc (p, 64);
 CHECK (p.get () == ptr);
 CHECK (isDataOk (p, 64));
 CHECK (isDataOk (p2, 256));
}

// Growing in place only needs (new_size - size) free bytes after the block.
TEST (Regression, ReallocGrowIntoExactGap)
{
 Allocator a (buf, sizeof (buf));
 Pointer p  = a.alloc (64);
 Pointer g  = a.alloc (64);
 Pointer p2 = a.alloc (64);
 writeTo (p, 64);
 a.free (g);

 void *ptr = p.get ();
 a.realloc (p, 128);
 CHECK (p.get () == ptr);
 CHECK (isDataOk (p, 64));
 (void) p2;
}

TEST (Regression, ReallocToZeroFrees)
{
 Allocator a (buf, sizeof (buf));
 Pointer p = a.alloc (64);
 a.realloc (p, 0);
 CHECK (p.get () == nullptr);
}

// Descriptors were placed at (base + size - sizeof (Block)) without
// alignment: misaligned access is UB (caught by UBSan).
TEST (Regression, UnalignedArena)
{
 Allocator a (buf + 1, 4093);
 vector<Pointer> ptrs;
 for ( size_t size : { 1, 3, 17, 33, 65 } )
 {
  ptrs.push_back (a.alloc (size));
  writeTo (ptrs.back (), size);
  CHECK (reinterpret_cast<uintptr_t> (ptrs.back ().get ()) % alignof (std::max_align_t) == 0);
 }
 a.free (ptrs[1]);
 a.free (ptrs[3]);
 a.defrag ();
 CHECK (isDataOk (ptrs[0], 1));
 CHECK (isDataOk (ptrs[2], 17));
 CHECK (isDataOk (ptrs[4], 65));
}

TEST (Regression, ArenaTooSmall)
{
 bool thrown = false;
 try { Allocator a (buf, 8); }
 catch ( AllocError &e ) { thrown = (e.getType () == NoMemory); }
 CHECK (thrown);
}

// AllocError inherited std::runtime_error privately.
TEST (Regression, ErrorIsStdException)
{
 Allocator a (buf, 1024);
 bool caught = false;
 try { a.alloc (1u << 20); }
 catch ( const std::exception & ) { caught = true; }
 CHECK (caught);
}

// A copy of a freed smart pointer must resolve to nullptr.
TEST (Regression, StaleCopyIsNull)
{
 Allocator a (buf, sizeof (buf));
 Pointer p = a.alloc (64);
 Pointer copy = p;
 a.free (p);
 CHECK (copy.get () == nullptr);
 Pointer q = a.alloc (64);
 CHECK (copy.get () == nullptr);
 (void) q;
}

// A smart pointer remembers the descriptor slot it saw last. After frees
// that slot may lie below the descriptor area and be overwritten by user
// data; the search must not read it as a descriptor.
TEST (Regression, CachedSlotReusedByData)
{
 Allocator a (buf, 1024);
 Pointer A = a.alloc (64), B = a.alloc (64), X = a.alloc (64); // X: lowest descriptor
 writeTo (X, 64);
 void *x = X.get ();
 a.free (A);   // X's descriptor moves up into A's slot
 a.free (B);   // X's cached slot is now two slots below the descriptor area

 // the largest block that still goes to the tail: it covers X's old slot
 Pointer big;
 size_t big_size = 1024;
 for ( ; big_size > 128; big_size -= 16 )
  try { big = a.alloc (big_size); break; } catch ( AllocError & ) {}
 CHECK (big.get () != nullptr);
 uint32_t *w = static_cast<uint32_t*> (big.get ());
 for ( size_t i = 0; i < big_size / sizeof (uint32_t); ++i )
  w[i] = 3;    // X's index everywhere

 CHECK (X.get () == x);
 CHECK (isDataOk (X, 64));
}

// Random alloc / free / realloc / defrag against a byte-exact model.
TEST (Stress, RandomOpsAgainstModel)
{
 struct Live { Pointer p; size_t size; unsigned char seed; };

 for ( unsigned run = 0; run < 20; ++run )
 {
  mt19937 rng (run);
  const size_t arena = 1024 + 512 * run;
  Allocator a (buf + (run % 8), arena);
  vector<Live> live;

  auto fill = [] (Live &l)
  {
   unsigned char *v = static_cast<unsigned char*> (l.p.get ());
   for ( size_t i = 0; i < l.size; ++i )
    v[i] = static_cast<unsigned char> (l.seed + i * 7);
  };
  auto ok = [] (Live &l, size_t n)
  {
   unsigned char *v = static_cast<unsigned char*> (l.p.get ());
   if ( !v ) return false;
   for ( size_t i = 0; i < n; ++i )
    if ( v[i] != static_cast<unsigned char> (l.seed + i * 7) )
     return false;
   return true;
  };

  for ( int step = 0; step < 3000; ++step )
  {
   unsigned op = rng () % 10;
   size_t size = 1 + rng () % (arena / 8);
   if ( op < 5 || live.empty () )
   {
    try
    {
     Live l { a.alloc (size), size, static_cast<unsigned char> (rng ()) };
     unsigned char *v = static_cast<unsigned char*> (l.p.get ());
     CHECK (v >= reinterpret_cast<unsigned char*> (buf)
            && v + size <= reinterpret_cast<unsigned char*> (buf) + run % 8 + arena);
     fill (l);
     live.push_back (l);
    }
    catch ( AllocError & ) {}
   }
   else if ( op < 8 )
   {
    size_t k = rng () % live.size ();
    a.free (live[k].p);
    live.erase (live.begin () + k);
   }
   else if ( op < 9 )
   {
    size_t k = rng () % live.size ();
    try
    {
     a.realloc (live[k].p, size);
     size_t kept = min (size, live[k].size);
     CHECK (ok (live[k], kept));
     live[k].size = size;
     fill (live[k]);
    }
    catch ( AllocError & ) { CHECK (ok (live[k], live[k].size)); }
   }
   else
    a.defrag ();

   for ( Live &l : live )
    if ( !ok (l, l.size) )
    {
     CHECK (!"live block corrupted");
     return;
    }
  }
 }
}

//-----------------------------------------
int  main (int argc, char **argv)
{
 setvbuf (stdout, nullptr, _IONBF, 0);
 const char *filter = (argc > 1) ? argv[1] : nullptr; // substring of a test name

 int failed_tests = 0, run_tests = 0;
 for ( const TestCase &t : registry () )
 {
  if ( filter && !strstr (t.name, filter) )
   continue;
  ++run_tests;
  int before = g_failures;
  try { t.fn (); }
  catch ( const std::exception &e )
  {
   fprintf (stderr, "  unexpected exception: %s\n", e.what ());
   ++g_failures;
  }
  bool ok = (g_failures == before);
  failed_tests += !ok;
  printf ("[%s] %s\n", ok ? " OK " : "FAIL", t.name);
 }
 printf ("\n%d tests, %d failed\n", run_tests, failed_tests);
 return (failed_tests || !run_tests) ? 1 : 0;
}
