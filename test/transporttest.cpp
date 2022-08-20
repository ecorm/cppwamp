/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <set>
#include <thread>
#include <cppwamp/codec.hpp>
#include <cppwamp/internal/asioconnector.hpp>
#include <cppwamp/internal/asiolistener.hpp>
#include <cppwamp/internal/tcpopener.hpp>
#include <cppwamp/internal/tcpacceptor.hpp>
#include <cppwamp/internal/rawsockconnector.hpp>
#include <cppwamp/internal/udsopener.hpp>
#include <cppwamp/internal/udsacceptor.hpp>
#include "faketransport.hpp"
#include "transporttest.hpp"

using namespace wamp;

using TcpAsioConnector = internal::AsioConnector<internal::TcpOpener>;
using TcpAsioListener  = internal::AsioListener<internal::TcpAcceptor>;
using UdsAsioConnector = internal::AsioConnector<internal::UdsOpener>;
using UdsAsioListener  = internal::AsioListener<internal::UdsAcceptor>;
using TcpTransport     = TcpAsioConnector::Transport;
using UdsTransport     = UdsAsioConnector::Transport;
using RML              = RawsockMaxLength;

using CodecIds = std::set<int>;

static constexpr auto jsonId = KnownCodecIds::json();
static constexpr auto msgpackId = KnownCodecIds::msgpack();

namespace
{

//------------------------------------------------------------------------------
struct TcpLoopbackFixture :
        protected LoopbackFixtureBase,
        public LoopbackFixture<TcpAsioConnector, TcpAsioListener>
{
    TcpLoopbackFixture(
                bool connected = true,
                int clientCodec = jsonId,
                CodecIds serverCodecs = {jsonId},
                RawsockMaxLength clientMaxRxLength = RML::kB_64,
                RawsockMaxLength serverMaxRxLength = RML::kB_64 )
        : LoopbackFixture(
              clientCtx,
              serverCtx,
              internal::TcpOpener(clientCtx.get_executor(),
                                  {tcpLoopbackAddr, tcpTestPort}),
              clientCodec,
              clientMaxRxLength,
              internal::TcpAcceptor(serverCtx.get_executor(), tcpTestPort),
              serverCodecs,
              serverMaxRxLength,
              connected )
    {}
};

//------------------------------------------------------------------------------
struct UdsLoopbackFixture :
        protected LoopbackFixtureBase,
        public LoopbackFixture<UdsAsioConnector, UdsAsioListener>
{
    UdsLoopbackFixture(
                bool connected = true,
                int clientCodec = jsonId,
                CodecIds serverCodecs = {jsonId},
                RawsockMaxLength clientMaxRxLength = RML::kB_64,
                RawsockMaxLength serverMaxRxLength = RML::kB_64 )
        : LoopbackFixture(
              clientCtx,
              serverCtx,
              internal::UdsOpener(clientCtx.get_executor(), {udsTestPath}),
              clientCodec,
              clientMaxRxLength,
              internal::UdsAcceptor(serverCtx.get_executor(), udsTestPath,
                                    true),
              serverCodecs,
              serverMaxRxLength,
              connected )
    {}
};

//------------------------------------------------------------------------------
template <typename TFixture>
void checkPing(TFixture& f)
{
    constexpr int sleepMs = 50;

    f.client->start(
        [&](ErrorOr<MessageBuffer>)
        {
            FAIL( "unexpected receive or error");
        });

    f.server->start(
        [&](ErrorOr<MessageBuffer>)
        {
            FAIL( "unexpected receive or error");
        });

    bool pingCompleted = false;
    auto payload = makeMessageBuffer("hello");
    f.client->ping(payload, [&](float elapsed)
    {
        CHECK( elapsed > sleepMs );
        pingCompleted = true;
        f.stop();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));

    CHECK_NOTHROW( f.run() );

    CHECK( pingCompleted );

    pingCompleted = false;
    payload = makeMessageBuffer("bonjour");
    f.server->ping(payload, [&](float elapsed)
    {
        CHECK( elapsed > sleepMs );
        pingCompleted = true;
        f.stop();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));

    CHECK_NOTHROW( f.run() );

    CHECK( pingCompleted );
}

//------------------------------------------------------------------------------
template <typename TFixture>
void checkUnsupportedSerializer(TFixture& f)
{
    f.lstn.establish([&](ErrorOr<Transporting::Ptr> transport)
    {
        CHECK( transport == makeUnexpectedError(RawsockErrc::badSerializer) );
    });

    f.cnct.establish([&](ErrorOr<Transporting::Ptr> transport)
    {
        CHECK( transport == makeUnexpectedError(RawsockErrc::badSerializer) );
    });

    CHECK_NOTHROW( f.run() );
}

//------------------------------------------------------------------------------
inline void checkCannedServerHandshake(uint32_t cannedHandshake,
                                       std::error_code expectedErrorCode)
{
    using AsioConnector = internal::AsioConnector<internal::TcpOpener>;
    using TransportingPtr  = typename Transporting::Ptr;

    AsioContext ioctx;
    internal::TcpAcceptor acpt(ioctx.get_executor(), tcpTestPort);
    FakeHandshakeAsioListener lstn(std::move(acpt), {jsonId}, RML::kB_64);
    lstn.setCannedHandshake(cannedHandshake);

    internal::TcpOpener opnr(ioctx.get_executor(),
                             {tcpLoopbackAddr, tcpTestPort});
    AsioConnector cnct(std::move(opnr), jsonId, RML::kB_64);

    lstn.establish( [](ErrorOr<TransportingPtr>) {} );

    bool aborted = false;
    cnct.establish(
        [&](ErrorOr<TransportingPtr> transport)
        {
            CHECK( transport == makeUnexpected(expectedErrorCode) );
            aborted = true;
        });

    CHECK_NOTHROW( ioctx.run() );
    CHECK( aborted );
}

//------------------------------------------------------------------------------
inline void checkCannedServerHandshake(uint32_t cannedHandshake,
                                       RawsockErrc expectedErrorCode)
{
    return checkCannedServerHandshake(cannedHandshake,
                                      make_error_code(expectedErrorCode));
}

//------------------------------------------------------------------------------
template <typename TErrorCode>
void checkCannedClientHandshake(uint32_t cannedHandshake,
                                RawsockErrc expectedServerCode,
                                TErrorCode expectedClientCode)
{
    using FakeConnector = internal::RawsockConnector<internal::TcpOpener,
                                                     CannedHandshakeConfig>;
    using AsioListener = internal::AsioListener<internal::TcpAcceptor>;

    AsioContext ioctx;
    IoStrand strand{ioctx.get_executor()};
    auto cnct = FakeConnector::create(strand, {tcpLoopbackAddr, tcpTestPort},
                                      jsonId);
    CannedHandshakeConfig::cannedNativeBytes() = cannedHandshake;

    internal::TcpAcceptor acpt(ioctx.get_executor(), tcpTestPort);
    AsioListener lstn(std::move(acpt), {jsonId}, RML::kB_64);

    bool serverAborted = false;
    lstn.establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            REQUIRE_FALSE( transport.has_value() );
            CHECK( transport.error() == expectedServerCode );
            serverAborted = true;
        });

    bool clientAborted = false;
    cnct->establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            REQUIRE_FALSE( transport.has_value() );
            CHECK( transport.error() == expectedClientCode );
            clientAborted = true;
        });

    CHECK_NOTHROW( ioctx.run() );
    CHECK( clientAborted );
    CHECK( serverAborted );
}

} // anonymous namespace

