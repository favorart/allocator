#include "allocator.h"

#include <cstring>

/*                                   v
 * +-------------+------------------------------------+
 * | EmptyHeader | Memory ... -->      <-- ... Blocks |
 * +-------------+------------------------------------+
 *                                   ^
 */

namespace {
/* the largest request that still fits Block::size_ after rounding up */
constexpr std::size_t kMaxRequest = UINT32_MAX / 2;
} // namespace

Allocator::Allocator(void* base, std::size_t size)
    : err_(NoMemory, "AllocError"), memory_(nullptr), size_(0), blocks_count_(0),
      first_block_(nullptr), last_block_(nullptr), max_index_(0)
{
    const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(base);
    const std::uintptr_t end = begin + size;
    // user blocks start aligned, the descriptors are aligned at the top
    const std::uintptr_t data = (begin + kAlign - 1) / kAlign * kAlign;
    if (!base || end < begin || end - begin < sizeof(Block))
        throw err_;
    const std::uintptr_t head = (end - sizeof(Block)) / alignof(Block) * alignof(Block);
    if (head < data || head - data < kAlign)
        throw err_;

    memory_ = reinterpret_cast<unsigned char*>(data);
    size_ = static_cast<std::size_t>(head + sizeof(Block) - data);
    std::memset(memory_, 0, size_);

    first_block_ = reinterpret_cast<Block*>(head);

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
    if (!size || size > kMaxRequest)
        throw err_;

    Block* block = put_block(round_up(size));
    if (!block)
        throw err_;

    Pointer p(this);
    p.block_ = block;
    p.block_index_ = block->index_;
    return p;
}

void Allocator::realloc(Pointer& p, std::size_t new_size)
{
    // keep the following blocks aligned
    new_size = round_up(new_size);

    Block *new_block = nullptr, *block = p.find();

    if (block) {
        unsigned char* ptr = (block->pointer_ + block->size_);

        if ((block->next_ && (std::size_t)(block->next_->pointer_ - ptr) >= new_size) ||
            (!block->next_ && ((std::size_t)((unsigned char*)last_block_ - ptr) >= new_size))) {
            block->size_ = new_size;
            return;
        } else if ((new_block = put_block(new_size))) {
            p.block_index_ = new_block->index_;
            p.owner_ = this;

            std::memcpy(new_block->pointer_, block->pointer_, block->size_);

            del_block(block);
            p.block_ = block;
            return;
        }
    }

    if ((new_block = put_block(new_size))) {
        p.block_ = new_block;
        p.block_index_ = new_block->index_;
        p.owner_ = this;
        return;
    }
    throw err_;
}

void Allocator::free(Pointer& p)
{
    del_block(p.find());
    p.owner_ = nullptr;
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

Allocator::Block* Allocator::tail_block() const
{
    Block* curr = first_block_;
    while (curr->next_)
        curr = curr->next_;
    return curr;
}

Allocator::Block* Allocator::new_block(std::uint32_t size, unsigned char* ptr, Block* prev,
                                       Block* next)
{
    // a new descriptor always takes the slot below the lowest one
    Block* block = --last_block_;

    // a smart pointer finds its block by the index, so two live blocks
    // must never share one (0 is the list head)
    if (++max_index_ == 0)
        ++max_index_;

    block->pointer_ = ptr;
    block->size_ = size;
    block->index_ = max_index_;
    block->prev_ = prev;
    block->next_ = next;

    if (prev)
        prev->next_ = block;
    if (next)
        next->prev_ = block;

    ++blocks_count_;
    return block;
}

Allocator::Block* Allocator::put_block(std::size_t size)
{
    Block* tail = tail_block();
    // any new block needs room for one more descriptor
    if (tail_room(end_of(tail)) < sizeof(Block))
        return nullptr;

    // first fit; the gap before the first block counts too
    for (Block* curr = first_block_; curr->next_; curr = curr->next_) {
        unsigned char* ptr = end_of(curr);
        if (static_cast<std::size_t>(curr->next_->pointer_ - ptr) >= size)
            return new_block(static_cast<std::uint32_t>(size), ptr, curr, curr->next_);
    }

    // the free tail
    if (tail_room(end_of(tail)) - sizeof(Block) >= size)
        return new_block(static_cast<std::uint32_t>(size), end_of(tail), tail, nullptr);

    return nullptr;
}

void Allocator::del_block(Block* block)
{
    if (!block || block == first_block_)
        return;

    // unlink
    if (block->next_)
        block->next_->prev_ = block->prev_;
    if (block->prev_)
        block->prev_->next_ = block->next_;

    // keep the descriptor array dense: move the lowest descriptor
    // into the hole (descriptors only move up)
    if (block != last_block_) {
        *block = *last_block_;
        if (block->prev_)
            block->prev_->next_ = block;
        if (block->next_)
            block->next_->prev_ = block;
    }

    std::memset(static_cast<void*>(last_block_), 0, sizeof(Block));
    ++last_block_;
    --blocks_count_;
}
