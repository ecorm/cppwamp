/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#if CPPWAMP_TESTING_TRANSPORT

#include <cppwamp/internal/legacyasioendpoint.hpp>
#include <cppwamp/internal/legacyasiotransport.hpp>
#include <cppwamp/internal/tcpacceptor.hpp>
#include <cppwamp/internal/tcpopener.hpp>
#include <cppwamp/internal/tcpacceptor.hpp>
#include <cppwamp/internal/udsopener.hpp>
#include <cppwamp/internal/udsacceptor.hpp>
#include "transporttest.hpp"

using namespace wamp;

using TcpAsioConnector = internal::LegacyAsioEndpoint<internal::TcpOpener>;
using TcpAsioListener  = internal::LegacyAsioEndpoint<internal::TcpAcceptor>;
using UdsAsioConnector = internal::LegacyAsioEndpoint<internal::UdsOpener>;
using UdsAsioListener  = internal::LegacyAsioEndpoint<internal::UdsAcceptor>;
using TcpTransport     = TcpAsioConnector::Transport;
using UdsTransport     = UdsAsioConnector::Transport;

namespace
{

//------------------------------------------------------------------------------
struct TcpLoopbackFixture :
        public LoopbackFixture<TcpAsioConnector, TcpAsioListener>
{
    TcpLoopbackFixture(
                bool connected = true,
                CodecId codec = Json::id(),
                RawsockMaxLength clientMaxRxLength = RawsockMaxLength::kB_64,
                RawsockMaxLength serverMaxRxLength = RawsockMaxLength::kB_64 )
        : LoopbackFixture(
              internal::TcpOpener(csvc, tcpLoopbackAddr, tcpTestPort),
              codec,
              clientMaxRxLength,
              internal::TcpAcceptor(ssvc, tcpTestPort),
              codec,
              serverMaxRxLength,
              connected )
    {}
};

//------------------------------------------------------------------------------
struct UdsLoopbackFixture :
        public LoopbackFixture<UdsAsioConnector, UdsAsioListener>
{
    UdsLoopbackFixture(
                bool connected = true,
                CodecId codec = Json::id(),
                RawsockMaxLength clientMaxRxLength = RawsockMaxLength::kB_64,
                RawsockMaxLength serverMaxRxLength = RawsockMaxLength::kB_64 )
        : LoopbackFixture(
              internal::UdsOpener(csvc, udsTestPath),
              codec,
              clientMaxRxLength,
              internal::UdsAcceptor(ssvc, udsTestPath, true),
              codec,
              serverMaxRxLength,
              connected )
    {}
};

//------------------------------------------------------------------------------
template <typename TFixture>
void checkReceiveTooLong(TFixture& f,
                         typename TFixture::TransportPtr sender,
                         typename TFixture::TransportPtr receiver)
{
    using Transport = typename TFixture::Transport;
    using Buffer    = typename Transport::Buffer;

    bool receiveFailed = false;
    receiver->start(
        [&](Buffer)
        {
            FAIL( "unexpected receive" );
        },
        [&](std::error_code ec)
        {
            CHECK( ec == TransportErrc::badRxLength );
            receiveFailed = true;
        });

    sender->start(
        [&](Buffer)
        {
            FAIL( "unexpected receive" );
        },
        [&](std::error_code) {} );

    auto sendBuf = sender->getBuffer();
    std::string tooLong(receiver->maxReceiveLength() + 1, 'a');
    sendBuf->write(tooLong.data(), tooLong.size());
    sender->send(std::move(sendBuf));

    REQUIRE_NOTHROW( f.run() );
    CHECK( receiveFailed );
}

} // anonymous namespace


