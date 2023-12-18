/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORT_RAWSOCKLIMITS_HPP
#define CPPWAMP_TRANSPORT_RAWSOCKLIMITS_HPP

#include "../transportlimits.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains timeouts and size limits for raw socket client transports. */
//------------------------------------------------------------------------------
class CPPWAMP_API RawsockClientLimits
    : public BasicClientTransportLimits<RawsockClientLimits>
{
public:
    RawsockClientLimits& withHeartbeatSize(std::size_t n)
    {
        heartbeatSize_ = n;
        return *this;
    }

    std::size_t heartbeatSize() const {return heartbeatSize_;}

private:
    std::size_t heartbeatSize_ = 125; // Use same limit as Websocket by default
};

//------------------------------------------------------------------------------
/** Contains timeouts and size limits for raw socket server transports. */
//------------------------------------------------------------------------------
class CPPWAMP_API RawsockServerLimits
    : public BasicServerTransportLimits<RawsockServerLimits>
{
public:
    RawsockServerLimits& withHeartbeatSize(std::size_t n)
    {
        heartbeatSize_ = n;
        return *this;
    }

    std::size_t heartbeatSize() const {return heartbeatSize_;}

private:
    std::size_t heartbeatSize_ = 125; // Use same limit as Websocket by default
};

} // namespace wamp

#endif // CPPWAMP_TRANSPORT_RAWSOCKLIMITS_HPP
