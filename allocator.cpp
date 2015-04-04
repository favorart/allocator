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
  if ( !size ) throw err_;
  //--------------------------
  Block *block = put_block (size);
  if ( !block ) throw err_;
  //--------------------------
  Pointer pp (first_block_);
  pp.block_       = block;
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
   uchar_t  *ptr = (block->pointer_ + block->size_);

   if ( ( block->next_ &&  (size_t) ( block->next_->pointer_ - ptr) >= new_size )
     || (!block->next_ && ((size_t) ( (uchar_t*) last_block_ - ptr) >= new_size)) )
   {
    block->size_ = new_size;
    return;
   }
   else if ( (new_block = put_block (new_size)) )
   {
    p.block_index_ = new_block->index_;
    p.first_ = first_block_;

    std::memcpy (new_block->pointer_, block->pointer_, block->size_);

    del_block (block);
    p.block_ = block;
    return;
   }   
  }
  
  if ( (new_block = put_block (new_size)) )
  {
   p.block_       = new_block;
   p.block_index_ = new_block->index_;
   p.first_       = first_block_;
   return;
  }
  throw err_;
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

  curr = first_block_->next_;
  next =         curr->next_;

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
   next = next->next_;
  }
 }
 //-------------------------------------
