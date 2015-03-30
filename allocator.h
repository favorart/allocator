#include "stdafx.h"
#include <stdexcept>
#include <string>

class Allocator;
enum  AllocErrorType
{ InvalidFree, NoMemory };
class AllocError: std::runtime_error
{
private:
 AllocErrorType type_;

public:
 AllocError (AllocErrorType type, std::string message):
             runtime_error  (message),
                      type_ (type)
 {}
 
 AllocErrorType  getType() const
 { return type_; }
};

class Pointer
{
 // protected:
 // void **pointer_;
 //-----------------------------------------
public:
 Pointer ():pointer_ (NULL) {}
 // explicit Pointer (void** p) : pointer_ (p) {}
 //          Pointer (Pointer& p) : pointer_ (p.pointer_) {}
 //-----------------------------------------
 void *pointer_;
 void*  get () const
 { return pointer_; } 
};

class Allocator
{
 //-----------------------------------------
 enum sign_t : uint_t
 { NONE, BUSY, FREE };
 //-----------------------------------------
 struct Block
 {
  uchar_t  *pointer_;
  sign_t       sign_;
  uint_t       size_;
  uchar_t  *leftptr_;
  //----------------
  void**   ppointer_;
 };
 //-----------------------------------------
 AllocError        err_;
 uint_t           size_;
 uchar_t       *memory_; // available memory
 uint_t   blocks_count_;
 //-----------------------------------------
 Block*  get_block (uint_t size, sign_t sign = FREE)
 {
  Block *current = (Block*) (memory_ + size_ - sizeof (Block));
  // realization of find_first
  for ( uint_t i = 0; i < blocks_count_; ++i )
  {
   if ( current->size_ >= size && current->sign_ == sign )
    return current;
   --current;
  }
  return NULL;
 }
 Block*  put_block (uint_t size, sign_t sign = BUSY)
 {
  Block *last_block = (Block*) (memory_ + size_ - blocks_count_ * sizeof (Block));
  Block *take_block =  get_block (size, FREE);

  if ( take_block )
  {
   if ( take_block == last_block  )
   {
    /* Bacause I resurve the place for last one in constructor */
    if( last_block->size_ < (size + sizeof (Block)) )
     take_block = NULL;
    else
    {
     ++blocks_count_;
     --last_block;

     last_block->leftptr_ = take_block->pointer_;
     last_block->pointer_ = take_block->pointer_ + size;
     last_block->sign_    = FREE;
     last_block->size_    = (take_block->size_ - (size + sizeof (Block)));

     take_block->sign_  = sign;
     take_block->size_  = size;
     // take_block->leftptr_ = 0;
     // take_block->pointer_
    }
   }
   else if ( take_block->size_ == size )
   {
    take_block->sign_ = sign;
   }
   else if ( last_block->size_ >= sizeof (Block) )
   {
    Block *free_block = last_block--;    

    *last_block = *free_block;
    *free_block = *take_block;

      last_block->size_ -= sizeof (Block);
    ++blocks_count_;

    // free_block->ileft_
    // free_block->index_
    // free_block->pointer_
    take_block->size_ = size;
    take_block->sign_ = sign;
    
    free_block->pointer_ = take_block->pointer_ + size;
    free_block->leftptr_ = take_block->pointer_;
    free_block->sign_    = FREE;
    free_block->size_   -= size;
   }
   else
    take_block = NULL;
  }
  return take_block;
 }
 Block*  get_block (uchar_t*  pointer_)
 {
  Block *current = (Block*) (memory_ + size_ - sizeof (Block));
  for ( uint_t i = 0U; i < blocks_count_; ++i )
  {
   if ( current->pointer_ == pointer_ )
    return current;
   --current;
  }
  return NULL;
 }
 int     del_block (uchar_t*  pointer_)
 {
  Block *btofree;
  Block *current = get_block (pointer_);
  Block *last_block = (Block*) (memory_ + size_ - sizeof (Block) * blocks_count_);

  // Block *rr = (Block*) (memory_ + size_ - sizeof (Block));
  // for ( uint_t i = 0U; i < blocks_count_; ++i )
  // {
  //  printf ("\nptr=%d  lft=%d  sz=%u  sg=%u", rr->pointer_, rr->leftptr_, rr->size_, rr->sign_);
  //  --rr;
  // }
  // printf ("\n");

  if ( current )
  {
   current->sign_ = FREE;

   // check neighbourhood
   if ( current->leftptr_ )
   {
    Block *left_block = get_block (current->leftptr_);
    if ( left_block->sign_ == FREE )
    {
     left_block->size_ += current->size_;
     if ( left_block < current )
     {
      *current = *left_block;
       btofree =  left_block;
     }
     else
     {
      btofree = current;
      current = left_block;
     }
     
     last_block->size_ += sizeof (Block);
     std::memmove (last_block + 1U, last_block, (btofree - last_block) * sizeof (Block));
     ++last_block;
     for ( Block* block = last_block; block < last_block + (btofree - last_block); ++block )
      if( block->ppointer_ )
       *block->ppointer_ = block->pointer_;
     --blocks_count_;
    }
   }

   if ( current <= last_block )
    return -1;

   Block *rght_block = get_block (current->pointer_ + current->size_);
   if ( rght_block <= last_block )
   {
    if ( rght_block != last_block )
     printf ("Error");

    last_block->leftptr_ = current->leftptr_;
    last_block->pointer_ = current->pointer_;
    last_block->size_ += (sizeof (Block) + current->size_);
    std::memmove (last_block + 1U, last_block, sizeof (Block));
    ++last_block;
    --blocks_count_;
   }
   else // if ( rght_block > last_block )
   {
    rght_block->leftptr_ = current->pointer_;

    if ( rght_block->sign_ == FREE )
    {
     current->size_ += rght_block->size_;

     if ( current < rght_block )
     {
      *rght_block = *current;
      btofree = current;
      current = rght_block;
     }
     else
     {
      btofree = rght_block;
     }

     rght_block = get_block (current->pointer_ + current->size_);
     if ( rght_block )
      rght_block->leftptr_ = current->pointer_;

     last_block->size_ += sizeof (Block);
     std::memmove (last_block + 1U, last_block, (btofree - last_block) * sizeof (Block));
     ++last_block;
     for ( Block* block = last_block; block < last_block + (btofree - last_block); ++block )
      if ( block->ppointer_ )
       *block->ppointer_ = block->pointer_;
     --blocks_count_;
    }
   }

   // Block *rr = (Block*) (memory_ + size_ - sizeof (Block));
   // for ( uint_t i = 0U; i < blocks_count_; ++i )
   // {
   //  printf ("\nptr=%d  lft=%d  sz=%u  sg=%u", rr->pointer_, rr->leftptr_, rr->size_, rr->sign_);
   //  --rr;
   // }
   // printf ("\n");

   return 0;
  }

  return -1;
 }
 //-----------------------------------------
public:
 explicit  Allocator (void  *base, size_t size);
 Pointer       alloc (size_t size); // throw (...);
 void        realloc (Pointer &p, size_t new_size); // throw (...);
 void           free (Pointer &p);
 void         defrag ()
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
         { return // ( ((Block*) b)->pointer_ - ((Block*) a)->pointer_ );
                  (((Block*) a)->pointer_ > ((Block*) b)->pointer_) ? -1 :
                  (((Block*) a)->pointer_ < ((Block*) b)->pointer_) ?  1 : 0; });

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
      *bn->ppointer_ =    bn->pointer_;
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
  blocks_count_        -=  shift_blocks;
  last_block->pointer_ -=  shift_memory;
  last_block->size_    += (shift_memory + shift_blocks * sizeof (Block));

  rr = (Block*) (memory_ + size_ - sizeof (Block));
  f = fopen ("log.txt", "a+");
  for ( uint_t i = 0U; i < blocks_count_; ++i )
  {
   fprintf (f,"\ni=%d ptr=%d  lft=%d  sz=%u  sg=%u",
            i, rr->pointer_, rr->leftptr_, rr->size_, rr->sign_);
   --rr;
  }
  fprintf (f, "\n");
  fclose (f);
 }
 //-----------------------------------------
 std::string    dump ();
 //-----------------------------------------
};
