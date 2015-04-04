/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_LEGACYASIOENDPOINT_HPP
#define CPPWAMP_INTERNAL_LEGACYASIOENDPOINT_HPP

#include <utility>
#include "../asiodefs.hpp"
#include "../rawsockdefs.hpp"
#include "asioendpoint.hpp"
#include "legacyasiotransport.hpp"
#include "rawsockhandshake.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TEstablisher>
class LegacyAsioEndpoint :
        public AsioEndpoint<TEstablisher, LegacyAsioTransport>
{
public:
    using Establisher = TEstablisher;

    LegacyAsioEndpoint(Establisher&& est, CodecId codecId,
                       RawsockMaxLength maxLength)
        : Base(std::move(est)),
          codecId_(codecId),
          maxLength_(maxLength)
    {}

protected:
    using Handshake = internal::RawsockHandshake;

    virtual void onEstablished() override
    {
        Base::complete(codecId_,
                       RawsockHandshake::byteLengthOf(maxLength_),
                       RawsockHandshake::byteLengthOf(maxLength_));
    }

    virtual void onHandshakeReceived(Handshake) override {}

    virtual void onHandshakeSent(Handshake) override {}

private:
    using Base = AsioEndpoint<TEstablisher, LegacyAsioTransport>;

    CodecId codecId_;
    RawsockMaxLength maxLength_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_LEGACYASIOENDPOINT_HPP
