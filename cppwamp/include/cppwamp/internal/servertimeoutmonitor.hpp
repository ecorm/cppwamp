/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_SERVERTIMEOUTMONITOR_HPP
#define CPPWAMP_INTERNAL_SERVERTIMEOUTMONITOR_HPP

#include <memory>
#include <type_traits>
#include "../errorcodes.hpp"
#include "../transportlimits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class ProgressiveDeadline
{
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    void reset()
    {
        deadline_ = TimePoint::max();
        maxDeadline_ = TimePoint::max();
        bytesBanked_ = 0;
    }

    void start(const ProgressiveTimeout& timeout, TimePoint now)
    {
        const bool minTimeoutUnspecifed = timeout.min() == unspecifiedTimeout;
        const bool maxTimeoutUnspecifed = timeout.max() == unspecifiedTimeout;
        maxDeadline_ = maxTimeoutUnspecifed ? TimePoint::max()
                                            : now + timeout.max();
        deadline_ = minTimeoutUnspecifed ? maxDeadline_ : now + timeout.min();
    }

    void update(const ProgressiveTimeout& timeout, std::size_t bytesTransferred)
    {
        if (deadline_ == maxDeadline_)
            return;

        if (timeout.min() != unspecifiedTimeout && timeout.rate() != 0)
        {
            auto n = bytesBanked_ + bytesTransferred;
            auto secs = n / timeout.rate();
            bytesBanked_ = n - (secs * timeout.rate());

            auto headroom = std::chrono::duration_cast<std::chrono::seconds>(
                maxDeadline_ - deadline_).count();

            using U = std::make_unsigned<decltype(headroom)>::type;
            if (secs > static_cast<U>(headroom))
                deadline_ = maxDeadline_;
            else
                deadline_ += std::chrono::seconds{secs};
        }
    }

    TimePoint due() const {return deadline_;}

private:
    TimePoint deadline_ = TimePoint::max();
    TimePoint maxDeadline_ = TimePoint::max();
    std::size_t bytesBanked_ = 0;
};

//------------------------------------------------------------------------------
template <typename TSettings>
class ServerTimeoutMonitor
{
public:
    using Settings = TSettings;
    using SettingsPtr = std::shared_ptr<Settings>;
    using TimePoint = std::chrono::steady_clock::time_point;

    explicit ServerTimeoutMonitor(SettingsPtr settings)
        : settings_(std::move(settings))
    {}

    void start(TimePoint now) {bumpActivityDeadline(now);}

    void startRead(TimePoint now)
    {
        readDeadline_.start(settings_->limits().readTimeout(), now);
        bumpActivityDeadline(now);
    }

    void updateRead(TimePoint now, std::size_t bytesRead)
    {
        readDeadline_.update(settings_->limits().readTimeout(), bytesRead);
        bumpActivityDeadline(now);
    }

    void endRead(TimePoint now)
    {
        readDeadline_.reset();
        bumpActivityDeadline(now);
    }

    void startWrite(TimePoint now)
    {
        writeDeadline_.start(settings_->limits().writeTimeout(), now);
        bumpActivityDeadline(now);
    }

    void updateWrite(TimePoint now, std::size_t bytesWritten)
    {
        writeDeadline_.update(settings_->limits().writeTimeout(), bytesWritten);
        bumpActivityDeadline(now);
    }

    void endWrite(TimePoint now)
    {
        writeDeadline_.reset();
        bumpActivityDeadline(now);
    }

    std::error_code check(TimePoint now) const
    {
        if (now >= activityDeadline_)
            return make_error_code(TransportErrc::idleTimeout);

        if (now >= readDeadline_.due())
            return make_error_code(TransportErrc::readTimeout);

        if (now >= writeDeadline_.due())
            return make_error_code(TransportErrc::writeTimeout);

        return {};
    }

private:
    void bumpActivityDeadline(TimePoint now)
    {
        auto timeout = settings_->limits().idleTimeout();
        if (internal::timeoutIsDefinite(timeout))
            activityDeadline_ = now + timeout;
    }

    internal::ProgressiveDeadline readDeadline_;
    internal::ProgressiveDeadline writeDeadline_;
    TimePoint activityDeadline_ = TimePoint::max();
    SettingsPtr settings_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_SERVERTIMEOUTMONITOR_HPP
