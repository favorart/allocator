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

extern FILE* log_file;


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
 Block    *first_block_; // v
 Block     *last_block_; // v
 uint_t      max_index_; // v
 //-----------------------------------------
 Block*  new_block (uint_t size, uchar_t *ptr, Block *left, Block *rght)
 {
  Block* block = --last_block_;

  block->pointer_ = ptr;  
  block->index_   = ++max_index_;
  block->size_    = size;
  block->left_    = left;
  block->rght_    = rght;

  ++blocks_count_;
  return block;
 }
 Block*  put_block (uint_t size)
 {
  Block   *next = NULL, *curr = first_block_->rght_;
  uchar_t *tail = NULL;

  if ( !curr )
  { /* Checking free head */
    curr = new_block (size, memory_, first_block_, NULL);
    first_block_->rght_ = curr;
    return curr;
  }

  next = curr->rght_;
  if ( next )
  {
   while ( next )
   { /* Realization of first_fit */
    if ( (next->pointer_ - (curr->pointer_ + curr->size_)) >= size )
    {
     Block*  block = new_block (size, (curr->pointer_ + curr->size_), curr, next);
     next->left_ = curr->rght_ = block;
     return block;
    }
    curr = next;
    next = next->rght_;
   }
  }
  
  if( !next )
  {
   next = curr;
   tail = (next->pointer_ + next->size_);
  }

  /* Checking free tail */
  if ( (((uchar_t*) (last_block_ - 1U)) - tail) >= size )
  { /* -1U = bacause need to reserve the place for last block */
   Block*  block = new_block (size, tail, next, NULL);
   next->rght_ = block;
   return block;
  }

  return NULL;
 }
 int     del_block (Block* block)
 {
  if ( !block || blocks_count_ <= 1 )
   return 1;

  if ( block->index_ == max_index_ )
   max_index_ = block->left_->index_;
#ifdef DBG1
  printf ("\nblock: sz=%04d  inx=%02u  rght=%02d  left=%02d", block->size_,
          block->index_, (block->rght_) ? (first_block_ - block->rght_) : (0),
          (block->left_) ? (first_block_ - block->left_) : (0));
  if( block->rght_ )
   printf ("\nright: sz=%04d  inx=%02u  rght=%02d  left=%02d", block->rght_->size_,
          block->rght_->index_, (block->rght_->rght_) ? (first_block_ - block->rght_->rght_) : (0),
          (block->rght_->left_) ? (first_block_ - block->rght_->left_) : (0));
  if ( block->left_ )
   printf ("\nleft sz=%04d  inx=%02u  rght=%02d  left=%02d", block->left_->size_,
          block->left_->index_, (block->left_->rght_) ? (first_block_ - block->left_->rght_) : (0),
          (block->left_->left_) ? (first_block_ - block->left_->left_) : (0));
#endif
  if ( block->rght_ )
   block->rght_->left_ = block->left_;
  if ( block->left_ )
   block->left_->rght_ = block->rght_;

  int bytes = 0;
  if ( last_block_->index_ != block->index_ )
  {
   *block = *last_block_;

   if ( block->left_ )
    block->left_->rght_ = block;
   if ( block->rght_ )
    block->rght_->left_ = block;

#ifdef DBG1
   printf ("\npass:\n block: sz=%04d  inx=%02u  rght=%02d  left=%02d", block->size_,
           block->index_, (block->rght_) ? (first_block_ - block->rght_) : (0),
           (block->left_) ? (first_block_ - block->left_) : (0));
   if ( block->rght_ )
    printf ("\nright: sz=%04d  inx=%02u  rght=%02d  left=%02d", block->rght_->size_,
    block->rght_->index_, (block->rght_->rght_) ? (first_block_ - block->rght_->rght_) : (0),
    (block->rght_->left_) ? (first_block_ - block->rght_->left_) : (0));
   if ( block->left_ )
    printf ("\nleft sz=%04d  inx=%02u  rght=%02d  left=%02d", block->left_->size_,
    block->left_->index_, (block->left_->rght_) ? (first_block_ - block->left_->rght_) : (0),
    (block->left_->left_) ? (first_block_ - block->left_->left_) : (0));
#endif
  }
  else
  {
   // block->le
#ifdef DBG1
   printf ("\nblock: sz=%04d  inx=%02u  rght=%02d  left=%02d", block->size_,
           block->index_, (block->rght_) ? (first_block_ - block->rght_) : (0),
           (block->left_) ? (first_block_ - block->left_) : (0));
   if ( block->rght_ )
    printf ("\nright: sz=%04d  inx=%02u  rght=%02d  left=%02d", block->rght_->size_,
    block->rght_->index_, (block->rght_->rght_) ? (first_block_ - block->rght_->rght_) : (0),
    (block->rght_->left_) ? (first_block_ - block->rght_->left_) : (0));
   if ( block->left_ )
    printf ("\nleft sz=%04d  inx=%02u  rght=%02d  left=%02d", block->left_->size_,
    block->left_->index_, (block->left_->rght_) ? (first_block_ - block->left_->rght_) : (0),
    (block->left_->left_) ? (first_block_ - block->left_->left_) : (0));
#endif
   // memset (block, 0, sizeof (Block));

  }  
  ++last_block_;
  --blocks_count_;

#define DBG
#ifdef  DBG
  int i = 0;
  fprintf (log_file, "bcount=%u\n", blocks_count_);
  for ( Block *curr = first_block_; curr >= last_block_; --curr )
  {
   fprintf (log_file, "%02d ptr=%d  sz=%04d  inx=%02u  rght=%02d  left=%02d", ++i, curr->pointer_, curr->size_,
           curr->index_, (curr->rght_) ? (first_block_ - curr->rght_) : (0),
           (curr->left_) ? (first_block_ - curr->left_) : (0));
   Block *next = (curr - 1);
   if ( next >= last_block_ )
    fprintf (log_file, "  | shift=%d\n", (curr - next));
  }
  fprintf (log_file, "\n\n");
#endif
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
 const   Allocator::Block  *first_;
 uint_t               block_index_; 
 // void  **pointer_;

 inline Allocator::Block*  Block () const
 {
  if ( !block_ )
   return NULL;

  while ( block_ != first_ )
  {
   if ( block_->index_ == block_index_ )
    return block_;
   ++block_;
  }

  return NULL;
 }
 //-----------------------------------------
public:
 Pointer ()                        : block_ (NULL), first_ (NULL ), block_index_ (0) {}
 Pointer (Allocator::Block *first) : block_ (NULL), first_ (first), block_index_ (0) {}
 //-----------------------------------------
 void*  get () const
 {
  block_ = Block ();
  return (block_) ? block_->pointer_ : NULL;
 }
 //-----------------------------------------
 friend class Allocator;
};
