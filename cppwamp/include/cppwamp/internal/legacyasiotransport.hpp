/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_LEGACYASIOTRANSPORT_HPP
#define CPPWAMP_LEGACYASIOTRANSPORT_HPP

#include "../error.hpp"
#include "asiotransport.hpp"
#include "endian.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TSocket>
class LegacyAsioTransport : public AsioTransport<TSocket>
{
private:
    using Base = AsioTransport<TSocket>;

public:
    using Ptr         = std::shared_ptr<LegacyAsioTransport>;
    using Socket      = TSocket;
    using SocketPtr   = typename Base::SocketPtr;
    using Buffer      = typename Base::Buffer;
    using PingHandler = typename Base::PingHandler;

    static Ptr create(SocketPtr&& socket, size_t maxTxLength,
                      size_t maxRxLength)
    {
        return Ptr(new LegacyAsioTransport(std::move(socket), maxTxLength,
                                           maxRxLength));
    }

    void ping(Buffer, PingHandler)
    {
        CPPWAMP_LOGIC_ERROR("Ping messages are not supported "
                            "on LegacyAsioTransport");
    }

    using Base::post;

protected:
    LegacyAsioTransport(SocketPtr&& socket, size_t maxTxLength,
                        size_t maxRxLength)
        : Base(std::move(socket), maxTxLength, maxRxLength)
    {}

    void sendMessage(RawsockMsgType type, Buffer&& message)
    {
        assert(this->isOpen() && "Attempting to send on bad transport");
        assert((message->length() <= this->maxSendLength()) &&
               "Outgoing message is longer than allowed by transport");

        message->header_ = endian::nativeToBig32(message->length());
        if (this->txQueue_.empty())
            transmit(std::move(message));
        else
            this->txQueue_.push(std::move(message));
    }

    void processHeader()
    {
        size_t length = endian::bigToNative32(this->rxBuffer_->header_);
        if ( this->check(length <= this->maxReceiveLength(),
                         TransportErrc::badRxLength) )
        {
            this->receivePayload(RawsockMsgType::wamp, length);
        }
    }
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_LEGACYASIOTRANSPORT_HPP
