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

    void startHandshake(TimePoint now)
    {
        auto timeout = settings_->limits().handshakeTimeout();
        if (internal::timeoutIsDefinite(timeout))
            handshakeDeadline_ = now + timeout;
    }

    void endHandshake() {handshakeDeadline_ = TimePoint::max();}

    void start(TimePoint now)
    {
        bumpLoiterDeadline(now);
    }

    void stop()
    {
        readDeadline_.reset();
        writeDeadline_.reset();
        handshakeDeadline_ = TimePoint::max();
        silenceDeadline_ = TimePoint::max();
        loiterDeadline_ = TimePoint::max();
        lingerDeadline_ = TimePoint::max();
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

    void startWrite(TimePoint now, bool bumpLoiter)
    {
        writeDeadline_.start(settings_->limits().writeTimeout(), now);
        if (bumpLoiter)
            bumpLoiterDeadline(now);
        isWriting_ = true;
    }

    void updateWrite(TimePoint now, std::size_t bytesWritten)
    {
        writeDeadline_.update(settings_->limits().writeTimeout(), bytesWritten);
        bumpLoiterDeadline(now);
    }

    void endWrite(TimePoint now, bool bumpLoiter)
    {
        writeDeadline_.reset();
        if (bumpLoiter)
            bumpLoiterDeadline(now);
        isWriting_ = false;
    }

    void heartbeat(TimePoint now)
    {
        bumpSilenceDeadline(now);
    }

    void startLinger(TimePoint now)
    {
        auto timeout = settings_->limits().lingerTimeout();
        if (internal::timeoutIsDefinite(timeout))
            lingerDeadline_ = now + timeout;
    }

    void endLinger() {lingerDeadline_ = TimePoint::max();}

    std::error_code check(TimePoint now) const
    {
        return make_error_code(checkForTimeouts(now));
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

    TransportErrc checkForTimeouts(TimePoint now) const
    {
        if (now >= readDeadline_.due())
            return TransportErrc::readTimeout;
        if (now >= writeDeadline_.due())
            return TransportErrc::writeTimeout;
        if (now >= silenceDeadline_)
            return TransportErrc::silenceTimeout;
        if (now >= loiterDeadline_)
            return TransportErrc::loiterTimeout;
        if (now >= handshakeDeadline_)
            return TransportErrc::handshakeTimeout;
        if (now >= lingerDeadline_)
            return TransportErrc::lingerTimeout;
        return TransportErrc::success;
    }

    internal::ProgressiveDeadline readDeadline_;
    internal::ProgressiveDeadline writeDeadline_;
    TimePoint handshakeDeadline_ = TimePoint::max();
    TimePoint silenceDeadline_ = TimePoint::max();
    TimePoint loiterDeadline_ = TimePoint::max();
    TimePoint lingerDeadline_ = TimePoint::max();
    SettingsPtr settings_;
    bool isReading_ = false;
    bool isWriting_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_SERVERTIMEOUTMONITOR_HPP
