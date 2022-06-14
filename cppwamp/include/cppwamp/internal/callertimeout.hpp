/*------------------------------------------------------------------------------
              Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CALLER_TIMEOUT_HPP
#define CPPWAMP_INTERNAL_CALLER_TIMEOUT_HPP

#include <chrono>
#include <memory>
#include <set>
#include <utility>
#include <boost/asio/steady_timer.hpp>
#include "../asiodefs.hpp"
#include "../peerdata.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct CallerTimeoutRecord
{
    using Clock = std::chrono::steady_clock;
    using Duration = Clock::duration;
    using Timepoint = Clock::time_point;

    CallerTimeoutRecord() = default;

    CallerTimeoutRecord(Duration timeout, RequestId rid)
        : deadline(Clock::now() + timeout),
        requestId(rid)
    {}

    bool operator<(const CallerTimeoutRecord& rhs) const
    {
        return deadline < rhs.deadline;
    }

    Timepoint deadline;
    RequestId requestId = 0;
};

//------------------------------------------------------------------------------
class CallerTimeoutScheduler :
    public std::enable_shared_from_this<CallerTimeoutScheduler>
{
public:
    using Duration = std::chrono::steady_clock::duration;
    using TimeoutHandler = std::function<void (RequestId)>;

    using Ptr = std::shared_ptr<CallerTimeoutScheduler>;

    static Ptr create(AnyExecutor exec)
    {
        return Ptr(new CallerTimeoutScheduler(std::move(exec)));
    }

    void listen(TimeoutHandler handler)
    {
        timeoutHandler_ = std::move(handler);
    }

    void add(Duration timeout, RequestId rid)
    {
        // The first record represents a deadline being waited on
        // by the timer.

        CallerTimeoutRecord rec{timeout, rid};
        bool wasIdle = deadlines_.empty();
        bool preemptsCurrentDeadline =
            !wasIdle && (rec < *deadlines_.begin());

        deadlines_.insert(rec);
        if (wasIdle)
            processNextDeadline();
        else if (preemptsCurrentDeadline)
            timer_.cancel();
    }

    void remove(RequestId rid)
    {
        if (deadlines_.empty())
            return;

        auto rec = deadlines_.begin();
        if (rec->requestId == rid)
        {
            deadlines_.erase(rec);
            timer_.cancel();
            return;
        }

        // The set should be small, so just do a linear search.
        auto end = deadlines_.end();
        for (; rec != end; ++rec)
        {
            if (rec->requestId == rid)
            {
                deadlines_.erase(rec);
                return;
            }
        }
    }

    void clear()
    {
        timeoutHandler_ = nullptr;
        deadlines_.clear();
        timer_.cancel();
    }

private:
    using WeakPtr = std::weak_ptr<CallerTimeoutScheduler>;

    explicit CallerTimeoutScheduler(AnyExecutor exec)
        : timer_(std::move(exec))
    {}

    void processNextDeadline()
    {
        auto deadline = deadlines_.begin()->deadline;
        auto requestId = deadlines_.begin()->requestId;
        timer_.expires_at(deadline);
        WeakPtr self(shared_from_this());
        timer_.async_wait([self, requestId](boost::system::error_code ec)
        {
            auto ptr = self.lock();
            if (ptr)
                ptr->onTimer(ec, requestId);
        });
    }

    void onTimer(boost::system::error_code ec, RequestId requestId)
    {
        if (!deadlines_.empty())
        {
            auto top = deadlines_.begin();
            bool preempted = top->requestId != requestId;
            if (!preempted)
            {
                if (!ec && timeoutHandler_)
                    timeoutHandler_(top->requestId);
                deadlines_.erase(top);
            }
            if (!deadlines_.empty())
                processNextDeadline();
        }
    }

    std::set<CallerTimeoutRecord> deadlines_;
    boost::asio::steady_timer timer_;
    TimeoutHandler timeoutHandler_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CALLER_TIMEOUT_HPP
