/*------------------------------------------------------------------------------
              Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ASYNCTASK_HPP
#define CPPWAMP_ASYNCTASK_HPP

#include <cassert>
#include <functional>
#include <boost/asio/post.hpp>
#include "../asiodefs.hpp"
#include "../asyncresult.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
// Bundles an AsyncHandler along with the executor in which the handler
// is to be posted.
//------------------------------------------------------------------------------
template <typename TResult>
class AsyncTask
{
public:
    using ValueType = TResult;

    AsyncTask() : executor_(nullptr) {}

    AsyncTask(AnyExecutor exec, AsyncHandler<TResult> handler)
        : executor_(exec),
          handler_(std::move(handler))
    {}

    AsyncTask(const AsyncTask& other) = default;

    AsyncTask(AsyncTask&& other) noexcept
        : executor_(std::move(other.executor_)),
          handler_(std::move(other.handler_))
    {
        other.executor_ = nullptr;
    }

    AsyncTask& operator=(const AsyncTask& other) = default;

    AsyncTask& operator=(AsyncTask&& other) noexcept
    {
        executor_ = std::move(other.executor_);
        handler_ = std::move(other.handler_);
        other.executor_ = nullptr;
        return *this;
    }

    explicit operator bool() const {return executor_ != nullptr;}

    AnyExecutor executor() const {return executor_;}

    const AsyncHandler<ValueType>& handler() const {return handler_;}

    void operator()(AsyncResult<ValueType> result) const &
    {
        assert(executor_ && "Invoking uninitialized AsyncTask");
        boost::asio::post(executor_, std::bind(handler_, std::move(result)));
    }

    void operator()(AsyncResult<ValueType> result) &&
    {
        assert(executor_ && "Invoking uninitialized AsyncTask");
        boost::asio::post(executor_, std::bind(std::move(handler_),
                                               std::move(result)));
    }

private:
    AnyExecutor executor_;
    AsyncHandler<ValueType> handler_;
};

//------------------------------------------------------------------------------
/** ResultTypeOfHandler specialization for AsyncTask */
//------------------------------------------------------------------------------
template <typename T>
struct ResultTypeOfHandler<AsyncTask<T>> {using Type = AsyncResult<T>;};

} // namespace wamp

#endif // CPPWAMP_ASYNCTASK_HPP
