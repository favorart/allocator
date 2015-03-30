#include "stdafx.h"
#include "allocator.h"

#include <set>
#include <vector>
#include <iostream>

//#include "gtest/gtest.h"
using namespace std;
char  buf[65536];

int  myprint (char *err)
{
 printf (err);
 return 0;
}

// TEST (Allocator, AllocInRange)
void AllocInRange ()
{
 Allocator a (buf, sizeof (buf));
 int size = 500;

 Pointer p = a.alloc (size);
 char *v = reinterpret_cast<char*>(p.get ());

 if ( v > buf && (v + size) < (buf + sizeof (buf)) )
  myprint ("Error");

 a.free (p);
}

static void  writeTo      (Pointer   &p, size_t size)
{
 char *v = reinterpret_cast<char*>(p.get ());
 for ( uint_t i = 0; i < size; i++ )
  v[i] = i % 31;
}
static bool isDataOk      (Pointer   &p, size_t size)
{
 char *v = reinterpret_cast<char*>(p.get ());
 for ( uint_t i = 0; i < size; i++ )
 {
  if ( v[i] != i % 31 )
   return false;
 }
 return true;
}
static bool isValidMemory (Pointer   &p, size_t allocatedSize)
{
 char *v = reinterpret_cast<char*>(p.get ());
 return (v >= buf && v + allocatedSize < buf + sizeof (buf));
}
static bool fillUp        (Allocator &a, size_t allocSize, vector<Pointer> &out)
{
 int Max = (2 * sizeof (buf) / allocSize); // To avoid creating an infinite loop.

 for ( int i = 0; i < Max; i++ ) try
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

// TEST (Allocator, AllocReadWrite) 
void AllocReadWrite ()
{
 Allocator a (buf, sizeof (buf));

 vector<Pointer> ptr;
 size_t size = 300;
 for ( int i = 0; i < 20; i++ )
 {
  ptr.push_back (a.alloc (size));

  if ( !isValidMemory (ptr.back (), size) )
   myprint ("error");
  writeTo (ptr.back (), size);
 }

 // FILE *f = fopen ("fff.txt", "w");
 // int i = 0;
 // for ( Pointer &p : ptr )
 // {
 //  fmyprint (f, "\ni=%d ptr=%d", i++, p.get ());
 // }
 // fmyprint (f, "\n");
 // fclose (f);

 for ( Pointer &p : ptr )
 {
  if ( !isDataOk (p, size) )
   myprint ("error");
 }

 // f = fopen ("free.txt", "w");
 // int j = 0;
 for ( Pointer &p : ptr )
 // for ( int i = 0; i < 20; i++ )
 {
  // myprint ("\ni=%d ptr=%d", j, ptr[i].get ());
  // fmyprint (f, "\ni=%d p.get=%d" /*ptr=%d"*/, j, /*p.get (),*/ ptr[i].get ());
  // j++;
  a.free (p);
 }
 // fmyprint (f, "\n");
 // fclose (f);
}

// TEST (Allocator, AllocNoMem)
void AllocNoMem ()
{
 Allocator a (buf, sizeof (buf));
 size_t size = sizeof (buf) / 5;

 vector<Pointer> ptr;
 try
 {
  for ( int i = 0; i < 6; i++ )
  {
   ptr.push_back (a.alloc (size));
  }

  int i = 0;
  // EXPECT_TRUE(false);
 }
 catch ( AllocError &e )
 {
  if ( e.getType () != AllocErrorType::NoMemory )
   myprint ("Error");
 }

 for ( Pointer &p : ptr )
 {
  a.free (p);
 }
}

// TEST (Allocator, AllocReuse)
void AllocReuse ()
{
 Allocator a (buf, sizeof (buf));

 vector<Pointer> ptrs;
 int size = 135;

 if ( !fillUp (a, size, ptrs) )
   myprint ("Error");
 a.free (ptrs[1]);

 if ( ptrs[1].get () != nullptr )
  myprint ("Error");
 ptrs[1] = a.alloc (size);

 if ( ptrs[1].get () == nullptr )
  myprint ("Error");
 writeTo (ptrs[1], size);

 for ( Pointer &p : ptrs )
 {
  if ( !isDataOk (p, size) )
   myprint ("Error");
  a.free (p);
 }
}

// TEST (Allocator, DefragMove)
void DefragMove ()
{
 Allocator a (buf, sizeof (buf));

 set<void *> initialPtrs;
 vector<Pointer> ptrs;
 int size = 135;

 if ( !fillUp (a, size, ptrs) )
   myprint ("Error");
 a.free (ptrs[1]);
 a.free (ptrs[10]);
 a.free (ptrs[15]);

 ptrs.erase (ptrs.begin () + 15);
 ptrs.erase (ptrs.begin () + 10);
 ptrs.erase (ptrs.begin () + 1);

 for ( Pointer &p : ptrs )
 {
  auto r = initialPtrs.insert (p.get ());
  // Ensure inserted a new element.
  if ( !r.second )
     myprint ("Error");
 }

 a.defrag ();

 bool moved = false;
 for ( Pointer &p : ptrs )
 {
  if (!isDataOk (p, size))
     myprint ("Error");
  moved = (moved || initialPtrs.find (p.get ()) == initialPtrs.end ());
 }

 if ( !moved )
   myprint ("Error");

 for ( Pointer &p : ptrs )
 {
  if (!isDataOk (p, size) )
     myprint ("Error");
  a.free (p);
 }
}

// TEST (Allocator, DefragMoveTwice)
void DefragMoveTwice ()
{
 Allocator a (buf, sizeof (buf));

 vector<Pointer> ptrs;
 int size = 225;

 if ( !fillUp (a, size, ptrs) )
   myprint ("Error");

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
  if ( !isDataOk (p, size) )
   myprint ("Error");
  a.free (p);
 }
}

