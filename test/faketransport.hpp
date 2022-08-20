/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TEST_FAKETRANSPORT_HPP
#define CPPWAMP_TEST_FAKETRANSPORT_HPP

#include <boost/asio/post.hpp>
#include <cppwamp/codec.hpp>
#include <cppwamp/internal/asioconnector.hpp>
#include <cppwamp/internal/asiolistener.hpp>
#include <cppwamp/internal/asiotransport.hpp>
#include <cppwamp/internal/rawsockheader.hpp>
#include <cppwamp/internal/rawsockconnector.hpp>
#include <cppwamp/internal/tcpacceptor.hpp>
#include <cppwamp/internal/tcpopener.hpp>

namespace wamp
{

//------------------------------------------------------------------------------
struct CannedHandshakeConfig : internal::DefaultRawsockClientConfig
{
    static uint32_t hostOrderBytes(int, RawsockMaxLength)
    {
        return internal::endian::nativeToBig32(cannedNativeBytes());
    }

    static uint32_t& cannedNativeBytes()
    {
        static uint32_t bytes = 0;
        return bytes;
    }
};

//------------------------------------------------------------------------------
class FakeHandshakeAsioListener :
        public internal::AsioListener<wamp::internal::TcpAcceptor>
{
private:
    using Base = internal::AsioListener<wamp::internal::TcpAcceptor>;

public:
    using Base::Base;

    virtual void onHandshakeReceived(Handshake) override
    {
        Base::sendHandshake(cannedHandshake_);
    }

    virtual void onHandshakeSent(Handshake) override
    {
        Base::onHandshakeSent(
            Handshake().setCodecId(KnownCodecIds::json())
                       .setMaxLength(RawsockMaxLength::kB_64) );
    }

    void setCannedHandshake(uint32_t hostOrder)
        {cannedHandshake_ = Handshake(hostOrder);}

private:
    Handshake cannedHandshake_;
};

//------------------------------------------------------------------------------
class FakeMsgTypeTransport :
        public internal::AsioTransport<boost::asio::ip::tcp::socket>
{
public:
    using Ptr = std::shared_ptr<FakeMsgTypeTransport>;

    static Ptr create(SocketPtr&& s, TransportInfo info)
    {
        return Ptr(new FakeMsgTypeTransport(std::move(s), info));
    }

    void send(MessageBuffer message)
    {
        auto fakeType = internal::RawsockMsgType(
                            (int)internal::RawsockMsgType::pong + 1);
        auto frame = newFrame(fakeType, std::move(message));
        sendFrame(std::move(frame));
    }

private:
    using Base = internal::AsioTransport<boost::asio::ip::tcp::socket>;

protected:
    using Base::Base;
};

//------------------------------------------------------------------------------
class FakeMsgTypeAsioListener :
        public internal::AsioListener<wamp::internal::TcpAcceptor>
{
private:
    using Base = internal::AsioListener<wamp::internal::TcpAcceptor>;

public:
    using Transport = FakeMsgTypeTransport;
    using TransportPtr = std::shared_ptr<Transport>;

    using Base::Base;

    virtual void onHandshakeSent(Handshake hs) override
    {
        if (!hs.hasError())
        {
            Transporting::Ptr trnsp{
                FakeMsgTypeTransport::create(
                    std::move(socket_),
                    {KnownCodecIds::json(), 64*1024, 64*1024})};
            boost::asio::post(strand_, std::bind(handler_, std::move(trnsp)));
            socket_.reset();
            handler_ = nullptr;
        }
        else
            Base::fail(hs.errorCode());
    }
};

//------------------------------------------------------------------------------
struct FakeTransportClientConfig : internal::DefaultRawsockClientConfig
{
    template <typename>
    using TransportType = FakeMsgTypeTransport;

};

} // namespace wamp

#endif // CPPWAMP_TEST_FAKETRANSPORT_HPP
