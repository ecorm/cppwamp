/*------------------------------------------------------------------------------
                     Copyright Emile Cormier 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ASIOEXECUTOR_HPP
#define CPPWAMP_ASIOEXECUTOR_HPP

#ifndef BOOST_THREAD_VERSION
#define BOOST_THREAD_VERSION 4
#endif

#ifndef BOOST_THREAD_PROVIDES_EXECUTORS
#define BOOST_THREAD_PROVIDES_EXECUTORS
#endif

#include <boost/thread/future.hpp>
#include <boost/thread/concurrent_queues/queue_op_status.hpp>

#include "asiodefs.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** An io_service-based executor that can be used with continuations.

    This class wraps a `boost::asio::io_service` and models a
    [boost::executors::loop_executor][loop_executor]. Work submitted to this
    executor is posted to the `io_service`.

    The purpose of this executor is to be able to use continuations on futures
    returned by FutuSession. By default, continuations are run in their own
    asynchronous thread. This is dangerous, because FutuSession is not
    thread-safe. AsioExecutor safeguard against this by posting continuations
    to `boost::asio::io_service`.

    [loop_executor]:
        http://www.boost.org/doc/libs/release/doc/html/thread/synchronization.html#thread.synchronization.executors.ref.loop_executor

    @see FutuSession */
//------------------------------------------------------------------------------
class AsioExecutor
{
public:
    using ClosedException = boost::sync_queue_is_closed;

    AsioExecutor(AsioService& iosvc);

    ~AsioExecutor();

    AsioService& iosvc();

    const AsioService& iosvc() const;

    void close();

    bool closed() const;

    template <typename TClosure>
    void submit(TClosure&& work);

    bool try_executing_one();

    template <typename TPredicate>
    bool reschedule_until(const TPredicate& predicate);

    void loop();

    void run_queued_closures();

    // Non-copyable
    AsioExecutor(const AsioExecutor&) = delete;
    AsioExecutor& operator=(const AsioExecutor&) = delete;

private:
    AsioService& iosvc_;
    bool isClosed_ = false;
};


//------------------------------------------------------------------------------
template <typename TClosure>
void AsioExecutor::submit(TClosure&& work)
{
    if (isClosed_)
        throw ClosedException();
    iosvc_.post(std::forward<TClosure>(work));
}

//------------------------------------------------------------------------------
template <typename TPredicate>
bool AsioExecutor::reschedule_until(const TPredicate& predicate)
{
    bool atLeastOne = false;
    do
    {
        if (iosvc().run_one() != 0)
            atLeastOne = true;
    } while ( !predicate() && !iosvc().stopped() );
    return atLeastOne;
}

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "./internal/asioexecutor.ipp"
#endif

#endif // CPPWAMP_ASIOEXECUTOR_HPP
