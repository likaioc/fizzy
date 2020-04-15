#include "stack.hpp"
#include <gmock/gmock.h>

using namespace fizzy;

TEST(stack, push_and_pop)
{
    Stack<char> stack;

    EXPECT_EQ(stack.size(), 0);
    EXPECT_TRUE(stack.empty());

    stack.push('a');
    stack.push('b');
    stack.push('c');

    EXPECT_EQ(stack.size(), 3);

    EXPECT_EQ(stack.pop(), 'c');
    EXPECT_EQ(stack.pop(), 'b');
    EXPECT_EQ(stack.pop(), 'a');

    EXPECT_EQ(stack.size(), 0);
    EXPECT_TRUE(stack.empty());
}

TEST(stack, drop_and_peek)
{
    Stack<char> stack;
    stack.push('w');
    stack.push('x');
    stack.push('y');
    stack.push('z');

    EXPECT_FALSE(stack.empty());
    EXPECT_EQ(stack.size(), 4);
    EXPECT_EQ(stack.top(), 'z');
    EXPECT_EQ(stack.size(), 4);

    stack.drop(1);
    EXPECT_EQ(stack.size(), 3);
    EXPECT_EQ(stack.top(), 'y');

    stack.drop(2);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(stack.top(), 'w');

    stack.drop(1);
    EXPECT_EQ(stack.size(), 0);
    EXPECT_TRUE(stack.empty());
}

TEST(stack, clear)
{
    Stack<char> stack;
    stack.push('a');
    stack.push('b');

    EXPECT_FALSE(stack.empty());
    EXPECT_EQ(stack.size(), 2);

    stack.drop(0);
    EXPECT_FALSE(stack.empty());
    EXPECT_EQ(stack.size(), 2);

    stack.clear();
    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.size(), 0);
}

TEST(stack, clear_on_empty)
{
    Stack<char> stack;
    stack.clear();
}

TEST(operand_stack, construct)
{
    OperandStack stack{0};
    EXPECT_THAT(stack.size(), 0);
    stack.shrink(0);
    EXPECT_THAT(stack.size(), 0);
}

TEST(operand_stack, top)
{
    OperandStack stack{1};
    EXPECT_THAT(stack.size(), 0);

    stack.push(1);
    EXPECT_THAT(stack.size(), 1);
    EXPECT_THAT(stack.top(), 1);
    EXPECT_THAT(stack[0], 1);

    stack.top() = 101;
    EXPECT_THAT(stack.size(), 1);
    EXPECT_THAT(stack.top(), 101);
    EXPECT_THAT(stack[0], 101);

    stack.shrink(0);
    EXPECT_THAT(stack.size(), 0);

    stack.push(2);
    EXPECT_THAT(stack.size(), 1);
    EXPECT_THAT(stack.top(), 2);
    EXPECT_THAT(stack[0], 2);
}

TEST(operand_stack, small)
{
    OperandStack stack{3};
    EXPECT_THAT(stack.size(), 0);

    stack.push(1);
    stack.push(2);
    stack.push(3);
    EXPECT_THAT(stack.size(), 3);
    EXPECT_THAT(stack.top(), 3);
    EXPECT_THAT(stack[0], 3);
    EXPECT_THAT(stack[1], 2);
    EXPECT_THAT(stack[2], 1);

    stack[0] = 13;
    stack[1] = 12;
    stack[2] = 11;
    EXPECT_THAT(stack.size(), 3);
    EXPECT_THAT(stack.top(), 13);
    EXPECT_THAT(stack[0], 13);
    EXPECT_THAT(stack[1], 12);
    EXPECT_THAT(stack[2], 11);

    EXPECT_THAT(stack.pop(), 13);
    EXPECT_THAT(stack.size(), 2);
    EXPECT_THAT(stack.top(), 12);
}

TEST(operand_stack, large)
{
    constexpr auto max_height = 33;
    OperandStack stack{max_height};

    for (unsigned i = 0; i < max_height; ++i)
        stack.push(i);

    EXPECT_THAT(stack.size(), max_height);
    for (int expected = max_height - 1; expected >= 0; --expected)
        EXPECT_THAT(stack.pop(), expected);
    EXPECT_THAT(stack.size(), 0);
}

TEST(operand_stack, shrink)
{
    constexpr auto max_height = 60;
    OperandStack stack{max_height};

    for (unsigned i = 0; i < max_height; ++i)
        stack.push(i);

    EXPECT_THAT(stack.size(), max_height);
    constexpr auto new_height = max_height / 3;
    stack.shrink(new_height);
    EXPECT_THAT(stack.size(), new_height);
    EXPECT_THAT(stack.top(), new_height - 1);
    EXPECT_THAT(stack[0], new_height - 1);
    EXPECT_THAT(stack[new_height - 1], 0);
}
