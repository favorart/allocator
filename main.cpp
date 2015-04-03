#include "stdafx.h"
#include "allocator.h"

void AllocInRange ();
void AllocReadWrite ();
void AllocNoMem ();
void AllocReuse ();
void DefragMove ();
void DefragMoveTwice ();
void DefragAvailable ();
void ReallocFromEmpty ();
void ReallocGrowInplace ();
void ReallocShrink ();
void ReallocGrow ();

int  main (int argc, char **argv)
{
 AllocInRange ();
 AllocReadWrite ();
 AllocNoMem ();
 AllocReuse ();
 DefragMove ();
 DefragMoveTwice ();
 DefragAvailable ();
 ReallocFromEmpty ();
 ReallocGrowInplace ();
 ReallocShrink ();
 ReallocGrow ();

	return 0;
}