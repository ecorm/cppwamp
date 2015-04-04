/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ASYNCRESULT_HPP
#define CPPWAMP_ASYNCRESULT_HPP

//------------------------------------------------------------------------------
/** @file
    Contains facilities for reporting results and errors back to asynchronous
    handlers. */
//------------------------------------------------------------------------------

#include <functional>
#include <system_error>

namespace wamp
{

//------------------------------------------------------------------------------
/** Value type that combines an asynchronous result with an error code.
    Normally, exceptions thrown during the execution of an asynchronous
    operation are not transported to the associated handler function. To work
    around this problem, this class provides a safe mechanism for transporting
    such errors back to asynchronous handlers. If the asynchronous handler
    attempts to access the value of a failed AsyncResult, an error::Wamp is
    thrown.

    @tparam T The value type of the asynchronous result. It must be copyable
              and default-constructible.

    @see AsyncHandler */
//------------------------------------------------------------------------------
template <typename T>
class AsyncResult
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
    void checkError() const;

    ValueType value_;
    std::error_code errorCode_;
    std::string errorInfo_;
};

//------------------------------------------------------------------------------
/** Type alias for a handler taking an AsyncResult`<T>` parameter. */
//------------------------------------------------------------------------------
template <typename T> using AsyncHandler = std::function<void (AsyncResult<T>)>;

} // namespace wamp

#include "internal/asyncresult.ipp"

#endif // CPPWAMP_ASYNCRESULT_HPP