// TEST (Allocator, DefragAvailable)
void DefragAvailable ()
{
 Allocator a (buf, sizeof (buf));

 vector<Pointer> ptrs;
 int size = 135;

 if (!fillUp (a, size, ptrs))
   myprint ("Error");

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

  cerr << "WARNING: Allocator not fragmented initially. Defrag tests are inconclusive." << endl;
 }
 catch ( AllocError & ) {}

 a.defrag ();
 Pointer newPtr = a.alloc (size * 2);
 writeTo (newPtr, size * 2);

 for ( Pointer &p : ptrs )
 {
  if (!isDataOk (p, size) )
     myprint ("Error");
  a.free (p);
 }
}

// TEST (Allocator, ReallocFromEmpty)
void ReallocFromEmpty ()
{
 Allocator a (buf, sizeof (buf));

 int size = 81;

 Pointer p;
 Pointer p1 = a.alloc (size);

 a.realloc (p, size);
 if (p.get () == nullptr )
  myprint ("Error");

 Pointer p2 = a.alloc (size);

 writeTo (p, size);
 writeTo (p1, size);
 writeTo (p2, size);

 if (!isDataOk (p, size))
   myprint ("Error");
 if (!isDataOk (p1, size))
   myprint ("Error");
 if (!isDataOk (p2, size))
   myprint ("Error");

 a.free (p);
 a.free (p1);
 a.free (p2);
}

// TEST (Allocator, ReallocGrowInplace)
void ReallocGrowInplace ()
{
 Allocator a (buf, sizeof (buf));

 int size = 135;
 Pointer p = a.alloc (size);
 writeTo (p, size);

 void *ptr = p.get ();
 a.realloc (p, size * 2);

 if ( p.get () != ptr )
   myprint ("Error");
 if (!isDataOk (p, size) )
   myprint ("Error");

 Pointer p2 = a.alloc (size);
 writeTo (p, size * 2);
 writeTo (p2, size);

 if (!isDataOk (p, size * 2) )
   myprint ("Error");
 if (!isDataOk (p2, size) )
   myprint ("Error");

 a.free (p);
 a.free (p2);
}

// TEST (Allocator, ReallocShrink)
void ReallocShrink ()
{
 Allocator a (buf, sizeof (buf));

 int size = 135;
 Pointer p = a.alloc (size);
 writeTo (p, size);

 void *ptr = p.get ();
 a.realloc (p, size / 2);

 if ( p.get () != ptr )
   myprint ("Error");

 Pointer p2 = a.alloc (size);
 writeTo (p2, size);

 if ( !isDataOk (p, size / 2))
   myprint ("Error");

 if( !isDataOk (p2, size))
   myprint ("Error");

 a.free (p);
 a.free (p2);
}

// TEST (Allocator, ReallocGrow)
void ReallocGrow ()
{
 Allocator a (buf, sizeof (buf));

 int size = 135;
 Pointer p = a.alloc (size);
 Pointer p2 = a.alloc (size);

 writeTo (p, size);
 writeTo (p2, size);

 void *ptr = p.get ();
 a.realloc (p, size * 2);

 if ( p.get () == ptr )
 {
  cerr << "WARNING: Reallocated chunk was not moved. realloc() grow tests inconclusive." << endl;
 }

 if ( !isDataOk (p, size) )
  myprint ("Error");
 writeTo (p, size * 2);

 if ( !isDataOk (p, size * 2))
   myprint ("Error");
 if ( !isDataOk (p2, size))
   myprint ("Error");

 a.free (p);
 a.free (p2);
}
