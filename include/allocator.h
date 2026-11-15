#pragma once

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

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

enum AllocErrorType
{
    InvalidFree,
    NoMemory
};

class AllocError : std::runtime_error
{
public:
    AllocError(AllocErrorType type, std::string message) : runtime_error(message), type_(type) {}

    AllocErrorType getType() const { return type_; }

private:
    AllocErrorType type_;
};

class Pointer;

class Allocator
{
public:
    explicit Allocator(void* base, std::size_t size);
    Pointer alloc(std::size_t size);
    void realloc(Pointer& p, std::size_t new_size);
    void free(Pointer& p);
    void defrag();

private:
    friend class Pointer;

    struct Block
    {
        unsigned char* pointer_;
        std::uint32_t size_;
        std::uint32_t index_;
        Block* prev_;
        Block* next_;
    };

    /* every user block starts at this alignment, like malloc() */
    static constexpr std::size_t kAlign = alignof(std::max_align_t);

    static std::size_t round_up(std::size_t size) { return (size + kAlign - 1) / kAlign * kAlign; }

    /* free bytes between the end of the data and the descriptors */
    std::size_t tail_room(const unsigned char* data_end) const
    {
        return static_cast<std::size_t>(reinterpret_cast<const unsigned char*>(last_block_) -
                                        data_end);
    }

    /* end of the block's data; for the list head, the start of the buffer */
    unsigned char* end_of(const Block* block) const
    {
        return (block == first_block_) ? memory_ : block->pointer_ + block->size_;
    }

    Block* tail_block() const;
    Block* new_block(std::uint32_t size, unsigned char* ptr, Block* prev, Block* next);
    Block* put_block(std::size_t size);
    int del_block(Block* block);

    AllocError err_;             /* to avoid the call new */
    unsigned char* memory_;      /* start of the data area (aligned) */
    std::size_t size_;           /* size of the managed area */
    std::uint32_t blocks_count_; /* quantity of blocks */
    Block* first_block_;
    Block* last_block_;
    std::uint32_t max_index_; /* to detect collisions and make corrections of smart pointers */
};

class Pointer
{
public:
    Pointer() : block_(nullptr), first_(nullptr), block_index_(0) {}
    Pointer(Allocator::Block* first) : block_(nullptr), first_(first), block_index_(0) {}

    void* get() const
    {
        block_ = Block();
        return (block_) ? block_->pointer_ : nullptr;
    }

private:
    friend class Allocator;

    /* Correction of the smart pointer value */
    Allocator::Block* Block() const
    {
        if (!block_)
            return nullptr;

        while (block_ != first_) {
            if (block_->index_ == block_index_)
                return block_;
            ++block_;
        }
        return nullptr;
    }

    mutable Allocator::Block* block_;
    const Allocator::Block* first_;
    std::uint32_t block_index_;
};
