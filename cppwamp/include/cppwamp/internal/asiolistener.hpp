/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ASIOLISTENER_HPP
#define CPPWAMP_INTERNAL_ASIOLISTENER_HPP

#include <cstdint>
#include <set>
#include <utility>
#include "../error.hpp"
#include "../transport.hpp"
#include "../rawsockoptions.hpp"
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
    using Establisher = TEstablisher;
    using CodecIds    = std::set<int>;

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
        auto peerCodec = hs.codecId();

        if (!hs.hasMagicOctet())
            Base::fail(RawsockErrc::badHandshake);
        else if (hs.reserved() != 0)
            Base::sendHandshake(Handshake::eReservedBitsUsed());
        else if (codecIds_.count(peerCodec))
        {
            maxTxLength_ = hs.maxLength();
            Base::sendHandshake(Handshake().setMaxLength(maxRxLength_)
                                           .setCodecId(peerCodec));
        }
        else
            Base::sendHandshake(Handshake::eUnsupportedFormat());
    }

    virtual void onHandshakeSent(Handshake hs) override
    {
        if (!hs.hasError())
            Base::complete(hs.codecId(), limits());
        else
            Base::fail(hs.errorCode());
    }

private:
    TransportLimits limits() const
    {
        return {Handshake::byteLengthOf(maxTxLength_),
                Handshake::byteLengthOf(maxRxLength_)};
    }

    CodecIds codecIds_;
    RawsockMaxLength maxTxLength_;
    RawsockMaxLength maxRxLength_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ASIOLISTENER_HPP