//------------------------------------------------------------------------------
SCENARIO( "Normal connection", "[Transport]" )
{
GIVEN( "an unconnected TCP connector/listener pair" )
{
    WHEN( "the client and server use JSON" )
    {
        TcpLoopbackFixture f(false, jsonId, {jsonId}, RML::kB_32, RML::kB_128);
        checkConnection(f, jsonId, 32*1024, 128*1024);
    }
    WHEN( "the client uses JSON and the server supports both" )
    {
        TcpLoopbackFixture f(false, jsonId, {jsonId, msgpackId},
                             RML::kB_32, RML::kB_128 );
        checkConnection(f, jsonId, 32*1024, 128*1024);
    }
    WHEN( "the client and server use Msgpack" )
    {
        TcpLoopbackFixture f(false, msgpackId, {msgpackId},
                             RML::kB_32, RML::kB_128 );
        checkConnection(f, msgpackId, 32*1024, 128*1024);
    }
    WHEN( "the client uses Msgpack and the server supports both" )
    {
        TcpLoopbackFixture f(false, msgpackId, {jsonId, msgpackId},
                             RML::kB_32, RML::kB_128 );
        checkConnection(f, msgpackId, 32*1024, 128*1024);
    }
}
GIVEN( "an unconnected UDS connector/listener pair" )
{
    WHEN( "the client and server use JSON" )
    {
        UdsLoopbackFixture f(false, jsonId, {jsonId}, RML::kB_32, RML::kB_128 );
        checkConnection(f, jsonId, 32*1024, 128*1024);
    }
    WHEN( "the client uses JSON and the server supports both" )
    {
        UdsLoopbackFixture f(false, jsonId, {jsonId, msgpackId},
                             RML::kB_32, RML::kB_128 );
        checkConnection(f, jsonId, 32*1024, 128*1024);
    }
    WHEN( "the client and server use Msgpack" )
    {
        UdsLoopbackFixture f(false, msgpackId, {msgpackId},
                             RML::kB_32, RML::kB_128 );
        checkConnection(f, msgpackId, 32*1024, 128*1024);
    }
    WHEN( "the client uses Msgpack and the server supports both" )
    {
        UdsLoopbackFixture f(false, msgpackId, {jsonId, msgpackId},
                             RML::kB_32, RML::kB_128 );
        checkConnection(f, msgpackId, 32*1024, 128*1024);
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Normal communications", "[Transport]" )
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
SCENARIO( "Consecutive send/receive", "[Transport]" )
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
SCENARIO( "Maximum length messages", "[Transport]" )
{
GIVEN( "a connected client/server TCP transport pair" )
{
    TcpLoopbackFixture f;
    const MessageBuffer message(f.client->info().maxRxLength, 'm');
    const MessageBuffer reply(f.server->info().maxRxLength, 'r');;
    checkSendReply(f, message, reply);
}
GIVEN( "a connected client/server UDS transport pair" )
{
    UdsLoopbackFixture f;
    const MessageBuffer message(f.client->info().maxRxLength, 'm');
    const MessageBuffer reply(f.server->info().maxRxLength, 'r');;
    checkSendReply(f, message, reply);
}
}

//------------------------------------------------------------------------------
SCENARIO( "Zero length messages", "[Transport]" )
{
const MessageBuffer message;
const MessageBuffer reply;

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
SCENARIO( "Ping/pong messages", "[Transport]" )
{
GIVEN( "a connected client/server TCP transport pair" )
{
    TcpLoopbackFixture f;
    checkPing(f);
}
GIVEN( "a connected client/server UDS transport pair" )
{
    UdsLoopbackFixture f;
    checkPing(f);
}
}

//------------------------------------------------------------------------------
SCENARIO( "Cancel listen", "[Transport]" )
{
    auto message = makeMessageBuffer("Hello");
    auto reply = makeMessageBuffer("World");

    GIVEN( "an unconnected TCP listener/connector pair" )
    {
        TcpLoopbackFixture f(false);
        checkCancelListen(f);
        checkConnection(f, jsonId);
        checkSendReply(f, message, reply);
    }
    GIVEN( "an unconnected UDS listener/connector pair" )
    {
        UdsLoopbackFixture f(false);
        checkCancelListen(f);
        checkConnection(f, jsonId);
        checkSendReply(f, message, reply);
    }
}

//------------------------------------------------------------------------------
SCENARIO( "Cancel connect", "[Transport]" )
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
SCENARIO( "Cancel receive", "[Transport]" )
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
SCENARIO( "Cancel send", "[Transport]" )
{
    // The size of transmission is set to maximum to increase the likelyhood
    // of the operation being aborted, rather than completed.

    GIVEN( "a connected TCP listener/connector pair" )
    {
        TcpLoopbackFixture f(false, jsonId, {jsonId},
                RML::MB_16, RML::MB_16);
        checkCancelSend(f);
    }
    GIVEN( "a connected UDS listener/connector pair" )
    {
        UdsLoopbackFixture f(false, jsonId, {jsonId},
                RML::MB_16, RML::MB_16);
        checkCancelSend(f);
    }
}

//------------------------------------------------------------------------------
SCENARIO( "Unsupported serializer", "[Transport]" )
{
GIVEN( "a TCP JSON client and a TCP Msgpack server" )
{
    TcpLoopbackFixture f(false, jsonId, {msgpackId});
    checkUnsupportedSerializer(f);
}
GIVEN( "a TCP Msgpack client and a TCP JSON server" )
{
    TcpLoopbackFixture f(false, msgpackId, {jsonId});
    checkUnsupportedSerializer(f);
}
GIVEN( "a UDS JSON client and a UDS Msgpack server" )
{
    UdsLoopbackFixture f(false, jsonId, {msgpackId});
    checkUnsupportedSerializer(f);
}
GIVEN( "a UDS Msgpack client and a UDS JSON server" )
{
    UdsLoopbackFixture f(false, msgpackId, {jsonId});
    checkUnsupportedSerializer(f);
}
}

//------------------------------------------------------------------------------
SCENARIO( "Connection denied by server", "[Transport]" )
{
GIVEN( "max length is unacceptable" )
{
    checkCannedServerHandshake(0x7f200000, RawsockErrc::badMaxLength);
}
GIVEN( "use of reserved bits" )
{
    checkCannedServerHandshake(0x7f300000, RawsockErrc::reservedBitsUsed);
}
GIVEN( "maximum connections reached" )
{
    checkCannedServerHandshake(0x7f400000, RawsockErrc::maxConnectionsReached);
}
GIVEN( "future error code" )
{
    checkCannedServerHandshake(0x7f500000,
                               std::error_code(5, rawsockCategory()));
}
}

//------------------------------------------------------------------------------
SCENARIO( "Invalid server handshake", "[Transport]" )
{
GIVEN( "a server that uses an invalid magic octet" )
{
    checkCannedServerHandshake(0xff710000, RawsockErrc::badHandshake);
}
GIVEN( "a server that uses a zeroed magic octet" )
{
    checkCannedServerHandshake(0x00710000, RawsockErrc::badHandshake);
}
GIVEN( "a server that uses an unspecified serializer" )
{
    checkCannedServerHandshake(0x7f720000, RawsockErrc::badHandshake);
}
GIVEN( "a server that uses an unknown serializer" )
{
    checkCannedServerHandshake(0x7f730000, RawsockErrc::badHandshake);
}
GIVEN( "a server that uses reserved bits" )
{
    checkCannedServerHandshake(0x7f710001, RawsockErrc::reservedBitsUsed);
}
}

//------------------------------------------------------------------------------
SCENARIO( "Invalid client handshake", "[Transport]" )
{
GIVEN( "a client that uses invalid magic octet" )
{
    checkCannedClientHandshake(0xff710000, RawsockErrc::badHandshake,
                               TransportErrc::failed);
}
GIVEN( "a client that uses a zeroed magic octet" )
{
    checkCannedClientHandshake(0x00710000, RawsockErrc::badHandshake,
                               TransportErrc::failed);
}
GIVEN( "a client that uses reserved bits" )
{
    checkCannedClientHandshake(0x7f710001, RawsockErrc::reservedBitsUsed,
                               RawsockErrc::reservedBitsUsed);
}
}

//------------------------------------------------------------------------------
SCENARIO( "Receiving messages longer than maximum", "[Transport]" )
{
using AsioConnector = internal::AsioConnector<internal::TcpOpener>;
using AsioListener  = internal::AsioListener<internal::TcpAcceptor>;
using FakeConnector = internal::RawsockConnector<internal::TcpOpener,
                                                 CannedHandshakeConfig>;

MessageBuffer tooLong(64*1024 + 1, 'A');

GIVEN ( "A server tricked into sending overly long messages to a client" )
{
    AsioContext ioctx;
    IoStrand strand{ioctx.get_executor()};
    auto tcpHost = TcpHost{tcpLoopbackAddr, tcpTestPort}
                       .withMaxRxLength(RML::kB_64);
    auto cnct = FakeConnector::create(strand, tcpHost, jsonId);
    CannedHandshakeConfig::cannedNativeBytes() = 0x7F810000;

    internal::TcpAcceptor acpt(ioctx.get_executor(), tcpTestPort);
    AsioListener lstn(std::move(acpt), {jsonId}, RML::kB_64);

    Transporting::Ptr server;
    Transporting::Ptr client;

    lstn.establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            REQUIRE( transport.has_value() );
            server = std::move(*transport);
        });

    cnct->establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            REQUIRE( transport.has_value() );
            client = std::move(*transport);
        });

    CHECK_NOTHROW( ioctx.run() );
    ioctx.restart();
    REQUIRE( server );
    REQUIRE( client );

    WHEN( "the server sends a message that exceeds the client's maximum" )
    {
        bool clientFailed = false;
        bool serverFailed = false;
        client->start(
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                CHECK( message.error() == TransportErrc::badRxLength );
                clientFailed = true;
            });

        server->start(
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                serverFailed = true;
            });

        server->send(tooLong);

        THEN( "the client obtains an error while receiving" )
        {
            CHECK_NOTHROW( ioctx.run() );
            CHECK( clientFailed );
            CHECK( serverFailed );
        }
    }
}
GIVEN ( "A client tricked into sending overly long messages to a server" )
{
    AsioContext ioctx;
    internal::TcpAcceptor acpt(ioctx.get_executor(), tcpTestPort);
    FakeHandshakeAsioListener lstn(std::move(acpt), {jsonId}, RML::kB_64);
    lstn.setCannedHandshake(0x7F810000);

    internal::TcpOpener opnr(ioctx.get_executor(),
                             {tcpLoopbackAddr, tcpTestPort});
    AsioConnector cnct(std::move(opnr), jsonId, RML::kB_64);

    Transporting::Ptr server;
    Transporting::Ptr client;

    lstn.establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            REQUIRE( transport.has_value() );
            server = std::move(*transport);
        });

    cnct.establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            REQUIRE( transport.has_value() );
            client = std::move(*transport);
        });

    CHECK_NOTHROW( ioctx.run() );
    ioctx.restart();
    REQUIRE( server );
    REQUIRE( client );

    WHEN( "the client sends a message that exceeds the server's maximum" )
    {
        bool clientFailed = false;
        bool serverFailed = false;
        client->start(
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                clientFailed = true;
            });

        server->start(
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                serverFailed = true;
            });

        client->send(std::move(tooLong));

        THEN( "the server obtains an error while receiving" )
        {
            CHECK_NOTHROW( ioctx.run() );
            CHECK( clientFailed );
            CHECK( serverFailed );
        }
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Receiving an invalid message type", "[Transport]" )
{
using AsioConnector    = internal::AsioConnector<internal::TcpOpener>;
using AsioListener     = internal::AsioListener<internal::TcpAcceptor>;
using FakeTransport    = FakeMsgTypeAsioListener::Transport;
using FakeTransportPtr = FakeMsgTypeAsioListener::Transport::Ptr;

GIVEN ( "A fake server that sends an invalid message type" )
{
    AsioContext ioctx;
    internal::TcpAcceptor acpt(ioctx.get_executor(), tcpTestPort);
    FakeMsgTypeAsioListener lstn(std::move(acpt), {jsonId}, RML::kB_64);

    internal::TcpOpener opnr(ioctx.get_executor(),
                             {tcpLoopbackAddr, tcpTestPort});
    AsioConnector cnct(std::move(opnr), jsonId, RML::kB_64);

    FakeTransportPtr server;
    Transporting::Ptr client;

    lstn.establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            REQUIRE( transport.has_value() );
            server = std::dynamic_pointer_cast<FakeTransport>(*transport);
        });

    cnct.establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            REQUIRE( transport.has_value() );
            client = std::move(*transport);
        });

    CHECK_NOTHROW( ioctx.run() );
    ioctx.restart();
    REQUIRE( server );
    REQUIRE( client );

    WHEN( "the server sends an invalid message to the client" )
    {
        bool clientFailed = false;
        bool serverFailed = false;
        client->start(
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                CHECK( message.error() == RawsockErrc::badMessageType );
                clientFailed = true;
            });

        server->start(
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                serverFailed = true;
            });

        auto msg = makeMessageBuffer("Hello");;
        server->send(msg);

        THEN( "the client obtains an error while receiving" )
        {
            CHECK_NOTHROW( ioctx.run() );
            CHECK( clientFailed );
            CHECK( serverFailed );
        }
    }
}
GIVEN ( "A fake client that sends an invalid message type" )
{
    using FakeConnector = internal::RawsockConnector<internal::TcpOpener,
                                                     FakeTransportClientConfig>;

    AsioContext ioctx;
    IoStrand strand{ioctx.get_executor()};
    auto tcpHost = TcpHost{tcpLoopbackAddr, tcpTestPort}
                       .withMaxRxLength(RML::kB_64);
    auto cnct = FakeConnector::create(strand, tcpHost, jsonId);

    internal::TcpAcceptor acpt(ioctx.get_executor(), tcpTestPort);
    AsioListener lstn(std::move(acpt), {jsonId}, RML::kB_64);

    Transporting::Ptr server;
    FakeTransportPtr client;

    lstn.establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            REQUIRE( transport.has_value() );
            server = std::move(*transport);
        });

    cnct->establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            REQUIRE( transport.has_value() );
            client = std::dynamic_pointer_cast<FakeTransport>(*transport);
        });

    CHECK_NOTHROW( ioctx.run() );
    ioctx.restart();
    REQUIRE( server );
    REQUIRE( client );

    WHEN( "the client sends an invalid message to the server" )
    {
        bool clientFailed = false;
        bool serverFailed = false;
        client->start(
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                clientFailed = true;
            });

        server->start(
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                CHECK( message.error() == RawsockErrc::badMessageType );
                serverFailed = true;
            });

        auto msg = makeMessageBuffer("Hello");
        client->send(std::move(msg));

        THEN( "the server obtains an error while receiving" )
        {
            CHECK_NOTHROW( ioctx.run() );
            CHECK( clientFailed );
            CHECK( serverFailed );
        }
    }
}
}
