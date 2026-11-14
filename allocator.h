#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

/*
 * Дата:  04.04.2015
 * Автор: Голиков Кирилл
 *
 * Техносфера mail.ru.
 * Курс: Многопоточное программрование на C++
 *
 * Задание:
 * Реализация аллокатора и класса умного указателя
 * с поддержкой дефрагментации памяти
 * БЕЗ ДОПОЛНИТЕЛЬНОЙ ПАМЯТИ
 *
 * Честная реализация двунаправленного списка в памяти,
 * хранение элементов списка в памяти с конца без дырок:
 *     + добавление и удаление (сложность O(1))
 *     + дефрагментация (сложность O(n*k))
 *     + перевыделение/расширение (сложность O(n*l))
 * , где
 *     n - количество блоков памяти
 *     k - средняя длина блока памяти в байтах
 *     l - длинна данного куска памяти
 *
 * Коррекция промахов умного указателя при изменении
 * положения элемента списка, содержащего иноформацию
 * о блоке памяти методом "ОТКРЫТОЙ АДРЕСАЦИИ" (сложность O(n))
 *
 * (English overview, layout diagram and the 2026 review notes: README.md)
 */

class Allocator;
enum  AllocErrorType
{ InvalidFree, NoMemory };
class AllocError : public std::runtime_error
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

// #define DBG
#ifdef  DBG
/* Debug info */
#include <cstdio>
extern  FILE  *log_file;
#endif

class Pointer;
class Allocator
{
 //-----------------------------------------
 struct Block
 { //---data----------
   unsigned char  *pointer_;
   std::uint32_t       size_;
   std::uint32_t      index_;
   //---list----------
   Block       *prev_;
   Block       *next_;
   //-----------------
 };
 /* Every user block starts at this alignment (like malloc) */
 static const size_t  kAlign = alignof (std::max_align_t);
 //-----------------------------------------
 AllocError        err_; /* to avoid the call new */
 unsigned char       *memory_; /* available memory      */
 size_t           size_; /* size of memory        */
 //-----------------------------------------      */
 std::uint32_t   blocks_count_; /* quantity of blocks    */
 Block    *first_block_; /* list head, never moves, highest descriptor */
 Block     *last_block_; /* lowest descriptor     */
 std::uint32_t      max_index_; /* to detect collisions and make corrections of smart pointers */
 //-----------------------------------------
 static size_t  round_up (size_t size)
 { return (size + kAlign - 1) / kAlign * kAlign; }
 /* Free bytes between the end of the data and the descriptors */
 size_t  tail_room (const unsigned char *data_end) const
 { return (size_t) ((const unsigned char*) last_block_ - data_end); }
 /* End of the data of the block, or of the arena head for the list head */
 unsigned char*  end_of (const Block *block) const
 { return (block == first_block_) ? memory_ : (block->pointer_ + block->size_); }
 /* The last block of the list: the one with the highest data address */
 Block*  tail_block () const
 {
  Block *curr = first_block_;
  while ( curr->next_ )
   curr = curr->next_;
  return curr;
 }
 //-----------------------------------------
 Block*  new_block (std::uint32_t size, unsigned char *ptr, Block *prev, Block *next)
 {
  Block* block = --last_block_;
  /* Block always appears in the place of last one */

  /* Fill the Block structure */
  block->pointer_ = ptr;
  /* Indices only grow: a smart pointer finds its block by the index,
   * so two live blocks must never share one (0 is the list head).  */
  if ( ++max_index_ == 0 )
   ++max_index_;
  block->index_   = max_index_;
  block->size_    = size;
  block->prev_    = prev;
  block->next_    = next;

  if ( prev ) prev->next_ = block;
  if ( next ) next->prev_ = block;

  ++blocks_count_;
  return block;
 }
 /* size must be already rounded up */
 Block*  put_block (size_t size)
 {
  Block *tail = tail_block ();
  /* Any new block needs room for one more descriptor */
  if ( tail_room (end_of (tail)) < sizeof (Block) )
   return NULL;

  /* Realization of first_fit (the gap before the first block counts too) */
  for ( Block *curr = first_block_; curr->next_; curr = curr->next_ )
  {
   unsigned char  *ptr = end_of (curr);
   if ( (size_t) (curr->next_->pointer_ - ptr) >= size )
    return new_block ((std::uint32_t) size, ptr, curr, curr->next_);
  }

  /* Checking free tail */
  if ( tail_room (end_of (tail)) - sizeof (Block) >= size )
   return new_block ((std::uint32_t) size, end_of (tail), tail, NULL);

  return NULL;
 }
 int     del_block (Block *block)
 {
  if ( !block || block == first_block_ )
   return 1;

  /* prick the element out of list */
  if ( block->next_ )
   block->next_->prev_ = block->prev_;
  if ( block->prev_ )
   block->prev_->next_ = block->next_;

  /* if it is not a last element:
   * replace the last one into given,
   * to keep the list without holes in memory
   * (descriptors only move up, towards first_block_)
   */
  if ( block != last_block_ )
  {
   *block = *last_block_;

   if ( block->prev_ )
    block->prev_->next_ = block;
   if ( block->next_ )
    block->next_->prev_ = block;
  }
  /* remove the last element */
  std::memset (static_cast<void*> (last_block_), 0, sizeof (Block));
  ++last_block_;
  --blocks_count_;

#ifdef  DBG
  {
   /* Debug info */
   int i = 0;
   std::fprintf (log_file, "\nbcount=%u\n", blocks_count_);
   for ( Block *curr = first_block_; curr >= last_block_; --curr )
    std::fprintf (log_file, "%02d  ptr=%p  sz=%04u  inx=%02u\n",
                  ++i, (void*) curr->pointer_, curr->size_, curr->index_);
  }
#endif

   return 0;
 }
 //-----------------------------------------
public:
 /* Manages [base, base + size). Throws AllocError(NoMemory) if it is too small. */
 explicit  Allocator (void  *base, size_t size);
 Pointer       alloc (size_t size);                 // throw (AllocError);
 void        realloc (Pointer &p, size_t new_size); // throw (AllocError); 0 means free
 void           free (Pointer &p);
 void         defrag ();
 //-----------------------------------------
 friend class Pointer;
};

class Pointer
{
protected:
 mutable Allocator::Block  *block_;
 const   Allocator         *owner_;
 std::uint32_t               block_index_;
 //-----------------------------------------
 /* Correct of smart pointer's value:
  * a descriptor can only move towards the list head,
  * so search upwards for the remembered index.
  */
 inline Allocator::Block*  Block () const
 {
  if ( !block_ || !owner_ )
   return NULL;

  /* Slots below last_block_ are free and may already hold user data */
  if ( block_ < owner_->last_block_ )
   block_ = owner_->last_block_;

  while ( block_ != owner_->first_block_ )
  {
#ifdef  DBG
   /* Debug info */
   std::fprintf (log_file, "bl: inx=%02u  %p\n", block_->index_, (void*) block_);
#endif
   if ( block_->index_ == block_index_ )
    return block_;
   ++block_;
  }

  block_ = NULL;
  return NULL;
 }
 //-----------------------------------------
public:
 Pointer ()                             : block_ (NULL), owner_ (NULL ), block_index_ (0) {}
 explicit Pointer (const Allocator *owner) : block_ (NULL), owner_ (owner), block_index_ (0) {}
 //-----------------------------------------
 void*  get () const
 {
  block_ = Block ();
  return (block_) ? block_->pointer_ : NULL;
 }
 //-----------------------------------------
 friend class Allocator;
};
