#include "allocator.h"

#include <cstring>

/*                                   v
 * +-------------+------------------------------------+
 * | EmptyHeader | Memory ... -->      <-- ... Blocks |
 * +-------------+------------------------------------+
 *                                   ^
 */

Allocator::Allocator(void* base, std::size_t size)
    : err_(NoMemory, "AllocError"), memory_((unsigned char*)base), size_(size), blocks_count_(0U),
      max_index_(0U)
{
    std::memset(memory_, 0, size_);

    first_block_ = (Block*)(memory_ + size_ - sizeof(Block));

    first_block_->size_ = 0;
    first_block_->prev_ = nullptr;
    first_block_->next_ = nullptr;
    first_block_->index_ = 0;
    first_block_->pointer_ = nullptr;

    last_block_ = first_block_;
    ++blocks_count_;
}

Pointer Allocator::alloc(std::size_t size)
{
    if (!size)
        throw err_;

    Block* block = put_block(size);
    if (!block)
        throw err_;

    Pointer pp(first_block_);
    pp.block_ = block;
    pp.block_index_ = block->index_;
    return pp;
}

void Allocator::realloc(Pointer& p, std::size_t new_size)
{
    Block *new_block = nullptr, *block = p.Block();

    if (block) {
        unsigned char* ptr = (block->pointer_ + block->size_);

        if ((block->next_ && (std::size_t)(block->next_->pointer_ - ptr) >= new_size) ||
            (!block->next_ && ((std::size_t)((unsigned char*)last_block_ - ptr) >= new_size))) {
            block->size_ = new_size;
            return;
        } else if ((new_block = put_block(new_size))) {
            p.block_index_ = new_block->index_;
            p.first_ = first_block_;

            std::memcpy(new_block->pointer_, block->pointer_, block->size_);

            del_block(block);
            p.block_ = block;
            return;
        }
    }

    if ((new_block = put_block(new_size))) {
        p.block_ = new_block;
        p.block_index_ = new_block->index_;
        p.first_ = first_block_;
        return;
    }
    throw err_;
}

void Allocator::free(Pointer& p)
{
    del_block(p.Block());
    p.first_ = nullptr;
    p.block_ = nullptr;
    p.block_index_ = 0;
}

void Allocator::defrag()
{
    Block *curr, *next;

    curr = first_block_->next_;
    next = curr->next_;

    while (next >= last_block_) {
        unsigned char* ptr = (curr->pointer_ + curr->size_);
        if (next->pointer_ != ptr) {
            std::memmove(ptr, next->pointer_, next->size_);
            next->pointer_ = ptr;
        }
        curr = next;
        next = next->next_;
    }
}

Allocator::Block* Allocator::new_block(std::uint32_t size, unsigned char* ptr, Block* prev,
                                       Block* next)
{
    /* Block always appears in the place of last one */
    Block* block = --last_block_;

    /* Fill the Block structure */
    block->pointer_ = ptr;
    block->index_ = ++max_index_;
    block->size_ = size;
    block->prev_ = prev;
    block->next_ = next;

    ++blocks_count_;
    return block;
}

Allocator::Block* Allocator::put_block(std::uint32_t size)
{
    Block* curr = first_block_->next_;
    if (!curr) { /* Checking free head */
        curr = new_block(size, memory_, first_block_, nullptr);
        first_block_->next_ = curr;
        return curr;
    }

    Block* next = curr->next_;
    while (next) {
        unsigned char* ptr = (curr->pointer_ + curr->size_);
        /* first fit */
        if ((std::uint32_t)(next->pointer_ - ptr) >= size) {
            Block* block = new_block(size, ptr, curr, next);
            next->prev_ = curr->next_ = block;
            return block;
        }
        curr = next;
        next = next->next_;
    }

    /* Checking free tail */
    unsigned char* tail = (curr->pointer_ + curr->size_);
    /* -1U: reserve the place for one more descriptor */
    if ((std::uint32_t)(((unsigned char*)(last_block_ - 1U)) - tail) >= size) {
        Block* block = new_block(size, tail, curr, nullptr);
        curr->next_ = block;
        return block;
    }

    return nullptr;
}

int Allocator::del_block(Block* block)
{
    if (!block || blocks_count_ <= 1)
        return 1;

    /* keep the max index small while the list is short */
    if (block->index_ == max_index_)
        max_index_ = block->prev_->index_;

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
    std::memset(last_block_, 0, sizeof(Block));
    ++last_block_;
    --blocks_count_;
    return 0;
}
