#pragma once

#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

namespace fizzy
{
template <typename T>
class Stack : private std::vector<T>
{
public:
    using std::vector<T>::clear;
    using std::vector<T>::empty;
    using std::vector<T>::size;

    /// Push an item to the stack.
    void push(T val) { std::vector<T>::emplace_back(val); }

    /// Returns the reference to the top item on the stack.
    ///
    /// This is not currently in use (except unit tests), but left as is classic stack method.
    const T& top() const noexcept { return std::vector<T>::back(); }

    /// Pops an item from the top of the stack and returns it.
    T pop()
    {
        const auto res = top();
        std::vector<T>::pop_back();
        return res;
    }

    /// Drops @a num_elements elements from the top of the stack.
    void drop(size_t num_elements) noexcept { std::vector<T>::resize(size() - num_elements); }
};

class OperandStack
{
    /// The pointer to the top item, or below the stack bottom if stack is empty.
    ///
    /// This pointer always alias m_storage, but it is kept as the first field
    /// because it is accessed the most. Therefore, it must be initialized
    /// in the constructor after the m_storage.
    uint64_t* m_top;

    /// The storage for items.
    std::unique_ptr<uint64_t[]> m_storage;

public:
    /// Default constructor. Sets the top item pointer to below the stack bottom.
    explicit OperandStack(size_t max_stack_height)
      : m_storage{std::make_unique<uint64_t[]>(max_stack_height)}
    {
        m_top = m_storage.get() - 1;
    }

    OperandStack(const OperandStack&) = delete;
    OperandStack& operator=(const OperandStack&) = delete;

    /// The current number of items on the stack (aka stack height).
    [[nodiscard]] size_t size() const noexcept
    {
        return static_cast<size_t>(m_top + 1 - &m_storage[0]);
    }

    /// Returns the reference to the top item.
    /// Requires non-empty stack.
    [[nodiscard]] auto& top() noexcept
    {
        assert(size() != 0);
        return *m_top;
    }

    /// Returns the reference to the stack item on given position from the stack top.
    /// Requires index < size().
    [[nodiscard]] auto& operator[](size_t index) noexcept
    {
        assert(index < size());
        return *(m_top - index);
    }

    /// Pushes an item on the stack.
    /// The stack max height limit is not checked.
    void push(uint64_t item) noexcept { *++m_top = item; }

    /// Returns an item popped from the top of the stack.
    /// Requires non-empty stack.
    auto pop() noexcept
    {
        assert(size() != 0);
        return *m_top--;
    }

    /// Shrinks the stack to the given new size by dropping items from the top.
    ///
    /// Requires new_size <= size().
    /// shrink(0) clears entire stack and moves the top pointer below the stack base.
    void shrink(size_t new_size) noexcept
    {
        assert(new_size <= size());
        // For new_size == 0, the m_top will point below the storage.
        m_top = m_storage.get() + new_size - 1;
    }

    /// Returns iterator to the bottom of the stack.
    [[nodiscard]] const uint64_t* rbegin() const noexcept { return m_storage.get(); }

    /// Returns end iterator counting from the bottom of the stack.
    [[nodiscard]] const uint64_t* rend() const noexcept { return m_top + 1; }
};
}  // namespace fizzy
