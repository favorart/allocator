#include "allocator.h"

#include <cstdio>

/* A short tour: allocate, fragment, defragment, keep using the smart pointers. */
int  main ()
{
 alignas (std::max_align_t) static char arena[4096];
 Allocator a (arena, sizeof (arena));

 Pointer p[4];
 for ( int i = 0; i < 4; ++i )
 {
  p[i] = a.alloc (100);
  std::snprintf (static_cast<char*> (p[i].get ()), 100, "block #%d", i);
 }

 a.free (p[0]);
 a.free (p[2]);
 std::printf ("before defrag: p[1] at +%td, p[3] at +%td\n",
              static_cast<char*> (p[1].get ()) - arena,
              static_cast<char*> (p[3].get ()) - arena);

 a.defrag ();
 std::printf ("after  defrag: p[1] at +%td, p[3] at +%td\n",
              static_cast<char*> (p[1].get ()) - arena,
              static_cast<char*> (p[3].get ()) - arena);
 std::printf ("contents: \"%s\", \"%s\"\n",
              static_cast<char*> (p[1].get ()),
              static_cast<char*> (p[3].get ()));

 a.realloc (p[1], 1000);
 std::printf ("after realloc(1000): p[1] at +%td, still \"%s\"\n",
              static_cast<char*> (p[1].get ()) - arena,
              static_cast<char*> (p[1].get ()));

 a.free (p[1]);
 a.free (p[3]);
 return 0;
}
