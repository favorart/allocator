#pragma once

/**
 * @file allocator.h
 * @brief A memory allocator over a fixed buffer with defragmentation,
 *        and a smart pointer that survives block moves.
 *
 * Technosphere (Mail.Ru Group), course "Multithreaded programming in C++".
 * Kirill Golikov, 04.04.2015. Reviewed in 2026.
 *
 * The task: an allocator and a smart pointer class with memory
 * defragmentation, WITHOUT ANY EXTRA MEMORY.
 *
 * A real doubly linked list stored inside the managed memory; its nodes
 * are kept at the end of the buffer as a dense array without holes:
 *   - add and remove a node: O(1);
 *   - defragmentation: O(n * k);
 *   - reallocation / growth: O(n * l);
 * where n is the number of blocks, k is the average block size in bytes,
 * l is the size of the given block.
 *
 * A smart pointer finds a node that moved by "open addressing": it scans
 * from the slot it saw last towards the list head, O(n).
 *
 * Layout of the managed buffer:
 * @code
 * +--------------------------------------------------------------+
 * | data -->              free              <-- descriptors | head |
 * +--------------------------------------------------------------+
 * @endcode
 */

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

/** @brief The kind of an allocation error. */
enum AllocErrorType
{
    InvalidFree, ///< Reserved: freeing a pointer that is not allocated.
    NoMemory     ///< Not enough memory, or an invalid size or arena.
};

/** @brief The exception thrown by Allocator. */
class AllocError : public std::runtime_error
{
public:
    /**
     * @brief Creates an error.
     * @param type    The kind of the error.
     * @param message The text returned by what().
     */
    AllocError(AllocErrorType type, const std::string& message)
        : std::runtime_error(message), type_(type)
    {
    }

    /** @return The kind of the error. */
    AllocErrorType getType() const { return type_; }

private:
    AllocErrorType type_;
};

/**
 * @brief The library version, taken from the git tag at build time.
 * @return "0.0.9" for a tagged build, "0.0.9-2-gabc1234" for a later commit,
 *         "unknown" without git.
 */
const char* allocatorVersion();

class Pointer;

/**
 * @brief Allocates blocks inside a buffer given by the caller.
 *
 * All bookkeeping lives inside the buffer. Blocks are handed out as
 * Pointer objects, because defrag() and realloc() may move them.
 * Not thread-safe. Neither copyable nor movable: every Pointer keeps
 * the address of its allocator.
 */
class Allocator final
{
public:
    /**
     * @brief Takes `[base, base + size)` under management and clears it.
     * @param base The start of the buffer; it does not have to be aligned.
     * @param size The size of the buffer in bytes.
     * @throws AllocError (NoMemory) if `base` is null or the buffer is too
     *         small for the list head and one aligned data unit.
     */
    explicit Allocator(void* base, std::size_t size);

    Allocator(const Allocator&) = delete;
    Allocator& operator=(const Allocator&) = delete;

    /**
     * @brief Allocates a block, first fit.
     * @param size The size in bytes, 1 .. 2 GiB. Rounded up to
     *             `alignof(std::max_align_t)`.
     * @return A pointer to the new block; its address is aligned
     *         to `alignof(std::max_align_t)`.
     * @throws AllocError (NoMemory) if `size` is 0 or too large,
     *         or there is no free space for it.
     */
    Pointer alloc(std::size_t size);

    /**
     * @brief Changes the size of a block, moving it if needed.
     *
     * Grows or shrinks in place when the space up to the next block allows
     * it; otherwise allocates a new block, copies the data and frees the old
     * one. Then only `p` is valid: its copies resolve to `nullptr`.
     *
     * @param p        The block. If it is empty, works like alloc().
     * @param new_size The new size in bytes. 0 works like free().
     * @throws AllocError (NoMemory) if there is no space; `p` and its
     *         data stay unchanged.
     */
    void realloc(Pointer& p, std::size_t new_size);

    /**
     * @brief Frees a block and resets `p`.
     *
     * An empty or already freed pointer (or its copy) is ignored.
     * @param p The block to free.
     */
    void free(Pointer& p);

    /**
     * @brief Moves all blocks to the start of the buffer, in address order,
     *        so that all free space becomes one piece.
     *
     * Pointer objects and their copies stay valid; raw addresses
     * from Pointer::get() do not.
     */
    void defrag();

private:
    friend class Pointer;

    /** A block descriptor: a node of the address-sorted list. */
    struct Block
    {
        unsigned char* pointer_; ///< Data of the block.
        std::uint32_t size_;     ///< Size of the data, rounded up.
        std::uint32_t index_;    ///< Unique id; 0 is the list head.
        Block* prev_;
        Block* next_;
    };

    /** Every user block starts at this alignment, like malloc(). */
    static constexpr std::size_t kAlign = alignof(std::max_align_t);

    static std::size_t round_up(std::size_t size) { return (size + kAlign - 1) / kAlign * kAlign; }

    /** Free bytes between the end of the data and the descriptors. */
    std::size_t tail_room(const unsigned char* data_end) const
    {
        return static_cast<std::size_t>(reinterpret_cast<const unsigned char*>(last_block_) -
                                        data_end);
    }

    /** End of the block's data; for the list head, the start of the buffer. */
    unsigned char* end_of(const Block* block) const
    {
        return (block == first_block_) ? memory_ : block->pointer_ + block->size_;
    }

    Block* tail_block() const;
    Block* new_block(std::uint32_t size, unsigned char* ptr, Block* prev, Block* next);
    Block* put_block(std::size_t size);
    void del_block(Block* block);

    AllocError err_;             ///< Prepared in advance, to throw without building a message.
    unsigned char* memory_;      ///< Start of the data area (aligned).
    std::size_t size_;           ///< Size of the managed area.
    std::uint32_t blocks_count_; ///< Number of descriptors, the list head included.
    Block* first_block_;         ///< List head: the highest descriptor, never moves.
    Block* last_block_;          ///< The lowest descriptor.
    std::uint32_t max_index_;    ///< The last index given out; indices only grow.
};

/**
 * @brief A handle to a block of an Allocator.
 *
 * Copyable. It remembers the descriptor slot it saw last and the block's
 * index. Descriptors only move towards the list head, so get() finds a
 * moved block by scanning upward from that slot.
 */
class Pointer final
{
public:
    /** @brief Creates an empty pointer: get() returns `nullptr`. */
    Pointer() = default;

    /**
     * @brief Returns the current address of the block.
     *
     * The address is valid until the next Allocator::realloc() or
     * Allocator::defrag().
     * @return The address, or `nullptr` if the pointer is empty
     *         or the block was freed.
     */
    void* get() const
    {
        block_ = find();
        return block_ ? block_->pointer_ : nullptr;
    }

private:
    friend class Allocator;

    explicit Pointer(const Allocator* owner) : owner_(owner) {}

    Allocator::Block* find() const;

    mutable Allocator::Block* block_ = nullptr; ///< The slot seen last.
    const Allocator* owner_ = nullptr;
    std::uint32_t block_index_ = 0;
};

inline Allocator::Block* Pointer::find() const
{
    if (!block_ || !owner_)
        return nullptr;

    // slots below the descriptor array are free and may already hold user data
    if (block_ < owner_->last_block_)
        block_ = owner_->last_block_;

    for (; block_ != owner_->first_block_; ++block_)
        if (block_->index_ == block_index_)
            return block_;

    block_ = nullptr;
    return nullptr;
}
