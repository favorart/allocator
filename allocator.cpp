#include "stdafx.h"
#include "allocator.h"

/*                                   v
 * +-------------+------------------------------------+
 * | EmptyHeader | Memory ... -->      <-- ... Blocks |
 * +-------------+------------------------------------+
 *                                   ^
 */
 //-----------------------------------------
              Allocator::Allocator (void *base, size_t size) :
                                    memory_ ((uchar_t*)base),
                                      size_ (size),
                                       err_ (NoMemory, "AllocError")
 {
  std::memset (memory_, 0, size_);
  //--------------------------
  Block *block = (Block*) (memory_ + size - sizeof (Block));
  
  block->size_ = size_ - 2 * sizeof (Block),
  block->sign_ = FREE,
  block->leftptr_ = NULL;
  block->pointer_ = memory_;
  block->ppointer_ = NULL;
  //--------------------------
  ++blocks_count_;
 }
 //-----------------------------------------
 Pointer      Allocator::alloc     (size_t size)
 {
  if ( !size ) throw err_;
  //--------------------------
  Block *block = put_block (size);
  if ( !block )
   throw err_;
  //--------------------------
  Pointer pp;
  pp.pointer_ = (void*) block->pointer_;
  block->ppointer_ = &pp.pointer_;
  //--------------------------
  return  pp;
 }
 //-----------------------------------------
 void         Allocator::realloc   (Pointer &p, size_t new_size)
 {
  Block *lastb = NULL;
  if ( p.get () )
  {
   lastb = get_block ((uchar_t*) p.get ());
   if ( !lastb )
    throw err_;
   //--------------------------
   Block *rght_block = get_block (lastb->pointer_ + lastb->size_);
   if ( rght_block->sign_ == FREE && rght_block->size_ >= new_size )
   {
    
    rght_block->pointer_ += (new_size - lastb->size_);
    rght_block->size_ -= (new_size - lastb->size_);
    lastb->size_ += (new_size - lastb->size_);

    Block *rr = get_block (rght_block->pointer_ + rght_block->size_);
    if ( rr )
    {
     rr->leftptr_ = rght_block->pointer_;
    }
    return;
   }
  }  
  
  Block *block = put_block (new_size);
  if ( !block )
   throw err_;

  if ( p.get () )
  {
   //--------------------------
   std::memcpy (block->pointer_, lastb->pointer_, lastb->size_);
   //--------------------------
   del_block (lastb->pointer_);
  }
  p.pointer_ = (void*) block->pointer_;
  block->ppointer_ = &p.pointer_;
 }
 //-----------------------------------------
 void         Allocator::free      (Pointer &p)
 {
  //--------------------------
  del_block ((uchar_t*) p.get ());
  p.pointer_ = NULL;
 }
 //-----------------------------------------
 std::string  Allocator::dump      ()
 { return ""; }
 //-----------------------------------------


