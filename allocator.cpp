#include "stdafx.h"
#include "allocator.h"
/*                                   v
 * +-------------+------------------------------------+
 * | EmptyHeader | Memory ... -->      <-- ... Blocks |
 * +-------------+------------------------------------+
 *                                   ^
 */
FILE* log_file = fopen ("log", "a+");
 //-------------------------------------
          Allocator::Allocator (void *base, size_t size) :
                                    memory_ ((uchar_t*)base),
                                      size_ (size),
                                       err_ (NoMemory, "AllocError"),
                              blocks_count_ (0U),
                                 max_index_ (0U)
 {
  std::memset (memory_, 0, size_);
  //--------------------------
  first_block_ = (Block*) (memory_ + size_ - sizeof (Block));

  first_block_->size_ = 0;
  first_block_->left_ = NULL;
  first_block_->rght_ = NULL;
  first_block_->index_ = 0;
  first_block_->pointer_ = NULL;
  //--------------------------
  last_block_ = first_block_;
  //--------------------------
  ++blocks_count_;
 }
 //-------------------------------------
 Pointer  Allocator::alloc     (size_t size)
 {
  if ( !size ) throw err_;
  //--------------------------
  Block *block = put_block (size);
  if ( !block ) throw err_;
  //--------------------------
  Pointer pp (first_block_);
  pp.block_ = block;
  pp.block_index_ = block->index_;
  //--------------------------
  return  pp;
 }
 //-------------------------------------
 void     Allocator::realloc   (Pointer &p, size_t new_size)
 {
  Block *new_block = NULL, *block = p.Block ();
  if ( block )
  {
   if ( (block->rght_ && block->rght_->pointer_ - (block->pointer_ + block->size_) >= new_size)
       || (((uchar_t*) last_block_) - (block->pointer_ + block->size_) >= new_size) )
   {
    block->size_ = new_size;
    return;
   }
   else if ( (new_block = put_block (new_size)) )
   {
    del_block (block);
    p.block_       = new_block;
    p.block_index_ = new_block->index_;
    p.first_       = first_block_;
   }
   else throw err_;
  }
  else
  {
   if ( !(new_block = put_block (new_size)) )
    throw err_;
   p.block_       = new_block;
   p.block_index_ = new_block->index_;
   p.first_       = first_block_;
  }
 }
 //-------------------------------------
 void     Allocator::free      (Pointer &p)
 {
  //--------------------------
  del_block (p.Block ());
  p.first_ = NULL;
  p.block_ = NULL;
  p.block_index_ = 0;  
 }
 //-------------------------------------
 void     Allocator::defrag    ()
 {
  Block *curr, *next;

  curr = first_block_->rght_;
  next =         curr->rght_;

  int i = 0;
  while ( next >= last_block_  )
  {
   uchar_t  *ptr = (curr->pointer_ + curr->size_);
   if ( next->pointer_ != ptr )
   {
    std::memmove (ptr, next->pointer_, next->size_);
    next->pointer_ = ptr;
   }

#ifdef DBG
   fprintf (log_file, "\ni=%d ptr=%d  sz=%u", i++, curr->pointer_ - memory_, curr->size_);
#endif
   curr = next;
   next = next->rght_;
  }
#ifdef DBG
  // Block *rr = (Block*) (memory_ + size_ - sizeof (Block));
  // FILE *f = fopen ("log.txt", "w");
  // for ( uint_t i = 0U; i < blocks_count_; ++i )
  // { fprintf (f, "\ni=%d ptr=%d  lft=%d  sz=%u  sg=%u",
  //           i, rr->pointer_, rr->leftptr_, rr->size_, rr->sign_);
  //   --rr;
  // }
  // fprintf (f, "\n");
  // fclose (f);
#endif
 }
 //-------------------------------------
