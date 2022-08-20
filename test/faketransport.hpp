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
struct BadMsgTypeTransportConfig : internal::DefaultRawsockTransportConfig
{
    static internal::RawsockFrame::Ptr enframe(internal::RawsockMsgType type,
                                               MessageBuffer&& payload)
    {
        auto badType = internal::RawsockMsgType(
            (int)internal::RawsockMsgType::pong + 1);
        return std::make_shared<internal::RawsockFrame>(badType,
                                                        std::move(payload));
    }
};

//------------------------------------------------------------------------------
using BadMsgTypeTransport =
    internal::RawsockTransport<boost::asio::ip::tcp::socket,
                               BadMsgTypeTransportConfig>;

//------------------------------------------------------------------------------
struct FakeTransportClientConfig : internal::DefaultRawsockClientConfig
{
    template <typename>
    using TransportType = BadMsgTypeTransport;

};

//------------------------------------------------------------------------------
struct FakeTransportServerConfig : internal::DefaultRawsockServerConfig
{
    template <typename>
    using TransportType = BadMsgTypeTransport;

};

} // namespace wamp

#endif // CPPWAMP_TEST_FAKETRANSPORT_HPP
