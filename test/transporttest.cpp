/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <set>
#include <thread>
#include <vector>
#include <catch2/catch.hpp>
#include <cppwamp/asiodefs.hpp>
#include <cppwamp/codec.hpp>
#include <cppwamp/errorcodes.hpp>
#include <cppwamp/rawsockoptions.hpp>
#include <cppwamp/transport.hpp>
#include <cppwamp/internal/tcpopener.hpp>
#include <cppwamp/internal/tcpacceptor.hpp>
#include <cppwamp/internal/rawsockconnector.hpp>
#include <cppwamp/internal/rawsocklistener.hpp>
#include <cppwamp/internal/udsopener.hpp>
#include <cppwamp/internal/udsacceptor.hpp>

using namespace wamp;

namespace
{

//------------------------------------------------------------------------------
using TcpRawsockConnector = internal::RawsockConnector<internal::TcpOpener>;
using TcpRawsockListener  = internal::RawsockListener<internal::TcpAcceptor>;
using UdsRawsockConnector = internal::RawsockConnector<internal::UdsOpener>;
using UdsRawsockListener  = internal::RawsockListener<internal::UdsAcceptor>;
using RML                 = RawsockMaxLength;
using CodecIds            = std::set<int>;

//------------------------------------------------------------------------------
constexpr auto jsonId = KnownCodecIds::json();
constexpr auto msgpackId = KnownCodecIds::msgpack();
constexpr unsigned short tcpTestPort = 9090;
constexpr const char tcpLoopbackAddr[] = "127.0.0.1";
constexpr const char udsTestPath[] = "cppwamptestuds";
const auto tcpHost = TcpHost{tcpLoopbackAddr, tcpTestPort}
                         .withMaxRxLength(RML::kB_64);
const auto tcpEndpoint = TcpEndpoint{tcpTestPort}.withMaxRxLength(RML::kB_64);

//------------------------------------------------------------------------------
template <typename TConnector, typename TListener>
struct LoopbackFixture
{
    using Connector      = TConnector;
    using ClientSettings = typename TConnector::Settings;
    using Listener       = TListener;
    using ServerSettings = typename TListener::Settings;
    using Transport      = typename Connector::Transport;

    template <typename TServerCodecIds>
    LoopbackFixture(ClientSettings clientSettings,
                    int clientCodec,
                    ServerSettings serverSettings,
                    TServerCodecIds&& serverCodecs,
                    bool connected = true)
    {
        cnct = Connector::create(IoStrand{cctx.get_executor()},
                                 std::move(clientSettings), clientCodec);
        lstn = Listener::create(IoStrand{sctx.get_executor()},
                                std::move(serverSettings),
                                std::forward<TServerCodecIds>(serverCodecs));
        if (connected)
            connect();
    }

    void connect()
    {
        lstn->establish(
            [&](ErrorOr<Transporting::Ptr> transportOrError)
            {
                auto transport = transportOrError.value();
                serverCodec = transport->info().codecId;
                server = std::move(transport);
            });

        cnct->establish(
            [&](ErrorOr<Transporting::Ptr> transportOrError)
            {
                auto transport = transportOrError.value();
                clientCodec = transport->info().codecId;
                client = std::move(transport);
            });

        run();
    }

    void disconnect()
    {
        server->stop();
        client->stop();
    }

    void run()
    {
        while (!sctx.stopped() || !cctx.stopped())
        {
            if (!sctx.stopped())
                sctx.poll();
            if (!cctx.stopped())
                cctx.poll();
        }
        sctx.reset();
        cctx.reset();
    }

    void stop()
    {
        sctx.stop();
        cctx.stop();
    }

