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

    void start(TimePoint now)
    {
        bumpLoiterDeadline(now);
        auto timeout = settings_->limits().overstayTimeout();
        if (internal::timeoutIsDefinite(timeout))
            overstayDeadline_ = now + timeout;
    }

    void startRead(TimePoint now)
    {
        readDeadline_.start(settings_->limits().readTimeout(), now);
        bumpLoiterDeadline(now);
        isReading_ = true;
    }

    void updateRead(TimePoint now, std::size_t bytesRead)
    {
        readDeadline_.update(settings_->limits().readTimeout(), bytesRead);
        bumpLoiterDeadline(now);
    }

    void endRead(TimePoint now)
    {
        readDeadline_.reset();
        bumpLoiterDeadline(now);
        isReading_ = false;
    }

    void startWrite(TimePoint now)
    {
        writeDeadline_.start(settings_->limits().writeTimeout(), now);
        bumpLoiterDeadline(now);
        isWriting_ = true;
    }

    void updateWrite(TimePoint now, std::size_t bytesWritten)
    {
        writeDeadline_.update(settings_->limits().writeTimeout(), bytesWritten);
        bumpLoiterDeadline(now);
    }

    void endWrite(TimePoint now)
    {
        writeDeadline_.reset();
        bumpLoiterDeadline(now);
        isWriting_ = false;
    }

    void heartbeat(TimePoint now)
    {
        bumpSilenceDeadline(now);
    }

    std::error_code check(TimePoint now) const
    {
        std::error_code ec;

        if (now >= readDeadline_.due())
            ec = make_error_code(TransportErrc::readTimeout);
        else if (now >= writeDeadline_.due())
            ec = make_error_code(TransportErrc::writeTimeout);
        else if (now >= silenceDeadline_)
            ec = make_error_code(TransportErrc::silenceTimeout);
        else if (now >= loiterDeadline_)
            ec = make_error_code(TransportErrc::loiterTimeout);
        else if (!isReading_ && !isWriting_ && now >= overstayDeadline_)
            ec = make_error_code(TransportErrc::overstayTimeout);

        return ec;
    }

private:
    void bumpLoiterDeadline(TimePoint now)
    {
        bumpSilenceDeadline(now);

        auto timeout = settings_->limits().loiterTimeout();
        if (internal::timeoutIsDefinite(timeout))
            loiterDeadline_ = now + timeout;
    }

    void bumpSilenceDeadline(TimePoint now)
    {
        auto timeout = settings_->limits().silenceTimeout();
        if (internal::timeoutIsDefinite(timeout))
            silenceDeadline_ = now + timeout;
    }

    internal::ProgressiveDeadline readDeadline_;
    internal::ProgressiveDeadline writeDeadline_;
    TimePoint silenceDeadline_ = TimePoint::max();
    TimePoint loiterDeadline_ = TimePoint::max();
    TimePoint overstayDeadline_ = TimePoint::max();
    SettingsPtr settings_;
    bool isReading_ = false;
    bool isWriting_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_SERVERTIMEOUTMONITOR_HPP
