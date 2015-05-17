/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ASIOCONNECTOR_HPP
#define CPPWAMP_INTERNAL_ASIOCONNECTOR_HPP

#include <cstdint>
#include "../codec.hpp"
#include "../error.hpp"
#include "../rawsockoptions.hpp"
#include "asioendpoint.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TEstablisher>
class AsioConnector : public AsioEndpoint<TEstablisher>
{
public:
    using Establisher = TEstablisher;

    AsioConnector(Establisher&& est, int codecId, RawsockMaxLength maxRxLength)
        : Base(std::move(est)),
          codecId_(codecId),
          maxRxLength_(maxRxLength)
    {}

private:
    using Base = AsioEndpoint<TEstablisher>;

protected:
    using Handshake = typename Base::Handshake;

    virtual void onEstablished() override
    {
        Base::sendHandshake( Handshake().setCodecId(codecId_)
                                        .setMaxLength(maxRxLength_) );
    }

    virtual void onHandshakeSent(Handshake) override {Base::receiveHandshake();}

    virtual void onHandshakeReceived(Handshake hs) override
    {
        if (!hs.hasMagicOctet())
            Base::fail(RawsockErrc::badHandshake);
        else if (hs.reserved() != 0)
            Base::fail(RawsockErrc::reservedBitsUsed);
        else if (hs.codecId() == codecId_)
            Base::complete(codecId_, hs.maxLengthInBytes(),
                           Handshake::byteLengthOf(maxRxLength_));
        else if (hs.hasError())
            Base::fail(hs.errorCode());
        else
            Base::fail(RawsockErrc::badHandshake);
    }

private:
    int codecId_;
    RawsockMaxLength maxRxLength_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ASIOCONNECTOR_HPP
