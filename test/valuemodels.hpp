/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TEST_VALUEMODELS_HPP
#define CPPWAMP_TEST_VALUEMODELS_HPP

#include <array>
#include <numeric>
#include <stdexcept>

namespace wamp
{

namespace test
{

//------------------------------------------------------------------------------
struct SmallValue
{
    SmallValue() : defaultConstructed(true) {}

    SmallValue(int n) : value(n), valueConstructed(true) {}

    SmallValue(const SmallValue& rhs)
    {
        if (rhs.poisoned)
            throw std::bad_alloc();
        value = rhs.value;
        copyConstructed = true;
    }

    SmallValue(SmallValue&& rhs)
    {
        if (rhs.poisoned)
            throw std::bad_alloc();
        value = std::move(rhs.value);
        moveConstructed = true;
        rhs.value = 0;
        rhs.movedFrom = true;
    }

    SmallValue& operator=(const SmallValue& rhs)
    {
        if (rhs.poisoned)
            throw std::bad_alloc();
        value = rhs.value;
        copyAssigned = true;
        return *this;
    }

    SmallValue& operator=(SmallValue&& rhs)
    {
        if (rhs.poisoned)
            throw std::bad_alloc();
        value = rhs.value;
        rhs.value = 0;
        moveAssigned = true;
        rhs.movedFrom = true;
        return *this;
    }

    bool operator==(const SmallValue& rhs) const {return value == rhs.value;}

    void resetFlags()
    {
        defaultConstructed = false;
        valueConstructed = false;
        copyConstructed = false;
        moveConstructed = false;
        copyAssigned = false;
        moveAssigned = false;
        movedFrom = false;
        poisoned = false;
    }

    void poison() {poisoned = true;}

    int value = 0;
    bool defaultConstructed = false;
    bool valueConstructed = false;
    bool copyConstructed = false;
    bool moveConstructed = false;
    bool copyAssigned = false;
    bool moveAssigned = false;
    bool movedFrom = false;
    bool poisoned = false;
};

//------------------------------------------------------------------------------
template <std::size_t Size>
struct LargeValue
{
    static_assert(Size > sizeof(int), "");

    LargeValue() : defaultConstructed(true) {}

    LargeValue(int n) : value(n), valueConstructed(true)
    {}

    LargeValue(const LargeValue& rhs)
    {
        if (rhs.poisoned)
            throw std::bad_alloc();
        value = rhs.value;
        copyConstructed = true;
    }

    LargeValue(LargeValue&& rhs)
    {
        if (rhs.poisoned)
            throw std::bad_alloc();
        value = std::move(rhs.value);
        moveConstructed = true;
        rhs.value = 0;
        rhs.movedFrom = true;
    }

    LargeValue& operator=(const LargeValue& rhs)
    {
        if (rhs.poisoned)
            throw std::bad_alloc();
        value = rhs.value;
        copyAssigned = true;
        return *this;
    }

    LargeValue& operator=(LargeValue&& rhs)
    {
        if (rhs.poisoned)
            throw std::bad_alloc();
        value = rhs.value;
        rhs.value = 0;
        moveAssigned = true;
        rhs.movedFrom = true;
        return *this;
    }

    bool operator==(const LargeValue& rhs) const {return value == rhs.value;}

    void poison() {poisoned = true;}

    void resetFlags()
    {
        defaultConstructed = false;
        valueConstructed = false;
        copyConstructed = false;
        moveConstructed = false;
        copyAssigned = false;
        moveAssigned = false;
        movedFrom = false;
        poisoned = false;
    }

    int value = 0;
    std::array<char, Size - sizeof(int)> padding;
    bool defaultConstructed = false;
    bool valueConstructed = false;
    bool copyConstructed = false;
    bool moveConstructed = false;
    bool copyAssigned = false;
    bool moveAssigned = false;
    bool movedFrom = false;
    bool poisoned = false;
};

} // namespace test

} // namespace wamp

#endif // CPPWAMP_TEST_VALUEMODELS_HPP
