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

    void start(const IncrementalTimeout& timeout, TimePoint now)
    {
        const bool minTimeoutUnspecifed = timeout.min() == unspecifiedTimeout;
        const bool maxTimeoutUnspecifed = timeout.max() == unspecifiedTimeout;
        maxDeadline_ = maxTimeoutUnspecifed ? TimePoint::max()
                                            : now + timeout.max();
        deadline_ = minTimeoutUnspecifed ? maxDeadline_ : now + timeout.min();
    }

    void update(const IncrementalTimeout& timeout, std::size_t bytesTransferred)
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
        bumpInactivityDeadline(now);
    }

    void stop()
    {
        readDeadline_.reset();
        writeDeadline_.reset();
        handshakeDeadline_ = TimePoint::max();
        silenceDeadline_ = TimePoint::max();
        inactivityDeadline_ = TimePoint::max();
        lingerDeadline_ = TimePoint::max();
    }

    void startRead(TimePoint now)
    {
        readDeadline_.start(settings_->limits().readTimeout(), now);
        bumpInactivityDeadline(now);
        isReading_ = true;
    }

    void updateRead(TimePoint now, std::size_t bytesRead)
    {
        readDeadline_.update(settings_->limits().readTimeout(), bytesRead);
        bumpInactivityDeadline(now);
    }

    void endRead(TimePoint now)
    {
        readDeadline_.reset();
        bumpInactivityDeadline(now);
        isReading_ = false;
    }

    void startWrite(TimePoint now, bool bumpInactivity)
    {
        writeDeadline_.start(settings_->limits().writeTimeout(), now);
        if (bumpInactivity)
            bumpInactivityDeadline(now);
        isWriting_ = true;
    }

    void updateWrite(TimePoint now, std::size_t bytesWritten)
    {
        writeDeadline_.update(settings_->limits().writeTimeout(), bytesWritten);
        bumpInactivityDeadline(now);
    }

    void endWrite(TimePoint now, bool bumpInactivity)
    {
        writeDeadline_.reset();
        if (bumpInactivity)
            bumpInactivityDeadline(now);
        isWriting_ = false;
    }

    void heartbeat(TimePoint now)
    {
        bumpSilenceDeadline(now);
    }

    // This is only used by QueuingTransport when shutting down the admitter,
    // as the TransportQueue has its own linger timeout mechanism via the
    // Bouncer policy.
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
    void bumpInactivityDeadline(TimePoint now)
    {
        bumpSilenceDeadline(now);

        auto timeout = settings_->limits().inactivityTimeout();
        if (internal::timeoutIsDefinite(timeout))
            inactivityDeadline_ = now + timeout;
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
        if (now >= inactivityDeadline_)
            return TransportErrc::inactivityTimeout;
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
    TimePoint inactivityDeadline_ = TimePoint::max();
    TimePoint lingerDeadline_ = TimePoint::max();
    SettingsPtr settings_;
    bool isReading_ = false;
    bool isWriting_ = false;
};

//------------------------------------------------------------------------------
template <typename TSettings>
class HttpServerTimeoutMonitor
{
public:
    using Settings = TSettings;
    using SettingsPtr = std::shared_ptr<Settings>;
    using TimePoint = std::chrono::steady_clock::time_point;

    explicit HttpServerTimeoutMonitor(SettingsPtr settings)
        : settings_(std::move(settings))
    {}

    void startHeader(TimePoint now)
    {
        auto timeout = settings_->limits().headerTimeout();
        if (timeoutIsDefinite(timeout))
            headerDeadline_ = now + timeout;
        keepaliveDeadline_ = TimePoint::max();
    }

    void endHeader()
    {
        headerDeadline_ = TimePoint::max();
    }

    void startBody(TimePoint now)
    {
        bodyDeadline_.start(settings_->limits().bodyTimeout(), now);
    }

    void updateBody(TimePoint now, std::size_t bytesRead)
    {
        bodyDeadline_.update(settings_->limits().bodyTimeout(), bytesRead);
    }

    void endBody()
    {
        bodyDeadline_.reset();
    }

    void startResponse(TimePoint now)
    {
        responseDeadline_.start(settings_->limits().responseTimeout(), now);
    }

    void updateResponse(TimePoint now, std::size_t bytesWritten)
    {
        responseDeadline_.update(settings_->limits().writeTimeout(),
                                 bytesWritten);
    }

    void endResponse(TimePoint now, bool keepAlive)
    {
        responseDeadline_.reset();

        if (keepAlive)
        {
            auto timeout = settings_->limits().keepaliveTimeout();
            if (timeoutIsDefinite(timeout))
                keepaliveDeadline_ = now + timeout;
        }
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
    TransportErrc checkForTimeouts(TimePoint now) const
    {
        if (now >= headerDeadline_)
            return TransportErrc::readTimeout;
        if (now >= bodyDeadline_.due())
            return TransportErrc::readTimeout;
        if (now >= responseDeadline_.due())
            return TransportErrc::writeTimeout;
        if (now >= keepaliveDeadline_)
            return TransportErrc::inactivityTimeout;
        if (now >= lingerDeadline_)
            return TransportErrc::lingerTimeout;
        return TransportErrc::success;
    }

    internal::ProgressiveDeadline responseDeadline_;
    internal::ProgressiveDeadline bodyDeadline_;
    TimePoint headerDeadline_ = TimePoint::max();
    TimePoint keepaliveDeadline_ = TimePoint::max();
    TimePoint lingerDeadline_ = TimePoint::max();
    SettingsPtr settings_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_SERVERTIMEOUTMONITOR_HPP
