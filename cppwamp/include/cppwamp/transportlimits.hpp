/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTLIMITS_HPP
#define CPPWAMP_TRANSPORTLIMITS_HPP

#include <cstddef>
#include "api.hpp"
#include "timeout.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
class CPPWAMP_API IncrementalTimeout
{
public:
    constexpr IncrementalTimeout() = default;

    constexpr IncrementalTimeout(Timeout max) : max_(max) {}

    constexpr IncrementalTimeout(Timeout min, std::size_t rate,
                                 Timeout max = unspecifiedTimeout)
        : min_(min), max_(max), rate_(rate)
    {}

    constexpr Timeout min() const {return min_;}

    constexpr Timeout max() const {return max_;}

    /** Obtains the number of transferred bytes needed per additional second
        added to the mininum timeout. */
    constexpr std::size_t rate() const {return rate_;}

    IncrementalTimeout& validate()
    {
        internal::checkTimeout(min_);
        internal::checkTimeout(max_);
        CPPWAMP_LOGIC_CHECK(min_ == unspecifiedTimeout || rate_ != 0,
                            "Rate cannot be zero when min timeout is specified");
        return *this;
    }

private:
    Timeout min_ = unspecifiedTimeout;
    Timeout max_ = std::chrono::seconds{15}; // Using ejabber's send_timeout
    std::size_t rate_ = 0;
};

//------------------------------------------------------------------------------
/** Contains general timeouts and size limits for client transports. */
//------------------------------------------------------------------------------
template <typename TDerived>
class BasicClientTransportLimits
{
public:
    TDerived& withRxMsgSize(std::size_t n) {return set(readMsgSize_, n);}

    TDerived& withTxMsgSize(std::size_t n) {return set(writeMsgSize_, n);}

    TDerived& withLingerTimeout(Timeout t) {return set(lingerTimeout_, t);}

    std::size_t readMsgSize() const {return readMsgSize_;}

    std::size_t writeMsgSize() const {return writeMsgSize_;}

    Timeout lingerTimeout() const {return lingerTimeout_;}

private:
    template <typename T>
    TDerived& set(T& member, T value)
    {
        member = value;
        return derived();
    }

    TDerived& set(Timeout& member, Timeout t)
    {
        member = internal::checkTimeout(t);
        return derived();
    }

    TDerived& derived() {return static_cast<TDerived&>(*this);}

    Timeout lingerTimeout_ = std::chrono::milliseconds{1000};
        // Using Gecko's kLingeringCloseTimeout
    std::size_t readMsgSize_ = 16*1024*1024;
        // Using WAMP's raw socket maximum payload length
    std::size_t writeMsgSize_ = 16*1024*1024;
        // Using WAMP's raw socket maximum payload length
};

//------------------------------------------------------------------------------
/** Contains general timeouts and size limits for server transports. */
//------------------------------------------------------------------------------
template <typename TDerived>
class BasicServerTransportLimits
{
public:
    TDerived& withReadMsgSize(std::size_t n) {return set(readMsgSize_, n);}

    TDerived& withWriteMsgSize(std::size_t n) {return set(writeMsgSize_, n);}

    TDerived& withHandshakeTimeout(Timeout t) {return set(handshakeTimeout_, t);}

    TDerived& withReadTimeout(IncrementalTimeout t) {return set(readTimeout_, t);}

    TDerived& withWriteTimeout(IncrementalTimeout t) {return set(writeTimeout_, t);}

    TDerived& withSilenceTimeout(Timeout t) {return set(silenceTimeout_, t);}

    TDerived& withInactivityTimeout(Timeout t) {return set(inactivityTimeout_, t);}

    TDerived& withLingerTimeout(Timeout t) {return set(lingerTimeout_, t);}

    TDerived& withBacklogCapacity(int n)
    {
        CPPWAMP_LOGIC_CHECK(n > 0, "Backlog capacity must be positive");
        backlogCapacity_ = n;
        return derived();
    }

    std::size_t readMsgSize() const {return readMsgSize_;}

    std::size_t writeMsgSize() const {return writeMsgSize_;}

    Timeout handshakeTimeout() const {return handshakeTimeout_;}

    const IncrementalTimeout& readTimeout() const {return readTimeout_;}

    const IncrementalTimeout& writeTimeout() const {return writeTimeout_;}

    /** Obtains the maximum time of no data being transferred, including
        pings. */
    Timeout silenceTimeout() const {return silenceTimeout_;}

    /** Obtains the maximum time of no data being transferred, excluding
        heartbeats. This prevents clients indefinitely keeping a connection
        alive by just sending pings. */
    Timeout inactivityTimeout() const {return inactivityTimeout_;}

    /** Obtains the maxiumum time the server will wait for a client to
        gracefully close the connection. */
    Timeout lingerTimeout() const {return lingerTimeout_;}

    int backlogCapacity() const {return backlogCapacity_;}

private:
    template <typename T>
    TDerived& set(T& member, T value)
    {
        member = value;
        return derived();
    }

    TDerived& set(Timeout& member, Timeout t)
    {
        member = internal::checkTimeout(t);
        return derived();
    }

    TDerived& set(IncrementalTimeout& member, IncrementalTimeout t)
    {
        member = t.validate();
        return derived();
    }

    TDerived& derived() {return static_cast<TDerived&>(*this);}

    IncrementalTimeout readTimeout_;
    IncrementalTimeout writeTimeout_;
    Timeout handshakeTimeout_ = std::chrono::seconds{30};
        // Using ejabberd's negotiation_timeout
    Timeout silenceTimeout_ = std::chrono::seconds{300};
        // Using ejabberd's websocket_timeout
    Timeout inactivityTimeout_ = std::chrono::minutes(60);
        // Using Nginx's keepalive_time
    Timeout lingerTimeout_ = std::chrono::seconds{30};
        // Using Nginx's lingering_time
    std::size_t readMsgSize_ = 16*1024*1024;
        // Using WAMP's raw socket maximum payload length
    std::size_t writeMsgSize_ = 16*1024*1024;
        // Using WAMP's raw socket maximum payload length
    int backlogCapacity_ = 0; // Use Asio's default by default
};

} // namespace wamp

#endif // CPPWAMP_TRANSPORTLIMITS_HPP
