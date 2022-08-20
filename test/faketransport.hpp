/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TEST_FAKETRANSPORT_HPP
#define CPPWAMP_TEST_FAKETRANSPORT_HPP

#include <boost/asio/post.hpp>
#include <cppwamp/codec.hpp>
#include <cppwamp/internal/rawsockheader.hpp>
#include <cppwamp/internal/rawsockconnector.hpp>
#include <cppwamp/internal/rawsocklistener.hpp>
#include <cppwamp/internal/rawsocktransport.hpp>
#include <cppwamp/internal/tcpacceptor.hpp>
#include <cppwamp/internal/tcpopener.hpp>

namespace wamp
{

//------------------------------------------------------------------------------
struct CannedHandshakeConfig : internal::DefaultRawsockClientConfig
{
    static uint32_t hostOrderHandshakeBytes(int, RawsockMaxLength)
    {
        return cannedHostBytes();
    }

    static uint32_t& cannedHostBytes()
    {
        static uint32_t bytes = 0;
        return bytes;
    }
};

//------------------------------------------------------------------------------
class FakeMsgTypeTransport :
        public internal::RawsockTransport<boost::asio::ip::tcp::socket>
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
    using Base = internal::RawsockTransport<boost::asio::ip::tcp::socket>;

protected:
    using Base::Base;
};

//------------------------------------------------------------------------------
struct FakeTransportClientConfig : internal::DefaultRawsockClientConfig
{
    template <typename>
    using TransportType = FakeMsgTypeTransport;

};

//------------------------------------------------------------------------------
struct FakeTransportServerConfig : internal::DefaultRawsockServerConfig
{
    template <typename>
    using TransportType = FakeMsgTypeTransport;

};

} // namespace wamp

#endif // CPPWAMP_TEST_FAKETRANSPORT_HPP
