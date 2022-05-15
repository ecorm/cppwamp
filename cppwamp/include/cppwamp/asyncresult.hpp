/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ASYNCRESULT_HPP
#define CPPWAMP_ASYNCRESULT_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for reporting results and errors back
           to asynchronous handlers. */
//------------------------------------------------------------------------------

#include <functional>
#include <system_error>
#include "api.hpp"
#include "error.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Value type that combines an asynchronous result with an error code.
    Normally, exceptions thrown during the execution of an asynchronous
    operation are not transported to the associated handler function. To work
    around this problem, this class provides a safe mechanism for transporting
    such errors back to asynchronous handlers. If the asynchronous handler
    attempts to access the value of a failed AsyncResult, an error::Failure is
    thrown.

    @tparam T The value type of the asynchronous result. It must be copyable
              and default-constructible.

    @see AsyncHandler */
//------------------------------------------------------------------------------
template <typename T>
class CPPWAMP_API AsyncResult
{
public:
    using ValueType = T; /**< Value type of the asynchronous result. */

    /** Converting constructor taking a value. */
    AsyncResult(T value = T());

    /** Converting constructor taking an error code. */
    AsyncResult(std::error_code ec);

    /** Constructor taking an error code and informational text. */
    AsyncResult(std::error_code ec, std::string info);

    /** Conversion to `bool` operator indicating if the asynchronous operation
        was successful. */
    explicit operator bool() const;

    /** Accesses the asynchronous result value. */
    ValueType& get();

    /** Const version of AsyncResult::get. */
    const ValueType& get() const;

    /** Returns the error code associated with this asynchronous result. */
    std::error_code errorCode() const;

    /** Returns informational text associated with an error condition. */
    const std::string& errorInfo() const;

    /** Sets the asynchronous result value. */
    AsyncResult& setValue(T value);

    /** Sets the error code. */
    AsyncResult& setError(std::error_code ec);

    /** Set the error code and informational text. */
    AsyncResult& setError(std::error_code ec, std::string info);

private:
    CPPWAMP_HIDDEN void checkError() const;

    ValueType value_;
    std::error_code errorCode_;
    std::string errorInfo_;
};

//------------------------------------------------------------------------------
/** Type alias for a handler taking an AsyncResult`<T>` parameter. */
//------------------------------------------------------------------------------
template <typename T> using AsyncHandler = std::function<void (AsyncResult<T>)>;


//------------------------------------------------------------------------------
/** Type traits template used to obtain the result type of an asynchronous
    handler. */
//------------------------------------------------------------------------------
template <typename THandler>
struct CPPWAMP_API ResultTypeOfHandler {};

//------------------------------------------------------------------------------
/** ResultTypeOfHandler specialization for AsyncHandler */
//------------------------------------------------------------------------------
template <typename T>
struct CPPWAMP_API ResultTypeOfHandler<AsyncHandler<T>>
{
    using Type = AsyncResult<T>;
};


//******************************************************************************
// AsyncResult implementation
//******************************************************************************

//------------------------------------------------------------------------------
/** @post `this->get() == value`
    @post `this->errorCode() == false`
    @post `this->errorInfo().empty() == true` */
//------------------------------------------------------------------------------
template <typename T>
AsyncResult<T>::AsyncResult(T value) : value_(std::move(value)) {}

//------------------------------------------------------------------------------
/** @post `this->errorCode() == ec`
    @post `this->errorInfo().empty() == true` */
//------------------------------------------------------------------------------
template <typename T>
AsyncResult<T>::AsyncResult(std::error_code ec) : value_(T()), errorCode_(ec) {}

//------------------------------------------------------------------------------
/** @post `this->errorCode() == ec`
    @post `this->errorInfo() == info` */
//------------------------------------------------------------------------------
template <typename T>
AsyncResult<T>::AsyncResult(std::error_code ec, std::string info)
    : value_(T()), errorCode_(ec), errorInfo_(std::move(info)) {}

//------------------------------------------------------------------------------
/** @return `true` iff `this->errorCode() == false` */
//------------------------------------------------------------------------------
template <typename T>
AsyncResult<T>::operator bool() const {return !errorCode_;}

//------------------------------------------------------------------------------
/** @details
    If the AsyncResult contains a non-zero error code, then an error::Failure
    exception is thrown. The `error::Failure::code` function of the thrown
    exception will return the same error code as `AsyncResult::errorCode`.
    @throws error::Failure if `this->errorCode() == true`. */
//------------------------------------------------------------------------------
template <typename T>
T& AsyncResult<T>::get()
{
    checkError();
    return value_;
}

//------------------------------------------------------------------------------
template <typename T>
const T& AsyncResult<T>::get() const
{
    checkError();
    return value_;
}

//------------------------------------------------------------------------------
/** @details
    If the asynchronous operation was successful, then
    `errorCode().value() == 0` and `errorCode()` will evaluate to
    `false`. */
//------------------------------------------------------------------------------
template <typename T>
std::error_code AsyncResult<T>::errorCode() const {return errorCode_;}

//------------------------------------------------------------------------------
/** @details
    This text is also used as the `info` string of an error::Failure
    exception that might be thrown during AsyncResult::get. */
//------------------------------------------------------------------------------
template <typename T>
const std::string& AsyncResult<T>::errorInfo() const {return errorInfo_;}

//------------------------------------------------------------------------------
/** @note Does not change the stored error code or the info text.
    @post `this->get() == value` */
//------------------------------------------------------------------------------
template <typename T>
AsyncResult<T>& AsyncResult<T>::setValue(T value)
{
    value_ = std::move(value);
    return *this;
}

//------------------------------------------------------------------------------
/** @note Does not change the stored value or the info text.
    @post `this->errorCode() == ec` */
//------------------------------------------------------------------------------
template <typename T>
AsyncResult<T>& AsyncResult<T>::setError(std::error_code ec)
{
    errorCode_ = ec;
    return *this;
}

//------------------------------------------------------------------------------
/** @note Does not change the stored value.
    @post `this->errorCode() == ec`
    @post `this->errorInfo() == info` */
//------------------------------------------------------------------------------
template <typename T>
AsyncResult<T>& AsyncResult<T>::setError(std::error_code ec, std::string info)
{
    errorCode_ = ec;
    errorInfo_ = std::move(info);
    return *this;
}

//------------------------------------------------------------------------------
template <typename T>
void AsyncResult<T>::checkError() const
{
    if (errorCode_)
    {
        if (errorInfo_.empty())
            throw error::Failure(errorCode_);
        else
            throw error::Failure(errorCode_, errorInfo_);
    }
}

} // namespace wamp

#endif // CPPWAMP_ASYNCRESULT_HPP
