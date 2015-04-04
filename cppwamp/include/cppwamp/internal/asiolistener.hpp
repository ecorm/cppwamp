/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ASIOLISTENER_HPP
#define CPPWAMP_INTERNAL_ASIOLISTENER_HPP

#include <cstdint>
#include <utility>
#include <vector>
#include "../error.hpp"
#include "../rawsockdefs.hpp"
#include "asioendpoint.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TEstablisher>
class AsioListener : public AsioEndpoint<TEstablisher>
{
public:
    using Establisher    = TEstablisher;

    AsioListener(Establisher&& est, CodecIds codecIds,
                 RawsockMaxLength maxRxLength)
        : Base(std::move(est)),
          codecIds_(std::move(codecIds)),
          maxTxLength_(RawsockMaxLength::kB_64),
          maxRxLength_(maxRxLength)
    {}

private:
    using Base = AsioEndpoint<TEstablisher>;

protected:
    using Handshake = typename Base::Handshake;

    virtual void onEstablished() override {Base::receiveHandshake();}

    virtual void onHandshakeReceived(Handshake hs) override
    {
        auto peerCodec = hs.codec();

        if (!hs.hasMagicOctet())
            Base::fail(RawsockErrc::badHandshake);
        else if (hs.reserved() != 0)
            Base::sendHandshake(Handshake::eReservedBitsUsed());
        else if (codecIds_.count(peerCodec))
        {
            maxTxLength_ = hs.maxLength();
            Base::sendHandshake(Handshake().setMaxLength(maxRxLength_)
                                           .setCodec(peerCodec));
        }
        else
            Base::sendHandshake(Handshake::eUnsupportedFormat());
    }

    virtual void onHandshakeSent(Handshake hs) override
    {
        if (!hs.hasError())
            Base::complete(hs.codec(), Handshake::byteLengthOf(maxTxLength_),
                           Handshake::byteLengthOf(maxRxLength_));
        else
            Base::fail(hs.errorCode());
    }

private:
    CodecIds codecIds_;
    RawsockMaxLength maxTxLength_;
    RawsockMaxLength maxRxLength_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ASIOLISTENER_HPP
