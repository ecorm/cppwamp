/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TIMEOUT_SCHEDULER_HPP
#define CPPWAMP_INTERNAL_TIMEOUT_SCHEDULER_HPP

#include <cassert>
#include <chrono>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <boost/asio/steady_timer.hpp>
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

    TimeoutRecord(Key key, Duration timeout)
        : key(std::move(key)),
          deadline(clampedDeadline(timeout))
    {}

    bool operator<(const TimeoutRecord& rhs) const
    {
        return std::tie(deadline, key) < std::tie(rhs.deadline, rhs.key);
    }

    Key key = {};
    Timepoint deadline;

private:
    static Timepoint clampedDeadline(Duration timeout)
    {
        using Limits = std::numeric_limits<typename Duration::rep>;
        auto now = Clock::now().time_since_epoch().count();
        auto ticks = timeout.count();
        if (ticks > 0)
        {
            auto limit = Limits::max() - now;
            if (ticks > limit)
                ticks = limit;
        }
        else
        {
            ticks = 0;
        }
        return Timepoint{Duration{now + ticks}};
    }
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

    void insert(Key key, Duration timeout)
    {
        // The first record contains the deadline being waited on
        // by the timer.

        Record rec{key, timeout};
        bool wasIdle = deadlines_.empty();
        bool preemptsCurrentDeadline =
            !wasIdle && (rec < *deadlines_.begin());

        auto inserted = deadlines_.insert(std::move(rec));
        assert(inserted.second);
        auto emplaced = byKey_.emplace(std::move(key), inserted.first);
        assert(emplaced.second);
        if (wasIdle)
            processNextDeadline();
        else if (preemptsCurrentDeadline)
            timer_.cancel();
    }

    void update(Key key, Duration timeout)
    {
        auto found = byKey_.find(key);
        if (found == byKey_.end())
            return;
        auto iter = found->second;
        bool invalidatesCurrentDeadline = (iter == deadlines_.begin());
        deadlines_.erase(iter);

        Record rec{key, timeout};
        invalidatesCurrentDeadline = invalidatesCurrentDeadline ||
                                     (rec < *deadlines_.begin());
        auto inserted = deadlines_.insert(std::move(rec));
        assert(inserted.second);
        found->second = inserted.first;

        if (invalidatesCurrentDeadline)
            timer_.cancel();
    }

    void erase(Key key)
    {
        auto found = byKey_.find(key);
        if (found == byKey_.end())
            return;
        auto iter = found->second;
        if (iter == deadlines_.begin())
            timer_.cancel();
        deadlines_.erase(iter);
        byKey_.erase(found);
    }

    void clear()
    {
        deadlines_.clear();
        timer_.cancel();
    }

private:
    using WeakPtr = std::weak_ptr<TimeoutScheduler>;
    using Record = TimeoutRecord<Key>;
    using RecordSet = std::set<Record>;

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
            if (!ec)
            {
                if (timeoutHandler_)
                    timeoutHandler_(top->key);
                deadlines_.erase(top);
            }
            if (!deadlines_.empty())
                processNextDeadline();
        }
    }

    // std::map<Timepoint, Key> cannot be used because deadlines are
    // not guaranteed to be unique.
    RecordSet deadlines_;
    std::map<Key, typename RecordSet::iterator> byKey_;
    boost::asio::steady_timer timer_;
    TimeoutHandler timeoutHandler_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TIMEOUT_SCHEDULER_HPP