    IoContext cctx;
    IoContext sctx;
    typename Connector::Ptr cnct;
    typename Listener::Ptr lstn;
    int clientCodec;
    int serverCodec;
    Transporting::Ptr client;
    Transporting::Ptr server;
};


//------------------------------------------------------------------------------
struct TcpLoopbackFixture :
        public LoopbackFixture<TcpRawsockConnector, TcpRawsockListener>
{
    TcpLoopbackFixture(
                bool connected = true,
                int clientCodec = jsonId,
                CodecIds serverCodecs = {jsonId},
                RawsockMaxLength clientMaxRxLength = RML::kB_64,
                RawsockMaxLength serverMaxRxLength = RML::kB_64 )
        : LoopbackFixture(
              TcpHost{tcpLoopbackAddr, tcpTestPort}
                .withMaxRxLength(clientMaxRxLength),
              clientCodec,
              TcpEndpoint{tcpTestPort}.withMaxRxLength(serverMaxRxLength),
              serverCodecs,
              connected )
    {}
};

//------------------------------------------------------------------------------
struct UdsLoopbackFixture :
        /* protected LoopbackFixtureBase, */
        public LoopbackFixture<UdsRawsockConnector, UdsRawsockListener>
{
    UdsLoopbackFixture(
                bool connected = true,
                int clientCodec = jsonId,
                CodecIds serverCodecs = {jsonId},
                RawsockMaxLength clientMaxRxLength = RML::kB_64,
                RawsockMaxLength serverMaxRxLength = RML::kB_64 )
        : LoopbackFixture(
              UdsPath{udsTestPath}.withMaxRxLength(clientMaxRxLength),
              clientCodec,
              UdsPath{udsTestPath}.withMaxRxLength(serverMaxRxLength),
              serverCodecs,
              connected )
    {}
};

//------------------------------------------------------------------------------
struct CannedHandshakeConfig : internal::DefaultRawsockClientConfig
{
    static constexpr bool mockUnresponsiveness() {return false;}

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
                               internal::TcpTraits,
                               BadMsgTypeTransportConfig>;

//------------------------------------------------------------------------------
struct FakeTransportClientConfig : internal::DefaultRawsockClientConfig
{
    template <typename, typename>
    using TransportType = BadMsgTypeTransport;

};

//------------------------------------------------------------------------------
struct FakeTransportServerOptions : internal::DefaultRawsockServerOptions
{
    template <typename, typename>
    using TransportType = BadMsgTypeTransport;

};

//------------------------------------------------------------------------------
MessageBuffer makeMessageBuffer(const std::string& str)
{
    using MessageBufferByte = typename MessageBuffer::value_type;
    auto data = reinterpret_cast<const MessageBufferByte*>(str.data());
    return MessageBuffer(data, data + str.size());
}

//------------------------------------------------------------------------------
template <typename TFixture>
void checkConnection(TFixture& f, int expectedCodec,
                     size_t clientMaxRxLength = 64*1024,
                     size_t serverMaxRxLength = 64*1024)
{
    f.lstn->establish([&](ErrorOr<Transporting::Ptr> transportOrError)
    {
        REQUIRE( transportOrError.has_value() );
        auto transport = *transportOrError;
        REQUIRE( transport );
        CHECK( transport->info().codecId == expectedCodec );
        CHECK( transport->info().maxRxLength == serverMaxRxLength );
        CHECK( transport->info().maxTxLength == clientMaxRxLength );
        f.server = transport;
    });

    f.cnct->establish([&](ErrorOr<Transporting::Ptr> transportOrError)
    {
        REQUIRE( transportOrError.has_value() );
        auto transport = *transportOrError;
        REQUIRE( transport );
        CHECK( transport->info().codecId == expectedCodec );
        CHECK( transport->info().maxRxLength == clientMaxRxLength );
        CHECK( transport->info().maxTxLength == serverMaxRxLength );
        f.client = transport;
    });

    CHECK_NOTHROW( f.run() );
}

//------------------------------------------------------------------------------
template <typename TFixture>
void checkSendReply(TFixture& f,
                    Transporting::Ptr sender,
                    Transporting::Ptr receiver,
                    const MessageBuffer& message,
                    const MessageBuffer& reply)
{
    bool receivedMessage = false;
    bool receivedReply = false;

    receiver->start(
        nullptr,
        [&](ErrorOr<MessageBuffer> buf)
        {
            if (buf.has_value())
            {
                receivedMessage = true;
                CHECK( message == *buf );
                receiver->send(reply);
            }
            else
            {
                CHECK( buf.error() == TransportErrc::aborted );
            }
        },
        nullptr);

    sender->start(
        nullptr,
        [&](ErrorOr<MessageBuffer> buf)
        {
            if (buf.has_value())
            {
                receivedReply = true;
                CHECK( reply == *buf );
                f.disconnect();
            }
            else
            {
                CHECK( buf.error() == TransportErrc::aborted );
            }
        },
        nullptr);

    sender->send(message);

    REQUIRE_NOTHROW( f.run() );

    CHECK( receivedMessage );
    CHECK( receivedReply );
}

//------------------------------------------------------------------------------
template <typename TFixture>
void checkSendReply(TFixture& f, const MessageBuffer& message,
                    const MessageBuffer& reply)
{
    checkSendReply(f, f.client, f.server, message, reply);
}

//------------------------------------------------------------------------------
template <typename TFixture>
void checkConsecutiveSendReceive(TFixture& f, Transporting::Ptr& sender,
                                 Transporting::Ptr& receiver)
{
    std::vector<MessageBuffer> messages;
    for (int i=0; i<100; ++i)
        messages.emplace_back(i, 'A' + i);

    sender->start(
        nullptr,
        [&](ErrorOr<MessageBuffer> buf)
        {
            REQUIRE( !buf );
            CHECK( buf.error() == TransportErrc::aborted );
        },
        nullptr);

    size_t count = 0;

    receiver->start(
        nullptr,
        [&](ErrorOr<MessageBuffer> buf)
        {
            if (buf.has_value())
            {
                REQUIRE( messages.at(count) == *buf );
                if (++count == messages.size())
                {
                    f.disconnect();
                }
            }
            else
            {
                CHECK( buf.error() == TransportErrc::aborted );
            }
        },
        nullptr);

    for (const auto& msg: messages)
        sender->send(msg);

    CHECK_NOTHROW( f.run() );
}

//------------------------------------------------------------------------------
template <typename TFixture>
void checkUnsupportedSerializer(TFixture& f)
{
    f.lstn->establish([&](ErrorOr<Transporting::Ptr> transport)
    {
        CHECK( transport == makeUnexpectedError(TransportErrc::badSerializer) );
    });

    f.cnct->establish([&](ErrorOr<Transporting::Ptr> transport)
    {
        CHECK( transport == makeUnexpectedError(TransportErrc::badSerializer) );
    });

    CHECK_NOTHROW( f.run() );
}

//------------------------------------------------------------------------------
void checkCannedServerHandshake(uint32_t cannedHandshake,
                                std::error_code expectedErrorCode)
{
    IoContext ioctx;
    IoStrand strand{ioctx.get_executor()};

    using MockListener = internal::RawsockListener<internal::TcpAcceptor,
                                                   CannedHandshakeConfig>;
    auto lstn = MockListener::create(strand, tcpEndpoint, {jsonId});
    CannedHandshakeConfig::cannedHostBytes() = cannedHandshake;
    lstn->establish( [](ErrorOr<Transporting::Ptr>) {} );

    bool aborted = false;
    auto cnct = TcpRawsockConnector::create(strand, tcpHost, jsonId);
    cnct->establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            CHECK( transport == makeUnexpected(expectedErrorCode) );
            aborted = true;
        });

