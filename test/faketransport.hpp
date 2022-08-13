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
#include <cppwamp/internal/tcpacceptor.hpp>
#include <cppwamp/internal/tcpopener.hpp>

namespace wamp
{

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
class FakeHandshakeAsioConnector :
        public internal::AsioConnector<wamp::internal::TcpOpener>
{
private:
    using Base = internal::AsioConnector<wamp::internal::TcpOpener>;

public:
    using Base::Base;

    virtual void onEstablished() override
        {Base::sendHandshake(cannedHandshake_);}

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

    static Ptr create(SocketPtr&& socket, size_t maxTxLength,
                      size_t maxRxLength)
    {
        return Ptr(new FakeMsgTypeTransport(std::move(socket),
                   maxTxLength, maxRxLength));
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
            auto trnsp = FakeMsgTypeTransport::create(std::move(socket_),
                                                      64*1024, 64*1024);
            std::error_code ec = make_error_code(TransportErrc::success);
            boost::asio::post(strand_,
                              std::bind(handler_, ec, KnownCodecIds::json(),
                                        std::move(trnsp)));
            socket_.reset();
            handler_ = nullptr;
        }
        else
            Base::fail(hs.errorCode());
    }
};

//------------------------------------------------------------------------------
class FakeMsgTypeAsioConnector :
        public internal::AsioConnector<wamp::internal::TcpOpener>
{
private:
    using Base = internal::AsioConnector<wamp::internal::TcpOpener>;

public:
    using Transport = FakeMsgTypeTransport;
    using TransportPtr = std::shared_ptr<Transport>;

    using Base::Base;

    virtual void onHandshakeReceived(Handshake hs) override
    {
        if (!hs.hasMagicOctet())
            Base::fail(RawsockErrc::badHandshake);
        else if (hs.reserved() != 0)
            Base::fail(RawsockErrc::reservedBitsUsed);
        else if (hs.codecId() == KnownCodecIds::json())
        {
            auto trnsp = FakeMsgTypeTransport::create(std::move(socket_),
                                                      64*1024, 64*1024);
            std::error_code ec = make_error_code(TransportErrc::success);
            boost::asio::post(strand_,
                              std::bind(handler_, ec, KnownCodecIds::json(),
                                        std::move(trnsp)));
            socket_.reset();
            handler_ = nullptr;
        }
        else if (hs.hasError())
            Base::fail(hs.errorCode());
        else
            Base::fail(RawsockErrc::badHandshake);
    }
};

} // namespace wamp

#endif // CPPWAMP_TEST_FAKETRANSPORT_HPP
