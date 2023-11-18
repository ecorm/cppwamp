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
class CPPWAMP_API ProgressiveTimeout
{
public:
    ProgressiveTimeout() = default;

    ProgressiveTimeout(Timeout max) : max_(internal::checkTimeout(max)) {}

    ProgressiveTimeout(Timeout min, std::size_t minRate,
                       Timeout max = unspecifiedTimeout)
        : min_(internal::checkTimeout(min)),
          max_(internal::checkTimeout(max)),
          minRate_(minRate)
    {}

    Timeout min() const {return min_;}

    Timeout max() const {return max_;}

    std::size_t minRate() const {return minRate_;}

private:
    Timeout min_ = unspecifiedTimeout;
    Timeout max_ = unspecifiedTimeout;
    std::size_t minRate_ = 0;
};

//------------------------------------------------------------------------------
/** Contains general timeouts and size limits for client transports. */
//------------------------------------------------------------------------------
template <typename TDerived>
class BasicClientLimits
{
public:
    TDerived& withRxMsgSize(std::size_t n) {return set(rxMsgSize_, n);}

    TDerived& withTxMsgSize(std::size_t n) {return set(txMsgSize_, n);}

    std::size_t rxMsgSize() const {return rxMsgSize_;}

    std::size_t txMsgSize() const {return txMsgSize_;}

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

    Timeout lingerTimeout_    = neverTimeout;
    std::size_t rxMsgSize_    = 16*1024*1024;
    std::size_t txMsgSize_    = 16*1024*1024;
};

//------------------------------------------------------------------------------
/** Contains general timeouts and size limits for server transports. */
//------------------------------------------------------------------------------
template <typename TDerived>
class BasicServerLimits
{
public:
    TDerived& withRxMsgSize(std::size_t n) {return set(rxMsgSize_, n);}

    TDerived& withTxMsgSize(std::size_t n) {return set(txMsgSize_, n);}

    TDerived& withHandshakeTimeout(Timeout t) {return set(handshakeTimeout_, t);}

    TDerived& withRxMsgTimeout(ProgressiveTimeout t) {return set(rxMsgTimeout_, t);}

    TDerived& withTxMsgTimeout(ProgressiveTimeout t) {return set(txMsgTimeout_, t);}

    TDerived& withIdleTimeout(Timeout t) {return set(idleTimeout_, t);}

    TDerived& withLingerTimeout(Timeout t) {return set(idleTimeout_, t);}

    TDerived& withBacklogCapacity(int n)
    {
        CPPWAMP_LOGIC_CHECK(n > 0, "Backlog capacity must be positive");
        backlogCapacity_ = n;
        return derived();
    }

    std::size_t rxMsgSize() const {return rxMsgSize_;}

    std::size_t txMsgSize() const {return txMsgSize_;}

    Timeout handshakeTimeout() const {return handshakeTimeout_;}

    const ProgressiveTimeout& rxMsgTimeout() const {return rxMsgTimeout_;}

    const ProgressiveTimeout& sendTimeout() const {return txMsgTimeout_;}

    Timeout idleTimeout() const {return idleTimeout_;}

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

    TDerived& derived() {return static_cast<TDerived&>(*this);}

    ProgressiveTimeout rxMsgTimeout_;
    ProgressiveTimeout txMsgTimeout_;
    Timeout handshakeTimeout_ = neverTimeout;
    Timeout idleTimeout_      = neverTimeout;
    Timeout lingerTimeout_    = neverTimeout;
    std::size_t rxMsgSize_    = 16*1024*1024;
    std::size_t txMsgSize_    = 16*1024*1024;
    int backlogCapacity_      = 0;
};

} // namespace wamp

#endif // CPPWAMP_TRANSPORTLIMITS_HPP