    CHECK_NOTHROW( ioctx.run() );
    CHECK( aborted );
}

//------------------------------------------------------------------------------
void checkCannedServerHandshake(uint32_t cannedHandshake,
                                TransportErrc expectedErrorCode)
{
    return checkCannedServerHandshake(cannedHandshake,
                                      make_error_code(expectedErrorCode));
}

//------------------------------------------------------------------------------
template <typename TErrorCode>
void checkCannedClientHandshake(uint32_t cannedHandshake,
                                TransportErrc expectedServerCode,
                                TErrorCode expectedClientCode)
{
    IoContext ioctx;
    IoStrand strand{ioctx.get_executor()};

    bool serverAborted = false;
    auto lstn = TcpRawsockListener::create(strand, tcpEndpoint, {jsonId});
    lstn->establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            REQUIRE_FALSE( transport.has_value() );
            CHECK( transport.error() == expectedServerCode );
            serverAborted = true;
        });

    using MockConnector = internal::RawsockConnector<internal::TcpOpener,
                                                     CannedHandshakeConfig>;
    auto cnct = MockConnector::create(strand, tcpHost, jsonId);
    CannedHandshakeConfig::cannedHostBytes() = cannedHandshake;
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
TEMPLATE_TEST_CASE( "Normal communications", "[Transport]",
                    TcpLoopbackFixture, UdsLoopbackFixture )
{
    TestType f;
    Transporting::Ptr sender = f.client;
    Transporting::Ptr receiver = f.server;
    auto message = makeMessageBuffer("Hello");
    auto reply = makeMessageBuffer("World");
    bool receivedMessage = false;
    bool receivedReply = false;

    receiver->start(
        nullptr,
        [&](ErrorOr<MessageBuffer> buf)
        {
            if (buf.has_value())
            {
                receivedMessage = true;
                CHECK( message == *buf );
                receiver->send(reply);
            }
            else
            {
                CHECK( buf.error() == TransportErrc::aborted );
            }
        },
        nullptr);

    sender->start(
        nullptr,
        [&](ErrorOr<MessageBuffer> buf)
        {
            if (buf.has_value())
            {
                receivedReply = true;
                CHECK( reply == *buf );
            }
            else
            {
                CHECK( buf.error() == TransportErrc::aborted );
            }
        },
        nullptr);

    sender->send(message);

    while (!receivedReply)
    {
        f.sctx.poll();
        f.cctx.poll();
    }
    f.sctx.reset();
    f.cctx.reset();

    CHECK( receivedMessage );

    // Another client connects to the same endpoint
    Transporting::Ptr server2;
    Transporting::Ptr client2;
    auto message2 = makeMessageBuffer("Hola");
    auto reply2 = makeMessageBuffer("Mundo");
    bool receivedMessage2 = false;
    bool receivedReply2 = false;
    message = makeMessageBuffer("Bonjour");
    reply = makeMessageBuffer("Le Monde");
    receivedMessage = false;
    receivedReply = false;

    f.lstn->establish(
        [&](ErrorOr<Transporting::Ptr> transportOrError)
        {
            REQUIRE( transportOrError.has_value() );
            auto transport = *transportOrError;
            REQUIRE( transport );
            CHECK( transport->info().codecId == KnownCodecIds::json() );
            CHECK( transport->info().maxRxLength == 64*1024 );
            CHECK( transport->info().maxTxLength == 64*1024 );
            server2 = transport;
            f.sctx.stop();
        });

    f.cnct->establish(
        [&](ErrorOr<Transporting::Ptr> transportOrError)
        {
            REQUIRE( transportOrError.has_value() );
            auto transport = *transportOrError;
            REQUIRE( transport );
            CHECK( transport->info().codecId == KnownCodecIds::json() );
            CHECK( transport->info().maxRxLength == 64*1024 );
            CHECK( transport->info().maxTxLength == 64*1024 );
            client2 = transport;
            f.cctx.stop();
        });

    REQUIRE_NOTHROW( f.run() );

    REQUIRE( client2 );
    REQUIRE( server2 );
    auto sender2 = client2;
    auto receiver2 = server2;

    // The two client/server pairs communicate independently
    receiver2->start(
        nullptr,
        [&](ErrorOr<MessageBuffer> buf)
        {
            if (buf.has_value())
            {
                receivedMessage2 = true;
                CHECK( message2 == *buf );
                receiver2->send(reply2);
            }
            else
            {
                CHECK( buf.error() == TransportErrc::aborted );
            }
        },
        nullptr);

    sender2->start(
        nullptr,
        [&](ErrorOr<MessageBuffer> buf)
        {
            if (buf.has_value())
            {
                receivedReply2 = true;
                CHECK( reply2 == *buf );
                sender2->stop();
                receiver2->stop();
            }
            else
            {
                CHECK( buf.error() == TransportErrc::aborted );
            }
        },
        nullptr);

    sender->send(message);
    sender2->send(message2);

    while (!receivedReply || !receivedReply2)
    {
        f.sctx.poll();
        f.cctx.poll();
    }
    f.sctx.reset();
    f.cctx.reset();

    CHECK( receivedMessage );
    CHECK( receivedReply );
    CHECK( receivedMessage2 );
    CHECK( receivedReply2 );

    f.disconnect();
    REQUIRE_NOTHROW( f.run() );}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE( "Consecutive send/receive", "[Transport]",
                    TcpLoopbackFixture, UdsLoopbackFixture )
{
    {
        TestType f;
        checkConsecutiveSendReceive(f, f.client, f.server);
    }
    {
        TestType f;
        checkConsecutiveSendReceive(f, f.server, f.client);
    }
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE( "Maximum length messages", "[Transport]",
                    TcpLoopbackFixture, UdsLoopbackFixture )
{
    TestType f;
    const MessageBuffer message(f.client->info().maxRxLength, 'm');
    const MessageBuffer reply(f.server->info().maxRxLength, 'r');;
    checkSendReply(f, message, reply);
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE( "Zero length messages", "[Transport]",
                    TcpLoopbackFixture, UdsLoopbackFixture )
{
    const MessageBuffer message;
    const MessageBuffer reply;

    TestType f;
    checkSendReply(f, message, reply);
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE( "Ping/pong messages", "[Transport]",
                    TcpLoopbackFixture, UdsLoopbackFixture )
{
    TestType f;
    auto payload = makeMessageBuffer("hello");
    MessageBuffer pong;

    f.client->start(
        nullptr,
        [&](ErrorOr<MessageBuffer>) {FAIL( "unexpected receive or error");},
        [&](MessageBuffer pongMessage)
        {
            pong = pongMessage;
            f.stop();
        });

    f.server->start(
        nullptr,
        [&](ErrorOr<MessageBuffer>) {FAIL( "unexpected receive or error");},
        [&](MessageBuffer pongMessage)
        {
            pong = pongMessage;
            f.stop();
        });

    f.client->ping(payload);
    CHECK_NOTHROW( f.run() );
    CHECK_THAT(pong, Catch::Matchers::Equals(payload));

    pong.clear();
    payload = makeMessageBuffer("bonjour");
    f.server->ping(payload);
    CHECK_NOTHROW( f.run() );
    CHECK_THAT(pong, Catch::Matchers::Equals(payload));
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE( "Cancel listen", "[Transport]",
                    TcpLoopbackFixture, UdsLoopbackFixture )
{
    auto message = makeMessageBuffer("Hello");
    auto reply = makeMessageBuffer("World");

    TestType f(false);
    f.lstn->establish([&](ErrorOr<Transporting::Ptr> transport)
    {
        CHECK( transport == makeUnexpectedError(TransportErrc::aborted) );
    });
    f.lstn->cancel();
    CHECK_NOTHROW( f.run() );

    // Check that a transport can be established after cancelling.
    checkConnection(f, jsonId);
    checkSendReply(f, message, reply);
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE( "Cancel connect", "[Transport]",
                    TcpLoopbackFixture, UdsLoopbackFixture )
{
    bool listenCompleted = false;
    TestType f(false);
    f.lstn->establish([&](ErrorOr<Transporting::Ptr> transport)
    {
        if (transport.has_value())
            f.server = *transport;
        listenCompleted = true;
    });

    bool connectCanceled = false;
    bool connectCompleted = false;
    f.cnct->establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            if (transport.has_value())
            {
                connectCompleted = true;
                f.client = *transport;
            }
            else
            {
                connectCanceled = true;
                CHECK( transport ==
                       makeUnexpectedError(TransportErrc::aborted) );
            }
            f.lstn->cancel();
        });
    f.cctx.poll();
    f.cctx.reset();

    f.cnct->cancel();
    f.run();

    // Check that the operation either aborts or completes
    REQUIRE( (connectCanceled || connectCompleted) );
    if (connectCanceled)
    {
        CHECK_FALSE( f.client );
        CHECK_FALSE( f.server );
    }
    else if (connectCompleted)
        CHECK( f.client );

    // Check that a transport can be established after cancelling.
    REQUIRE( listenCompleted );
    auto message = makeMessageBuffer("Hello");
    auto reply = makeMessageBuffer("World");
    checkConnection(f, jsonId);
    checkSendReply(f, message, reply);
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE( "Cancel receive", "[Transport]",
                    TcpLoopbackFixture, UdsLoopbackFixture )
{
    TestType f;
    bool clientHandlerInvoked = false;
    f.client->start(
        nullptr,
        [&](ErrorOr<MessageBuffer> buf)
        {
            clientHandlerInvoked = true;
        },
        nullptr);

    std::error_code serverError;
    f.server->start(
        nullptr,
        [&](ErrorOr<MessageBuffer> buf)
        {
            REQUIRE( !buf );
            serverError = buf.error();
        },
        nullptr);

    f.cctx.poll();
    f.cctx.reset();

    // Close the transport while the receive operation is in progress,
    // and check the client handler is not invoked.
    f.client->stop();
    REQUIRE_NOTHROW( f.run() );
    CHECK_FALSE( clientHandlerInvoked );
    CHECK_FALSE( !serverError );
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE( "Cancel send", "[Transport]",
                    TcpLoopbackFixture, UdsLoopbackFixture )
{
    // The size of transmission is set to maximum to increase the likelyhood
    // of the operation being aborted, rather than completed.
    TestType f(false, jsonId, {jsonId}, RML::MB_16, RML::MB_16);
    f.lstn->establish([&](ErrorOr<Transporting::Ptr> transport)
    {
        REQUIRE(transport.has_value());
        f.server = *transport;
    });
    f.cnct->establish([&](ErrorOr<Transporting::Ptr> transport)
    {
        REQUIRE(transport.has_value());
        f.client = *transport;
        CHECK( f.client->info().maxTxLength == 16*1024*1024 );
    });
    f.run();

    // Start a send operation
    bool handlerInvoked = false;
    f.client->start(
        nullptr,
        [&](ErrorOr<MessageBuffer> buf)
        {
            handlerInvoked = true;
        },
        nullptr);
    MessageBuffer message(f.client->info().maxTxLength, 'a');
    f.client->send(message);
    REQUIRE_NOTHROW( f.cctx.poll() );
    f.cctx.reset();

    // Close the transport and check that the client handler was not invoked.
    f.client->stop();
    f.run();
    CHECK_FALSE( handlerInvoked );
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
    checkCannedServerHandshake(0x7f200000, TransportErrc::badLengthLimit);
}
GIVEN( "use of reserved bits" )
{
    checkCannedServerHandshake(0x7f300000, TransportErrc::badFeature);
}
GIVEN( "maximum connections reached" )
{
    checkCannedServerHandshake(0x7f400000, TransportErrc::saturated);
}
GIVEN( "future error code" )
{
    checkCannedServerHandshake(0x7f500000, TransportErrc::failed);
}
}

//------------------------------------------------------------------------------
SCENARIO( "Invalid server handshake", "[Transport]" )
{
GIVEN( "a server that uses an invalid magic octet" )
{
    checkCannedServerHandshake(0xff710000, TransportErrc::badHandshake);
}
GIVEN( "a server that uses a zeroed magic octet" )
{
    checkCannedServerHandshake(0x00710000, TransportErrc::badHandshake);
}
GIVEN( "a server that uses an unspecified serializer" )
{
    checkCannedServerHandshake(0x7f720000, TransportErrc::badHandshake);
}
GIVEN( "a server that uses an unknown serializer" )
{
    checkCannedServerHandshake(0x7f730000, TransportErrc::badHandshake);
}
GIVEN( "a server that uses reserved bits" )
{
    checkCannedServerHandshake(0x7f710001, TransportErrc::badFeature);
}
}

//------------------------------------------------------------------------------
SCENARIO( "Invalid client handshake", "[Transport]" )
{
GIVEN( "a client that uses invalid magic octet" )
{
    checkCannedClientHandshake(0xff710000, TransportErrc::badHandshake,
                               TransportErrc::failed);
}
GIVEN( "a client that uses a zeroed magic octet" )
{
    checkCannedClientHandshake(0x00710000, TransportErrc::badHandshake,
                               TransportErrc::failed);
}
GIVEN( "a client that uses reserved bits" )
{
    checkCannedClientHandshake(0x7f710001, TransportErrc::badFeature,
                               TransportErrc::badFeature);
}
}

//------------------------------------------------------------------------------
SCENARIO( "Client sending a message longer than maximum", "[Transport]" )
{
GIVEN ( "a mock server under-reporting its maximum receive length" )
{
    IoContext ioctx;
    IoStrand strand{ioctx.get_executor()};
    MessageBuffer tooLong(64*1024 + 1, 'A');

    using MockListener = internal::RawsockListener<internal::TcpAcceptor,
                                                   CannedHandshakeConfig>;
    Transporting::Ptr server;
    auto lstn = MockListener::create(strand, tcpEndpoint, {jsonId});
    CannedHandshakeConfig::cannedHostBytes() = 0x7F810000;
    lstn->establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            REQUIRE( transport.has_value() );
            server = std::move(*transport);
        });

    Transporting::Ptr client;
    auto cnct = TcpRawsockConnector::create(strand, tcpHost, jsonId);
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

    WHEN( "the client sends a message that exceeds the server's maximum" )
    {
        bool clientFailed = false;
        bool serverFailed = false;
        client->start(
            nullptr,
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                clientFailed = true;
            },
            nullptr);

        server->start(
            nullptr,
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                serverFailed = true;
            },
            nullptr);

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
SCENARIO( "Server sending a message longer than maximum", "[Transport]" )
{
GIVEN ( "a mock client under-reporting its maximum receive length" )
{
    IoContext ioctx;
    IoStrand strand{ioctx.get_executor()};
    MessageBuffer tooLong(64*1024 + 1, 'A');

    Transporting::Ptr server;
    auto lstn = TcpRawsockListener::create(strand, tcpEndpoint, {jsonId});
    lstn->establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            REQUIRE( transport.has_value() );
            server = std::move(*transport);
        });

    using MockConnector = internal::RawsockConnector<internal::TcpOpener,
                                                     CannedHandshakeConfig>;
    auto cnct = MockConnector::create(strand, tcpHost, jsonId);
    CannedHandshakeConfig::cannedHostBytes() = 0x7F810000;
    Transporting::Ptr client;
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
            nullptr,
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                CHECK( message.error() == TransportErrc::tooLong );
                clientFailed = true;
            },
            nullptr);

        server->start(
            nullptr,
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                serverFailed = true;
            },
            nullptr);

        server->send(tooLong);

        THEN( "the client obtains an error while receiving" )
        {
            CHECK_NOTHROW( ioctx.run() );
            CHECK( clientFailed );
            CHECK( serverFailed );
        }
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Client sending an invalid message type", "[Transport]" )
{
GIVEN ( "A mock client that sends an invalid message type" )
{
    IoContext ioctx;
    IoStrand strand{ioctx.get_executor()};

    auto lstn = TcpRawsockListener::create(strand, tcpEndpoint, {jsonId});
    Transporting::Ptr server;
    lstn->establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            REQUIRE( transport.has_value() );
            server = std::move(*transport);
        });

    using MockConnector = internal::RawsockConnector<internal::TcpOpener,
                                                     FakeTransportClientConfig>;
    auto cnct = MockConnector::create(strand, tcpHost, jsonId);
    Transporting::Ptr client;
    cnct->establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            REQUIRE( transport.has_value() );
            client = *transport;
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
            nullptr,
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                clientFailed = true;
            },
            nullptr);

        server->start(
            nullptr,
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                CHECK( message.error() == TransportErrc::badCommand );
                serverFailed = true;
            },
            nullptr);

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

//------------------------------------------------------------------------------
SCENARIO( "Server sending an invalid message type", "[Transport]" )
{
GIVEN ( "A mock server that sends an invalid message type" )
{
    IoContext ioctx;
    IoStrand strand{ioctx.get_executor()};

    using MockListener = internal::RawsockListener<internal::TcpAcceptor,
                                                   FakeTransportServerOptions>;
    auto lstn = MockListener::create(strand, tcpEndpoint, {jsonId});
    Transporting::Ptr server;
    lstn->establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            REQUIRE( transport.has_value() );
            server = *transport;
        });

    auto cnct = TcpRawsockConnector::create(strand, tcpHost, jsonId);
    Transporting::Ptr client;
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

    WHEN( "the server sends an invalid message to the client" )
    {
        bool clientFailed = false;
        bool serverFailed = false;
        client->start(
            nullptr,
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                CHECK( message.error() == TransportErrc::badCommand );
                clientFailed = true;
            },
            nullptr);

        server->start(
            nullptr,
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                serverFailed = true;
            },
            nullptr);

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
}
