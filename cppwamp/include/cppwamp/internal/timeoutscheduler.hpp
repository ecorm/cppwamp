/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TIMEOUT_SCHEDULER_HPP
#define CPPWAMP_INTERNAL_TIMEOUT_SCHEDULER_HPP

#include <cassert>
#include <chrono>
#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include "../asiodefs.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TKey>
struct TimeoutRecord
{
    using Key = TKey;
    using Clock = std::chrono::steady_clock;
    using Duration = Clock::duration;
    using Timepoint = Clock::time_point;

    TimeoutRecord() = default;

    TimeoutRecord(Duration timeout, Key key)
        : deadline(Clock::now() + timeout),
          key(std::move(key))
    {
        // TODO: Prevent overflow
    }

    bool operator<(const TimeoutRecord& rhs) const
    {
        return std::tie(deadline, key) < std::tie(rhs.deadline, rhs.key);
    }

    Timepoint deadline;
    Key key = {};
};

//------------------------------------------------------------------------------
template <typename TKey>
class TimeoutScheduler
    : public std::enable_shared_from_this<TimeoutScheduler<TKey>>
{
public:
    using Key = TKey;
    using Duration = std::chrono::steady_clock::duration;
    using TimeoutHandler = std::function<void (Key)>;

    using Ptr = std::shared_ptr<TimeoutScheduler>;

    static Ptr create(IoStrand strand)
    {
        return Ptr(new TimeoutScheduler(std::move(strand)));
    }

    ~TimeoutScheduler()
    {
        deadlines_.clear();
        timeoutHandler_ = nullptr;
    }

    void listen(TimeoutHandler handler)
    {
        timeoutHandler_ = std::move(handler);
    }

    void unlisten() {timeoutHandler_ = nullptr;}

    void insert(Duration timeout, Key key)
    {
        // The first record represents the deadline being waited on
        // by the timer.

        Record rec{timeout, std::move(key)};
        bool wasIdle = deadlines_.empty();
        bool preemptsCurrentDeadline =
            !wasIdle && (rec < *deadlines_.begin());

        auto inserted = deadlines_.insert(rec);
        assert(inserted.second);
        if (wasIdle)
            processNextDeadline();
        else if (preemptsCurrentDeadline)
            timer_.cancel();
    }

    void erase(Key key)
    {
        if (deadlines_.empty())
            return;

        auto rec = deadlines_.begin();
        if (rec->key == key)
        {
            deadlines_.erase(rec);
            timer_.cancel();
            return;
        }

        // The set should be small, so just do a linear search.
        auto end = deadlines_.end();
        for (; rec != end; ++rec)
        {
            if (rec->key == key)
            {
                deadlines_.erase(rec);
                return;
            }
        }
    }

    void clear()
    {
        deadlines_.clear();
        timer_.cancel();
    }

private:
    using WeakPtr = std::weak_ptr<TimeoutScheduler>;
    using Record = TimeoutRecord<Key>;

    explicit TimeoutScheduler(IoStrand strand)
        : timer_(std::move(strand))
    {}

    void processNextDeadline()
    {
        auto deadline = deadlines_.begin()->deadline;
        auto key = deadlines_.begin()->key;
        timer_.expires_at(deadline);
        WeakPtr self(this->shared_from_this());
        timer_.async_wait([self, key](boost::system::error_code ec)
        {
            auto ptr = self.lock();
            if (ptr)
                ptr->onTimer(ec, key);
        });
    }

    void onTimer(boost::system::error_code ec, Key key)
    {
        if (!deadlines_.empty())
        {
            auto top = deadlines_.begin();
            bool preempted = top->key != key;
            if (!preempted)
            {
                if (!ec && timeoutHandler_)
                    timeoutHandler_(top->key);
                deadlines_.erase(top);
            }
            if (!deadlines_.empty())
                processNextDeadline();
        }
    }

    // std::map<Timepoint, Key> cannot be used because deadlines are
    // not guaranteed to be unique.
    std::set<Record> deadlines_;
    boost::asio::steady_timer timer_;
    TimeoutHandler timeoutHandler_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TIMEOUT_SCHEDULER_HPP