//------------------------------------------------------------------------------
SCENARIO( "Normal legacy connection", "[Transport]" )
{
GIVEN( "an unconnected TCP connector/listener pair" )
{
    WHEN( "the client and server use JSON" )
    {
        TcpLoopbackFixture f(false, Json::id());
        checkConnection(f, Json::id());
    }
    WHEN( "the client and server use Msgpack" )
    {
        TcpLoopbackFixture f(false, Msgpack::id());
        checkConnection(f, Msgpack::id());
    }
}
GIVEN( "an unconnected UDS connector/listener pair" )
{
    WHEN( "the client and server use JSON" )
    {
        UdsLoopbackFixture f(false, Json::id());
        checkConnection(f, Json::id());
    }
    WHEN( "the client and server use Msgpack" )
    {
        UdsLoopbackFixture f(false, Msgpack::id());
        checkConnection(f, Msgpack::id());
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Normal legacy communications", "[Transport,Legacy]" )
{
GIVEN( "a connected client/server TCP transport pair" )
{
    TcpLoopbackFixture f;
    checkCommunications(f);
}
GIVEN( "a connected client/server UDS transport pair" )
{
    UdsLoopbackFixture f;
    checkCommunications(f);
}
}

//------------------------------------------------------------------------------
SCENARIO( "Consecutive legacy send/receive", "[Transport,Legacy]" )
{
GIVEN( "a connected client/server TCP transport pair" )
{
    {
        TcpLoopbackFixture f;
        checkConsecutiveSendReceive(f, f.client, f.server);
    }
    {
        TcpLoopbackFixture f;
        checkConsecutiveSendReceive(f, f.server, f.client);
    }
}
GIVEN( "a connected client/server UDS transport pair" )
{
    {
        UdsLoopbackFixture f;
        checkConsecutiveSendReceive(f, f.client, f.server);
    }
    {
        UdsLoopbackFixture f;
        checkConsecutiveSendReceive(f, f.server, f.client);
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Maximum length legacy messages", "[Transport]" )
{
GIVEN( "a connected client/server TCP transport pair" )
{
    TcpLoopbackFixture f;
    const std::string message(f.client->maxReceiveLength(), 'm');
    const std::string reply(f.server->maxReceiveLength(), 'r');;
    checkSendReply(f, message, reply);
}
GIVEN( "a connected client/server UDS transport pair" )
{
    UdsLoopbackFixture f;
    const std::string message(f.client->maxReceiveLength(), 'm');
    const std::string reply(f.server->maxReceiveLength(), 'r');;
    checkSendReply(f, message, reply);
}
}

//------------------------------------------------------------------------------
SCENARIO( "Zero length legacy messages", "[Transport,Legacy]" )
{
const std::string message("");
const std::string reply("");

GIVEN( "a connected client/server TCP transport pair" )
{
    TcpLoopbackFixture f;
    checkSendReply(f, message, reply);
}
GIVEN( "a connected client/server UDS transport pair" )
{
    UdsLoopbackFixture f;
    checkSendReply(f, message, reply);
}
}

//------------------------------------------------------------------------------
SCENARIO( "Cancel legacy listen", "[Transport,Legacy]" )
{
    GIVEN( "an unconnected TCP listener/connector pair" )
    {
        TcpLoopbackFixture f(false);
        checkCancelListen(f);
        checkConnection(f, Json::id());
        checkSendReply(f, "Hello", "World");
    }
    GIVEN( "an unconnected UDS listener/connector pair" )
    {
        UdsLoopbackFixture f(false);
        checkCancelListen(f);
        checkConnection(f, Json::id());
        checkSendReply(f, "Hello", "World");
    }
}

//------------------------------------------------------------------------------
SCENARIO( "Cancel legacy connect", "[Transport,Legacy]" )
{
    GIVEN( "an unconnected TCP listener/connector pair" )
    {
        TcpLoopbackFixture f(false);
        checkCancelConnect(f);
    }
    GIVEN( "an unconnected UDS listener/connector pair" )
    {
        UdsLoopbackFixture f(false);
        checkCancelConnect(f);
    }
}

//------------------------------------------------------------------------------
SCENARIO( "Cancel legacy receive", "[Transport,Legacy]" )
{
    GIVEN( "a connected TCP listener/connector pair" )
    {
        TcpLoopbackFixture f;
        checkCancelReceive(f);
    }
    GIVEN( "a connected UDS listener/connector pair" )
    {
        UdsLoopbackFixture f;
        checkCancelReceive(f);
    }
}

//------------------------------------------------------------------------------
SCENARIO( "Cancel legacy send", "[Transport]" )
{
    // The size of transmission is set to maximum to increase the likelyhood
    // of the operation being aborted, rather than completed.

    GIVEN( "a connected TCP listener/connector pair" )
    {
        TcpLoopbackFixture f(false, Json::id(), RawsockMaxLength::MB_16,
                             RawsockMaxLength::MB_16);
        checkCancelSend(f);
    }
    GIVEN( "a connected UDS listener/connector pair" )
    {
        UdsLoopbackFixture f(false, Json::id(), RawsockMaxLength::MB_16,
                             RawsockMaxLength::MB_16);
        checkCancelSend(f);
    }
}

//------------------------------------------------------------------------------
SCENARIO( "Receiving legacy messages longer than maximum", "[Transport]" )
{
GIVEN ( "A TCP client that sends an overly long message" )
{
    TcpLoopbackFixture f(true, Json::id(), RawsockMaxLength::kB_64,
                         RawsockMaxLength::kB_32);
    checkReceiveTooLong(f, f.client, f.server);
}
GIVEN ( "A TCP server that sends an overly long message" )
{
    TcpLoopbackFixture f(true, Json::id(), RawsockMaxLength::kB_32,
                         RawsockMaxLength::kB_64);
    checkReceiveTooLong(f, f.server, f.client);
}
GIVEN ( "A UDS client that sends an overly long message" )
{
    UdsLoopbackFixture f(true, Json::id(), RawsockMaxLength::kB_64,
                         RawsockMaxLength::kB_32);
    checkReceiveTooLong(f, f.client, f.server);
}
GIVEN ( "A UDS server that sends an overly long message" )
{
    UdsLoopbackFixture f(true, Json::id(), RawsockMaxLength::kB_32,
                         RawsockMaxLength::kB_64);
    checkReceiveTooLong(f, f.server, f.client);
}
}

#endif // #if CPPWAMP_TESTING_TRANSPORT
