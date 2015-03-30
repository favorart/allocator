#include "stdafx.h"
#include <stdexcept>
#include <string>

class Allocator;
enum  AllocErrorType
{ InvalidFree, NoMemory };
class AllocError : std::runtime_error
{
private:
 AllocErrorType type_;

public:
 AllocError (AllocErrorType type, std::string message) :
  runtime_error (message),
  type_ (type)
 {}

 AllocErrorType  getType () const
 { return type_; }
};

class Pointer;
class Allocator
{
 //-----------------------------------------
 struct Block
 { //----------------
   uchar_t  *pointer_;
   uint_t       size_;
   uint_t      index_;
   //----------------
   Block       *left_;
   Block       *rght_;
   //----------------
 };
 //-----------------------------------------
 AllocError        err_; // v
 uchar_t       *memory_; // v - available memory
 uint_t           size_; // v
 //-----------------------------------------
 uint_t   blocks_count_; // v
 Block     first_block_; // v
 Block     *last_block_; // v
 uint_t      max_index_; // v
 //-----------------------------------------
 Block*  new_block (uint_t size, uchar_t *ptr, Block* left, Block *rght)
 {
  Block* block = --last_block_;

  block->pointer_ = ptr;  
  block->index_   = max_index_++;
  block->size_    = size;
  block->left_    = left;
  block->rght_    = rght;

  ++blocks_count_;
  return block;
 }
 Block*  put_block (uint_t size)
 {
  Block *next, *curr = first_block_.rght_;
  if ( !curr )
  { /* Checking free head */
   return new_block (size, memory_, &first_block_, NULL);
  }

  next = curr->rght_;
  for ( uint_t i = 0U; i < (blocks_count_ - 1U); ++i )
  { /* Realization of first_fit */
   if ( (next->pointer_ - (curr->pointer_ + curr->size_)) >= size )
   {
    Block*  block = new_block (size, (curr->pointer_ + curr->size_), curr, next);
    next->left_ = curr->rght_ = block;
   }
   curr = next;
   next = next->rght_;
  }

  /* Checking free tail */
  if ( (((uchar_t*)(last_block_ - 1U)) - (next->pointer_ + next->size_)) >= size )
  { /* -1U = bacause need to reserve the place for last block */
   Block*  block = new_block (size, (next->pointer_ + next->size_), next, NULL);
   next->rght_ = block;
   return block;
  }
  return NULL;
 }
 int     del_block (Block* block)
 {
  if ( !block || blocks_count_ <= 1 )
   return 1;

#ifdef DBG
  for ( Block *curr = first_block_.rght_; curr >= last_block_; curr = curr->rght_ )
  {
   printf ("\ptr=%d  sz=%d  inx=%u\n", rr->pointer_, rr->size_, rr->index_);
  }
  printf ("\n");
#endif

  if ( block->index_ == max_index_ )
   --max_index_;

  uint_t  bytes = (block - last_block_) * sizeof (Block);
  std::memmove (last_block_ + 1U, last_block_, bytes);
  ++last_block_;

  return 0;
 }
 //-----------------------------------------
public:
 explicit  Allocator (void  *base, size_t size);
 Pointer       alloc (size_t size);                 // throw (...);
 void        realloc (Pointer &p, size_t new_size); // throw (...);
 void           free (Pointer &p);
 void         defrag ();
 //-----------------------------------------
 std::string    dump ()
 { return ""; }
 //-----------------------------------------
 friend class Pointer;
};

class Pointer
{
protected:
 mutable Allocator::Block  *block_;
 uint_t               block_index_;
 // void  **pointer_;

 Allocator::Block*  Block () const
 {
  if ( !block_ )
   return NULL;

  while ( block_->index_ != block_index_ )
   if ( !(++block_)->pointer_ )
   {
    return (block_ = NULL);
   }

  return block_;
 }
 //-----------------------------------------
public:
 Pointer () : block_ (NULL), block_index_ (0) {}
 //-----------------------------------------
 void*  get () const
 {
  Allocator::Block* b = Block ();
  return (b) ? b->pointer_ : NULL;
 }
 //-----------------------------------------
 friend class Allocator;
};
