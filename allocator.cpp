#include "stdafx.h"
#include "allocator.h"

/*                                   v
 * +-------------+------------------------------------+
 * | EmptyHeader | Memory ... -->      <-- ... Blocks |
 * +-------------+------------------------------------+
 *                                   ^
 */
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
  first_block_.size_ = 0;
  first_block_.left_ = NULL;
  first_block_.rght_ = NULL;
  first_block_.index_ = 0;
  first_block_.pointer_ = NULL;
  //--------------------------
  last_block_ = &first_block_;
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
  Pointer pp;
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
   }
   else throw err_;
  }
  else
  {
   if ( !(new_block = put_block (new_size)) )
    throw err_;
   p.block_       = new_block;
   p.block_index_ = new_block->index_;
  }
 }
 //-------------------------------------
 void     Allocator::free      (Pointer &p)
 {
  //--------------------------
  del_block (p.Block ());
  p.block_ = NULL;
  p.block_index_ = 0;
 }
 //-------------------------------------
 void     Allocator::defrag    ()
 {
  Block *rr = (Block*) (memory_ + size_ - sizeof (Block));
  FILE *f = fopen ("log.txt", "w");
  for ( uint_t i = 0U; i < blocks_count_; ++i )
  {
   fprintf (f, "\ni=%d ptr=%d  lft=%d  sz=%u  sg=%u",
            i, rr->pointer_, rr->leftptr_, rr->size_, rr->sign_);
   --rr;
  }
  fprintf (f, "\n");
  fclose (f);

  Block *last_block = (Block*) (memory_ + size_ - sizeof (Block) * blocks_count_);
  qsort (last_block, blocks_count_, sizeof (Block),
         [](const void *a, const void *b)
  {
   return // ( ((Block*) b)->pointer_ - ((Block*) a)->pointer_ );
    (((Block*) a)->pointer_ > ((Block*) b)->pointer_) ? -1 :
    (((Block*) a)->pointer_ < ((Block*) b)->pointer_) ? 1 : 0;
  });

  // moving memory
  uint_t shift_blocks = 1U, shift_memory = 0U;
  Block  *current = (Block*) (memory_ + size_ - sizeof (Block));
  for ( Block *next = current; current > last_block; current = next )
  {
   if ( current->sign_ == FREE ) // works only first time
   {
    if ( next > last_block )
    {
     next = current - 1;
     ++shift_blocks;
    }
    while ( next->sign_ == FREE && next > last_block )
    {
     ++shift_blocks;
     --next;
    }

    uint_t the_size = next->size_, the_count = 1U;
    Block *block = next - 1U;
    while ( block->sign_ != FREE && block > last_block )
    {
     the_size += block->size_;
     ++the_count;
     --block;
    }

    current->pointer_ -= shift_memory;
    if ( next != last_block )
    {
     std::memmove (current->pointer_, next->pointer_, the_size);
    }
    shift_memory += (next->pointer_ - current->pointer_);

    Block *bn = next;
    uint_t j = the_count;
    while ( j-- )
    {
     bn->pointer_ -= (next->pointer_ - current->pointer_);
     bn->leftptr_ -= (next->pointer_ - current->pointer_);
     *bn->ppointer_ = bn->pointer_;
     --bn;
    }

    // move the next itself
    std::memmove (next + shift_blocks, next, sizeof (Block) * the_count);
    next = block;
   } // end if
   else
   {
    *current->ppointer_ = current->pointer_;
    --next;
   }
  } // end for
  blocks_count_ -= shift_blocks;
  last_block->pointer_ -= shift_memory;
  last_block->size_ += (shift_memory + shift_blocks * sizeof (Block));

  rr = (Block*) (memory_ + size_ - sizeof (Block));
  f = fopen ("log.txt", "a+");
  for ( uint_t i = 0U; i < blocks_count_; ++i )
  {
   fprintf (f, "\ni=%d ptr=%d  lft=%d  sz=%u  sg=%u",
            i, rr->pointer_, rr->leftptr_, rr->size_, rr->sign_);
   --rr;
  }
  fprintf (f, "\n");
  fclose (f);
 }
 //-------------------------------------
