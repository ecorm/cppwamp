/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include "../error.hpp"

namespace wamp
{

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
