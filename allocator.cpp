#include "stdafx.h"
#include "allocator.h"
/*                                   v
 * +-------------+------------------------------------+
 * | EmptyHeader | Memory ... -->      <-- ... Blocks |
 * +-------------+------------------------------------+
 *                                   ^
 */
#ifdef DBG
FILE* log_file = stdout; // fopen ("log", "a+");
#endif
//-------------------------------------
namespace {
/* Largest request that still fits Block::size_ after rounding */
const size_t  kMaxRequest = UINT32_MAX / 2;
}
//-------------------------------------
          Allocator::Allocator (void *base, size_t size) :
                                       err_ (NoMemory, "AllocError"),
                                    memory_ (NULL),
                                      size_ (0U),
                              blocks_count_ (0U),
                               first_block_ (NULL),
                                last_block_ (NULL),
                                 max_index_ (0U)
 {
  uintptr_t begin = (uintptr_t) base;
  uintptr_t end   = begin + size;
  /* user blocks start aligned, descriptors are aligned at the top */
  uintptr_t data  = (begin + kAlign - 1) / kAlign * kAlign;
  if ( !base || end < begin || end - begin < sizeof (Block) )
   throw err_;
  uintptr_t head  = (end - sizeof (Block)) / alignof (Block) * alignof (Block);
  if ( head < data || head - data < kAlign )
   throw err_;

  memory_ = (uchar_t*) data;
  size_   = (size_t) (head + sizeof (Block) - data);
  std::memset (memory_, 0, size_);
  //--------------------------
  first_block_ = (Block*) head;

  first_block_->size_ = 0;
  first_block_->prev_ = NULL;
  first_block_->next_ = NULL;
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
  if ( !size || size > kMaxRequest ) throw err_;
  //--------------------------
  Block *block = put_block (round_up (size));
  if ( !block ) throw err_;
  //--------------------------
  Pointer pp (this);
  pp.block_       = block;
  pp.block_index_ = block->index_;
  //--------------------------
  return  pp;
 }
 //-------------------------------------
 void     Allocator::realloc   (Pointer &p, size_t new_size)
 {
  if ( !new_size )
  {
   free (p);
   return;
  }
  if ( new_size > kMaxRequest ) throw err_;
  //--------------------------
  Block *block = p.Block ();
  if ( !block )
  {
   p = alloc (new_size);
   return;
  }
  //--------------------------
  size_t  size = round_up (new_size);
  /* in place: up to the next block or up to the descriptors */
  uchar_t  *limit = (block->next_) ? block->next_->pointer_ : (uchar_t*) last_block_;
  if ( (size_t) (limit - block->pointer_) >= size )
  {
   block->size_ = (uint_t) size;
   return;
  }
  //--------------------------
  /* move: here size > block->size_ */
  Block *new_block = put_block (size);
  if ( !new_block ) throw err_;

  std::memcpy (new_block->pointer_, block->pointer_, block->size_);

  p.block_index_ = new_block->index_;
  p.owner_       = this;
  /* new_block is the last descriptor: del_block moves it into block's place */
  del_block (block);
  p.block_ = block;
 }
 //-------------------------------------
 void     Allocator::free      (Pointer &p)
 {
  //--------------------------
  del_block (p.Block ());
  p.owner_ = NULL;
  p.block_ = NULL;
  p.block_index_ = 0;
 }
 //-------------------------------------
 void     Allocator::defrag    ()
 {
  /* The list is sorted by data address: slide every block down */
  uchar_t  *end = memory_;
  for ( Block *curr = first_block_->next_; curr; curr = curr->next_ )
  {
   if ( curr->pointer_ != end )
   {
    std::memmove (end, curr->pointer_, curr->size_);
    curr->pointer_ = end;
   }
   end += curr->size_;
  }
 }
 //-------------------------------------
