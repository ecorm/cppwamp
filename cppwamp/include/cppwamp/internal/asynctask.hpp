/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ASYNCTASK_HPP
#define CPPWAMP_ASYNCTASK_HPP

#include <cassert>
#include <functional>
#include "../asiodefs.hpp"
#include "../asyncresult.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
// Bundles an AsyncHandler along with the AsioService in which the handler
// is to be posted.
//------------------------------------------------------------------------------
template <typename TResult>
class AsyncTask
{
public:
    using ValueType = TResult;

    AsyncTask() : iosvc_(nullptr) {}

    AsyncTask(AsioService& iosvc, AsyncHandler<TResult> handler)
        : iosvc_(&iosvc),
          handler_(std::move(handler))
    {}

    AsyncTask(const AsyncTask& other) = default;

    AsyncTask(AsyncTask&& other) noexcept
        : iosvc_(other.iosvc_),
          handler_(std::move(other.handler_))
    {
        other.iosvc_ = nullptr;
    }

    AsyncTask& operator=(const AsyncTask& other) = default;

    AsyncTask& operator=(AsyncTask&& other) noexcept
    {
        iosvc_ = other.iosvc_;
        handler_ = std::move(other.handler_);
        other.iosvc_ = nullptr;
        return *this;
    }

    explicit operator bool() const {return iosvc_ != nullptr;}

    AsioService& iosvc() const {return *iosvc_;}

    const AsyncHandler<ValueType>& handler() const {return handler_;}

    void operator()(AsyncResult<ValueType> result) const &
    {
        assert(iosvc_ && "Invoking uninitialized AsyncTask");
        iosvc_->post(std::bind(handler_, std::move(result)));
    }

    void operator()(AsyncResult<ValueType> result) &&
    {
        assert(iosvc_ && "Invoking uninitialized AsyncTask");
        iosvc_->post(std::bind(std::move(handler_), std::move(result)));
    }

private:
    AsioService* iosvc_;
    AsyncHandler<ValueType> handler_;
};

//------------------------------------------------------------------------------
/** ResultTypeOfHandler specialization for AsyncTask */
//------------------------------------------------------------------------------
template <typename T>
struct ResultTypeOfHandler<AsyncTask<T>> {using Type = AsyncResult<T>;};

} // namespace wamp

#endif // CPPWAMP_ASYNCTASK_HPP
