#include "stdafx.h"
#include <stdexcept>
#include <string>

/*
 * Date:   04.04.2015
 * Author: Kirill Golikov
 *
 * Technosphere (Mail.Ru Group).
 * Course: Multithreaded programming in C++
 *
 * Task:
 * An allocator and a smart pointer class
 * with memory defragmentation,
 * WITHOUT ANY EXTRA MEMORY.
 *
 * A real doubly linked list stored in the managed memory,
 * its nodes are kept at the end of the memory without holes:
 *     + add and remove (O(1))
 *     + defragmentation (O(n*k))
 *     + reallocation / growth (O(n*l))
 * where
 *     n - the number of memory blocks
 *     k - the average block size in bytes
 *     l - the size of the given block
 *
 * A smart pointer corrects its misses when the list node holding
 * the block information moves, by "OPEN ADDRESSING" (O(n)).
 */

class Allocator;
enum AllocErrorType
{
    InvalidFree,
    NoMemory
};
class AllocError : std::runtime_error
{
private:
    AllocErrorType type_;

public:
    AllocError(AllocErrorType type, std::string message) : runtime_error(message), type_(type) {}

    AllocErrorType getType() const { return type_; }
};

// #define DBG
#ifdef DBG
/* Debug info */
extern FILE* log_file;
#endif

class Pointer;
class Allocator
{
    //-----------------------------------------
    struct Block
    { //---data----------
        uchar_t* pointer_;
        uint_t size_;
        uint_t index_;
        //---list----------
        Block* prev_;
        Block* next_;
        //-----------------
    };
    //-----------------------------------------
    AllocError err_;  /* to avoid the call new */
    uchar_t* memory_; /* available memory      */
    uint_t size_;     /* size of memory        */
    //-----------------------------------------      */
    uint_t blocks_count_; /* quantity of blocks    */
    Block* first_block_;
    Block* last_block_;
    uint_t max_index_; /* to detect collisions and make corrections of smart pointers */
    //-----------------------------------------
    Block* new_block(uint_t size, uchar_t* ptr, Block* prev, Block* next)
    {
        Block* block = --last_block_;
        /* Block always appears in the place of last one */

        /* Fill the Block structure */
        block->pointer_ = ptr;
        block->index_ = ++max_index_;
        block->size_ = size;
        block->prev_ = prev;
        block->next_ = next;

        ++blocks_count_;
        return block;
    }
    Block* put_block(uint_t size)
    {
        Block* curr = first_block_->next_;
        if (!curr) { /* Checking free head */
            curr = new_block(size, memory_, first_block_, NULL);
            first_block_->next_ = curr;
            return curr;
        }

        Block* next = curr->next_;
        while (next) {
            uchar_t* ptr = (curr->pointer_ + curr->size_);
            /* first fit */
            if ((uint_t)(next->pointer_ - ptr) >= size) {
                Block* block = new_block(size, ptr, curr, next);
                next->prev_ = curr->next_ = block;
                return block;
            }
            curr = next;
            next = next->next_;
        }

        /* Checking free tail */
        uchar_t* tail = (curr->pointer_ + curr->size_);
        /* -1U: reserve the place for one more descriptor */
        if ((uint_t)(((uchar_t*)(last_block_ - 1U)) - tail) >= size) {
            Block* block = new_block(size, tail, curr, NULL);
            curr->next_ = block;
            return block;
        }

        return NULL;
    }
    int del_block(Block* block)
    {
        if (!block || blocks_count_ <= 1)
            return 1;

        /* keep the max index small while the list is short */
        if (block->index_ == max_index_)
            max_index_ = block->prev_->index_;

#ifdef DBG1
        /* Debug info */
        fprintf(log_file, "\nthis: sz=%04d  inx=%02u  next=%02d  prev=%02d", block->size_,
                block->index_, (block->next_) ? (first_block_ - block->next_) : (0),
                (block->prev_) ? (first_block_ - block->prev_) : (0));
        if (block->next_)
            fprintf(log_file, "\nnext: sz=%04d  inx=%02u  next=%02d  prev=%02d",
                    block->next_->size_, block->next_->index_,
                    (block->next_->next_) ? (first_block_ - block->next_->next_) : (0),
                    (block->next_->prev_) ? (first_block_ - block->next_->prev_) : (0));
        if (block->prev_)
            fprintf(log_file, "\nprev: sz=%04d  inx=%02u  next=%02d  prev=%02d",
                    block->prev_->size_, block->prev_->index_,
                    (block->prev_->next_) ? (first_block_ - block->prev_->next_) : (0),
                    (block->prev_->prev_) ? (first_block_ - block->prev_->prev_) : (0));
#endif

        /* unlink the element from the list */
        if (block->next_)
            block->next_->prev_ = block->prev_;
        if (block->prev_)
            block->prev_->next_ = block->next_;

        /* if it is not a last element:
         * replace the last one into given,
         * to keep the list without holes in memory
         */
        if (block->index_ != last_block_->index_) {
            *block = *last_block_;

            if (block->prev_)
                block->prev_->next_ = block;
            if (block->next_)
                block->next_->prev_ = block;
        }
        /* remove the last element */
        memset(last_block_, 0, sizeof(Block));
        ++last_block_;
        --blocks_count_;

#ifdef DBG
        {
            /* Debug info */
            int i = 0;
            fprintf(log_file, "\nbcount=%u\n", blocks_count_);
            for (Block* curr = first_block_; curr >= last_block_; --curr) {
                fprintf(log_file, "%02d  ptr=%d  sz=%04d  inx=%02u  next=%02d   prev=%02d", ++i,
                        curr->pointer_, curr->size_, curr->index_,
                        (curr->next_) ? (first_block_ - curr->next_) : (-1),
                        (curr->prev_) ? (first_block_ - curr->prev_) : (-1));

                Block* end = (curr - 1);
                if (end >= last_block_)
                    fprintf(log_file, "  | shift=%d\n", (curr - end));
            }
            fprintf(log_file, "\nlb: inx=%02u  %x\n", last_block_->index_, last_block_);
        }
#endif

        return 0;
    }
    //-----------------------------------------
    std::string dump()
    {
        return "";
    }
    //-----------------------------------------
public:
    explicit Allocator(void* base, size_t size);
    Pointer alloc(size_t size);                // throw (...);
    void realloc(Pointer& p, size_t new_size); // throw (...);
    void free(Pointer& p);
    void defrag();
    //-----------------------------------------
    friend class Pointer;
};

class Pointer
{
protected:
    mutable Allocator::Block* block_;
    const Allocator::Block* first_;
    uint_t block_index_;
    //-----------------------------------------
    /* Correct of smart pointer's value */
    inline Allocator::Block* Block() const
    {
        if (!block_)
            return NULL;

        while (block_ != first_) {
#ifdef DBG
            /* Debug info */
            fprintf(log_file, "bl: inx=%02u  %x\n", block_->index_, block_);
#endif
            if (block_->index_ == block_index_)
                return block_;
            ++block_;
        }

        return NULL;
    }
    //-----------------------------------------
public:
    Pointer() : block_(NULL), first_(NULL), block_index_(0) {}
    Pointer(Allocator::Block* first) : block_(NULL), first_(first), block_index_(0) {}
    //-----------------------------------------
    void* get() const
    {
        block_ = Block();
        return (block_) ? block_->pointer_ : NULL;
    }
    //-----------------------------------------
    friend class Allocator;
};
