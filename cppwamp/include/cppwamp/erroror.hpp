/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ERROROR_HPP
#define CPPWAMP_ERROROR_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the ErrorOr template class. */
//------------------------------------------------------------------------------

#include <cassert>
#include <functional>
#include <memory>
#include <system_error>
#include <utility>
#include "api.hpp"
#include "exceptions.hpp"
#include "traits.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Minimalistic implementation of
    [std::unexpected](https://wg21.link/P0323#expected.un)<std::error_code>.

    This wrapper type is used to initialize an ErrorOr with an error in an
    unambiguous manner.

    @see ErrorOr */
//------------------------------------------------------------------------------
template <typename E>
class Unexpected
{
public:
    using error_type = E; ///< Type representing errors.

    Unexpected() = delete;

    /** Constructor taking an error value. */
    explicit Unexpected(error_type error) noexcept : error_(std::move(error)) {}

    /** Accesses the error value. */
    error_type& value() & noexcept { return error_; }

    /** Moves the error value. */
    error_type&& value() && noexcept { return std::move(error_); }

    /** Accesses the error value. */
    const error_type& value() const& noexcept { return error_; }

    /** Moves the error value. */
    const error_type&& value() const&& noexcept { return std::move(error_); }

    /** Swaps contents with another UnexpectedError. */
    void swap(Unexpected& other) noexcept(isNothrowSwappable<E>())
    {
        using std::swap;
        swap(error_, other.error_);
    }

private:
    error_type error_;
};

/** Equality comparison.
    @relates Unexpected */
template <typename E1, typename E2>
CPPWAMP_API bool operator==(const Unexpected<E1>& x, const Unexpected<E2>& y)
{
    return x.value() == y.value();
}

/** Inequality comparison.
    @relates Unexpected */
template <typename E1, typename E2>
CPPWAMP_API bool operator!=(const Unexpected<E1>& x, const Unexpected<E2>& y)
{
    return x.value() != y.value();
}

/** Non-member swap.
    @relates Unexpected */
template <typename E>
CPPWAMP_API void swap(Unexpected<E>& x, Unexpected<E>& y)
    noexcept(noexcept(x.swap(y)))
{
    x.swap(y);
}

/** Non-standard factory function needed in C++11 due to lack of CTAD.
    @relates Unexpected */
template <typename E>
constexpr Unexpected<ValueTypeOf<E>> makeUnexpected(E&& error)
{
    return Unexpected<ValueTypeOf<E>>{std::forward<E>(error)};
}


//------------------------------------------------------------------------------
/** Type alias for Unexpected<std::error_code>. */
using UnexpectedError = Unexpected<std::error_code>;

//------------------------------------------------------------------------------
/** Convenience function that creates an UnexpectedError from
    an error code enum. */
//------------------------------------------------------------------------------
template <typename TErrorEnum>
CPPWAMP_API UnexpectedError makeUnexpectedError(TErrorEnum errc)
{
    return UnexpectedError(make_error_code(errc));
}


//------------------------------------------------------------------------------
/** Minimalistic implementation of
    [std::expected](https://wg21.link/P0323)<T, std::error_code>

    @tparam T The contained value type when there is no error.

    @see UnexpectedError, AsyncHandler */
//------------------------------------------------------------------------------
template <typename T>
class CPPWAMP_API ErrorOr
{
public:
    using value_type = T;               ///< Type representing result values.
    using error_type = std::error_code; ///< Type representing errors. */

    /** Default constructor. */
    ErrorOr() = default;

    // NOLINTBEGIN(google-explicit-constructor)

    /** Converting constructor taking a value. */
    ErrorOr(value_type value) : value_(std::move(value)) {}

    /** Converting constructor taking an Unexpected. */
    template <typename G>
    ErrorOr(Unexpected<G> unex)
        : value_(),
          error_(std::move(unex).value()),
          hasError_(true)
    {}

    // NOLINTEND(google-explicit-constructor)

    /** Initializes the value in-place using the given value constructor
        arguments. */
    template <typename... Args>
    value_type& emplace(Args&&... args)
    {
        value_ = value_type(std::forward<Args>(args)...);
        error_ = error_type();
        hasError_ = false;
        return value_;
    }

    /** Value assignment. */
    ErrorOr& operator=(value_type value)
    {
        value_ = std::move(value);
        error_ = error_type();
        hasError_ = false;
        return *this;
    }

    /** %Error assignment. */
    template <typename G>
    ErrorOr& operator=(Unexpected<G> unex)
    {
        error_ = error_type(std::move(unex).value());
        value_ = value_type();
        hasError_ = true;
        return *this;
    }

    /** Swap contents with another instance. */
    void swap(ErrorOr& rhs) noexcept(isNothrowSwappable<value_type>())
    {
        using std::swap;
        swap(value_, rhs.value_);
        swap(error_, rhs.error_);
        swap(hasError_, rhs.hasError_);
    }

    /** Unchecked access of a member of the stored value. */
    value_type* operator->()
    {
        assert(has_value());
        return std::addressof(value_);
    }

    /** Unchecked access of a member of the stored value. */
    const value_type* operator->() const
    {
        assert(has_value());
        return std::addressof(value_);
    }

    /** Unchecked access of the stored value.
        @pre `this->has_value() == true` */
    value_type& operator*() &
    {
        assert(has_value());
        return value_;
    }

    /** Unchecked move of the stored value.
        @pre `this->has_value() == true` */
    value_type&& operator*() &&
    {
        assert(has_value());
        return std::move(value_);
    }

    /** Unchecked access of the stored value.
        @pre `this->has_value() == true` */
    const value_type& operator*() const&
    {
        assert(has_value());
        return value_;
    }

    /** Unchecked move of the stored value.
        @pre `this->has_value() == true` */
    const value_type&& operator*() const&&
    {
        assert(has_value());
        return std::move(value_);
    }

    /** Indicates if a value is being contained. */
    explicit operator bool() const noexcept {return has_value();}

    /** Indicates if a value is being contained. */
    bool has_value() const noexcept {return !hasError_;}

    /** Checked access of the stored value.
        @pre `this->has_value() == true`
        @throws error::Failure if `this->has_value() == false` */
    value_type& value() &
    {
        checkError();
        return value_;
    }

    /** Checked move of the stored value.
        @pre `this->has_value() == true`
        @throws error::Failure if `this->has_value() == false` */
    value_type&& value() &&
    {
        checkError();
        return std::move(value_);
    }

    /** Checked access of the stored value.
        @pre `this->has_value() == true`
        @throws error::Failure if `this->has_value() == false` */
    const value_type& value() const&
    {
        checkError();
        return value_;
    }

    /** Checked move of the stored value.
        @pre `this->has_value() == true`
        @throws error::Failure if `this->has_value() == false` */
    const value_type&& value() const&&
    {
        checkError();
        return std::move(value_);
    }

    /** Unchecked access of the stored error.
        @pre `this->has_value() == false` */
    error_type& error()
    {
        assert(!has_value());
        return error_;
    }

    /** Unchecked access of the stored error.
        @pre `this->has_value() == false` */
    const error_type& error() const
    {
        assert(!has_value());
        return error_;
    }

    /** Returns the stored value if it exists, or the given fallback value. */
    template <typename U>
    value_type value_or(U&& v) const&
    {
        if (!has_value())
            return std::forward<U>(v);
        return value_;
    }

    /** Returns the moved stored value if it exists, or the given fallback
        value. */
    template <typename U>
    value_type value_or(U&& v) &&
    {
        if (!has_value())
            return std::forward<U>(v);
        return std::move(value_);
    }

private:
    CPPWAMP_HIDDEN void checkError() const
    {
        if (hasError_)
            throw error::Failure{error_};
    }

    value_type value_;
    error_type error_;
    bool hasError_ = false;
};

/** Non-member swap.
    @relates ErrorOr */
template <typename T>
CPPWAMP_API void swap(ErrorOr<T>& x, ErrorOr<T>& y)
    noexcept(noexcept(x.swap(y)))
{
    x.swap(y);
}

/** Equality comparison with another ErrorOr.
    @relates ErrorOr */
template <typename T1, typename T2>
CPPWAMP_API bool operator==(const ErrorOr<T1>& x, const ErrorOr<T2>& y)
{
    return (x.has_value() == y.has_value())
               ? (x.has_value() ? *x == *y : x.error() == y.error())
               : false;
}

/** Inequality comparison with another ErrorOr.
    @relates ErrorOr */
template <typename T1, typename T2>
CPPWAMP_API bool operator!=(const ErrorOr<T1>& x, const ErrorOr<T2>& y)
{
    return (x.has_value() == y.has_value())
               ? (x.has_value() ? *x != *y : x.error() != y.error())
               : true;
}

/** Equality comparison with a value.
    @relates ErrorOr */
template <typename T1, typename T2>
CPPWAMP_API bool operator==(const ErrorOr<T1>& x, const T2& v)
{
    return x.has_value() ? *x == v : false;
}

/** Equality comparison with a value.
    @relates ErrorOr */
template <typename T1, typename T2>
CPPWAMP_API bool operator==(const T2& v, const ErrorOr<T1>& x)
{
    return x.has_value() ? *x == v : false;
}

/** Inequality comparison with a value.
    @relates ErrorOr */
template <typename T1, typename T2>
CPPWAMP_API bool operator!=(const ErrorOr<T1>& x, const T2& v)
{
    return x.has_value() ? *x != v : true;
}

/** Inequality comparison with a value.
    @relates ErrorOr */
template <typename T1, typename T2>
CPPWAMP_API bool operator!=(const T2& v, const ErrorOr<T1>& x)
{
    return x.has_value() ? *x != v : true;
}

/** Equality comparison with an error.
    @relates ErrorOr */
template <typename T, typename E>
CPPWAMP_API bool operator==(const ErrorOr<T>& x, const Unexpected<E>& e)
{
    return x.has_value() ? false : x.error() == e.value();
}

/** Equality comparison with an error.
    @relates ErrorOr */
template <typename T, typename E>
CPPWAMP_API bool operator==(const Unexpected<E>& e, const ErrorOr<T>& x)
{
    return x.has_value() ? false : x.error() == e.value();
}

/** Inequality comparison with an error.
    @relates ErrorOr */
template <typename T, typename E>
CPPWAMP_API bool operator!=(const ErrorOr<T>& x, const Unexpected<E>& e)
{
    return x.has_value() ? true : x.error() != e.value();
}

/** Inequality comparison with an error.
    @relates ErrorOr */
template <typename T, typename E>
CPPWAMP_API bool operator!=(const Unexpected<E>& e, const ErrorOr<T>& x)
{
    return x.has_value() ? true : x.error() != e.value();
}

//------------------------------------------------------------------------------
/** Used to conveniently check if an operation completed, throwing an
    error::Failure if there was a failure. */
//------------------------------------------------------------------------------
using ErrorOrDone = ErrorOr<bool>;

} // namespace wamp

#endif // CPPWAMP_ERROROR_HPP
