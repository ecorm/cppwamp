/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <set>
#include <thread>
#include <vector>
#include <boost/asio/steady_timer.hpp>
#include <catch2/catch.hpp>
#include <cppwamp/asiodefs.hpp>
#include <cppwamp/codec.hpp>
#include <cppwamp/errorcodes.hpp>
#include <cppwamp/rawsockoptions.hpp>
#include <cppwamp/transport.hpp>
#include <cppwamp/internal/tcpconnector.hpp>
#include <cppwamp/internal/tcplistener.hpp>
#include <cppwamp/internal/udsconnector.hpp>
#include <cppwamp/internal/udslistener.hpp>
#include "silentclient.hpp"

using namespace wamp;
using namespace wamp::internal;

namespace
{

//------------------------------------------------------------------------------
using RML = RawsockMaxLength;

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

    LoopbackFixture(ClientSettings clientSettings,
                    int clientCodec,
                    ServerSettings serverSettings,
                    CodecIdSet serverCodecs,
                    bool connected = true)
    {
        cnct = Connector::create(boost::asio::make_strand(cctx),
                                 std::move(clientSettings), clientCodec);
        lstn = Listener::create(sctx.get_executor(),
                                boost::asio::make_strand(sctx),
                                std::move(serverSettings),
                                std::move(serverCodecs));
        if (connected)
            connect();
    }

    void connect()
    {
        lstn->observe(
            [&](ListenResult result)
            {
                auto transport = result.transport();
                server = std::move(transport);
                server->admit(
                    [this](ErrorOr<int> codecId)
                    {
                        if (codecId.has_value())
                            serverCodec = *codecId;
                    });
            });
        lstn->establish();

        cnct->establish(
            [&](ErrorOr<Transporting::Ptr> transportOrError)
            {
                auto transport = transportOrError.value();
                clientCodec = transport->info().codecId();
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
struct TcpLoopbackFixture : public LoopbackFixture<TcpConnector, TcpListener>
{
    TcpLoopbackFixture(
                bool connected = true,
                int clientCodec = jsonId,
                CodecIdSet serverCodecs = {jsonId},
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
struct UdsLoopbackFixture : public LoopbackFixture<UdsConnector, UdsListener>
{
    UdsLoopbackFixture(
                bool connected = true,
                int clientCodec = jsonId,
                CodecIdSet serverCodecs = {jsonId},
                RawsockMaxLength clientMaxRxLength = RML::kB_64,
                RawsockMaxLength serverMaxRxLength = RML::kB_64 )
        : LoopbackFixture(
              UdsHost{udsTestPath}.withMaxRxLength(clientMaxRxLength),
              clientCodec,
              UdsEndpoint{udsTestPath}.withMaxRxLength(serverMaxRxLength),
              serverCodecs,
              connected )
    {}
};

//------------------------------------------------------------------------------
struct CannedHandshakeServerTransportConfig
    : BasicRawsockTransportConfig<TcpTraits>
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

using CannedHandshakeListener =
    RawsockListener<
        BasicTcpListenerConfig<
            RawsockServerTransport<CannedHandshakeServerTransportConfig>>>;

//------------------------------------------------------------------------------
struct CannedHandshakeConnectorConfig : TcpConnectorConfig
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

using CannedHandshakeConnector =
    RawsockConnector<CannedHandshakeConnectorConfig>;

//------------------------------------------------------------------------------
struct BadMsgKindConfig : BasicRawsockTransportConfig<TcpTraits>
{
    static void preProcess(TransportFrameKind& kind, MessageBuffer&)
    {
        auto badKind = TransportFrameKind((int)TransportFrameKind::pong + 1);
        kind = badKind;
    }
};

//------------------------------------------------------------------------------
struct MonitorPingPongConfig : BasicRawsockTransportConfig<TcpTraits>
{
    static void preProcess(TransportFrameKind& kind, MessageBuffer& buf)
    {
        if (kind == TransportFrameKind::ping)
        {
            pings().push_back(buf);
        }
        else if (kind == TransportFrameKind::pong)
        {
            if (!cannedPong().empty())
                buf = cannedPong();
            pongs().push_back(buf);
        }
    }

    using BufferList = std::vector<MessageBuffer>;

    static void clear()
    {
        pings().clear();
        pongs().clear();
        cannedPong().clear();
    }

    static BufferList& pings()
    {
        static BufferList pingList;
        return pingList;
    }

    static BufferList& pongs()
    {
        static BufferList pongList;
        return pongList;
    }

    static MessageBuffer& cannedPong()
    {
        static MessageBuffer cannedPongBuffer;
        return cannedPongBuffer;
    }
};

//------------------------------------------------------------------------------
using PingPongConnectorConfig =
    BasicTcpConnectorConfig<RawsockClientTransport<MonitorPingPongConfig>>;

//------------------------------------------------------------------------------
using PingPongListenerConfig =
    BasicTcpListenerConfig<RawsockServerTransport<MonitorPingPongConfig>>;

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
    f.lstn->observe([&](ListenResult result)
    {
        REQUIRE( result.ok() );
        auto transport = result.transport();
        REQUIRE( transport != nullptr );
        f.server = transport;
        f.server->admit(
            [=](ErrorOr<int> codecId)
            {
                REQUIRE(codecId.has_value());
                CHECK(*codecId == expectedCodec);
                CHECK( transport->info().codecId() == expectedCodec );
                CHECK( transport->info().maxRxLength() == serverMaxRxLength );
                CHECK( transport->info().maxTxLength() == clientMaxRxLength );
            });
    });
    f.lstn->establish();

    f.cnct->establish([&](ErrorOr<Transporting::Ptr> transportOrError)
    {
        REQUIRE( transportOrError.has_value() );
        auto transport = *transportOrError;
        REQUIRE( transport != nullptr );
        CHECK( transport->info().codecId() == expectedCodec );
        CHECK( transport->info().maxRxLength() == clientMaxRxLength );
        CHECK( transport->info().maxTxLength() == serverMaxRxLength );
        f.client = transport;
    });

    CHECK_NOTHROW( f.run() );
}

//------------------------------------------------------------------------------
template <typename TFixture>
void checkSendReply(
    TFixture& f, Transporting::Ptr sender, Transporting::Ptr receiver,
    const MessageBuffer& message, const MessageBuffer& reply)
{
    bool receivedMessage = false;
    bool receivedReply = false;

    receiver->start(
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
        [&](ErrorOr<MessageBuffer> buf)
        {
            REQUIRE( !buf );
            CHECK( buf.error() == TransportErrc::aborted );
        },
        nullptr);

    size_t count = 0;

    receiver->start(
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
    std::error_code serverEc;
    std::error_code clientEc;

    f.lstn->observe([&](ListenResult result)
    {
        REQUIRE( result.ok() );
        f.server = result.transport();
        f.server->admit(
            [&serverEc](ErrorOr<int> codecId)
            {
                if (!codecId.has_value())
                    serverEc = codecId.error();
            });
    });
    f.lstn->establish();

    f.cnct->establish([&clientEc](ErrorOr<Transporting::Ptr> transport)
    {
        if (!transport.has_value())
            clientEc = transport.error();
    });

    CHECK_NOTHROW( f.run() );
    CHECK( serverEc == TransportErrc::badSerializer );
    CHECK( clientEc == TransportErrc::badSerializer );
}

//------------------------------------------------------------------------------
void checkCannedServerHandshake(
    uint32_t cannedHandshake, TransportErrc expectedClientErrc,
    TransportErrc expectedServerErrc = TransportErrc::success)
{
    IoContext ioctx;
    auto exec = ioctx.get_executor();
    auto strand = boost::asio::make_strand(exec);
    auto lstn = CannedHandshakeListener::create(exec, strand, tcpEndpoint,
                                                {jsonId});
    Transporting::Ptr server;
    CannedHandshakeServerTransportConfig::cannedHostBytes() = cannedHandshake;
    std::error_code serverEc;
    std::error_code clientEc;

    lstn->observe( [&](ListenResult result)
    {
        REQUIRE( result.ok() );
        server = result.transport();
        server->admit(
            [&serverEc](ErrorOr<int> codecId)
            {
                if (!codecId.has_value())
                    serverEc = codecId.error();
            });
    });
    lstn->establish();

    auto cnct = TcpConnector::create(strand, tcpHost, jsonId);
    cnct->establish(
        [&clientEc](ErrorOr<Transporting::Ptr> transport)
        {
            if (!transport.has_value())
                clientEc = transport.error();
        });

    CHECK_NOTHROW( ioctx.run() );
    CHECK( clientEc == expectedClientErrc );
    CHECK( serverEc == expectedServerErrc );
}

//------------------------------------------------------------------------------
void checkCannedClientHandshake(uint32_t cannedHandshake,
                                TransportErrc expectedServerCode,
                                TransportErrc expectedClientCode)
{
    IoContext ioctx;
    auto exec = ioctx.get_executor();
    auto strand = boost::asio::make_strand(exec);
    auto lstn = TcpListener::create(exec, strand, tcpEndpoint, {jsonId});
    Transporting::Ptr server;
    std::error_code serverEc;
    std::error_code clientEc;

    lstn->observe(
        [&](ListenResult result)
        {
            REQUIRE( result.ok() );
            server = result.transport();
            server->admit(
                [&serverEc](ErrorOr<int> codecId)
                {
                    if (!codecId.has_value())
                        serverEc = codecId.error();
                });
        });
    lstn->establish();

    auto cnct = CannedHandshakeConnector::create(strand, tcpHost, jsonId);
    CannedHandshakeConnectorConfig::cannedHostBytes() = cannedHandshake;
    cnct->establish(
        [&clientEc](ErrorOr<Transporting::Ptr> transport)
        {
            if (!transport.has_value())
                clientEc = transport.error();
        });

    CHECK_NOTHROW( ioctx.run() );
    CHECK( serverEc == expectedServerCode );
    CHECK( clientEc == expectedClientCode );
}

} // anonymous namespace

//------------------------------------------------------------------------------
SCENARIO( "Normal connection", "[Transport][Rawsock]" )
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
TEMPLATE_TEST_CASE( "Normal communications", "[Transport][Rawsock]",
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

    f.lstn->observe(
        [&](ListenResult result)
        {
            REQUIRE( result.ok() );
            auto transport = result.transport();
            REQUIRE( transport != nullptr );
            server2 = transport;
            server2->admit(
                [=, &f](ErrorOr<int> codecId)
                {
                    REQUIRE(codecId.has_value());
                    CHECK( codecId.value() == KnownCodecIds::json() );
                    CHECK( transport->info().codecId() == KnownCodecIds::json() );
                    CHECK( transport->info().maxRxLength() == 64*1024 );
                    CHECK( transport->info().maxTxLength() == 64*1024 );
                    f.sctx.stop();
                });
        });
    f.lstn->establish();

    f.cnct->establish(
        [&](ErrorOr<Transporting::Ptr> transportOrError)
        {
            REQUIRE( transportOrError.has_value() );
            auto transport = *transportOrError;
            REQUIRE( transport );
            CHECK( transport->info().codecId() == KnownCodecIds::json() );
            CHECK( transport->info().maxRxLength() == 64*1024 );
            CHECK( transport->info().maxTxLength() == 64*1024 );
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
TEMPLATE_TEST_CASE( "Consecutive send/receive", "[Transport][Rawsock]",
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
TEMPLATE_TEST_CASE( "Maximum length messages", "[Transport][Rawsock]",
                    TcpLoopbackFixture, UdsLoopbackFixture )
{
    TestType f;
    const MessageBuffer message(f.client->info().maxRxLength(), 'm');
    const MessageBuffer reply(f.server->info().maxRxLength(), 'r');;
    checkSendReply(f, message, reply);
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE( "Zero length messages", "[Transport][Rawsock]",
                    TcpLoopbackFixture, UdsLoopbackFixture )
{
    const MessageBuffer message;
    const MessageBuffer reply;

    TestType f;
    checkSendReply(f, message, reply);
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE( "Cancel listen", "[Transport][Rawsock]",
                    TcpLoopbackFixture, UdsLoopbackFixture )
{
    auto message = makeMessageBuffer("Hello");
    auto reply = makeMessageBuffer("World");

    TestType f(false);
    f.lstn->observe([&](ListenResult result)
    {
        REQUIRE_FALSE( result.ok() );
        CHECK( result.error() == TransportErrc::aborted );
    });
    f.lstn->establish();
    f.lstn->cancel();
    CHECK_NOTHROW( f.run() );

    // Check that a transport can be established after cancelling.
    checkConnection(f, jsonId);
    checkSendReply(f, message, reply);
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE( "Cancel connect", "[Transport][Rawsock]",
                    TcpLoopbackFixture, UdsLoopbackFixture )
{
    bool listenCompleted = false;
    std::error_code listenEc;
    TestType f(false);
    f.lstn->observe([&](ListenResult result)
    {
        if (result.ok())
        {
            f.server = result.transport();
            f.server->admit(
                [&](ErrorOr<int> codecId)
                {
                    listenCompleted = true;
                    if (!codecId.has_value())
                        listenEc = codecId.error();
                });
        }
        else
        {
            listenCompleted = true;
            listenEc = result.error();
        }
    });
    f.lstn->establish();

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
    }
    else if (connectCompleted)
    {
        CHECK( f.client );
    }
    if (listenEc)
    {
        INFO("listenEc.message(): " << listenEc.message());
        CHECK(((listenEc == TransportErrc::disconnected) ||
               (listenEc == TransportErrc::aborted)));
    }

    // Check that a transport can be established after cancelling.
    REQUIRE( listenCompleted );
    auto message = makeMessageBuffer("Hello");
    auto reply = makeMessageBuffer("World");
    checkConnection(f, jsonId);
    checkSendReply(f, message, reply);
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE( "Cancel receive", "[Transport][Rawsock]",
                    TcpLoopbackFixture, UdsLoopbackFixture )
{
    TestType f;
    bool clientHandlerInvoked = false;
    f.client->start(
        [&](ErrorOr<MessageBuffer> buf)
        {
            clientHandlerInvoked = true;
        },
        nullptr);

    std::error_code serverError;
    f.server->start(
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
TEMPLATE_TEST_CASE( "Cancel send", "[Transport][Rawsock]",
                    TcpLoopbackFixture, UdsLoopbackFixture )
{
    // The size of transmission is set to maximum to increase the likelyhood
    // of the operation being aborted, rather than completed.
    TestType f(false, jsonId, {jsonId}, RML::MB_16, RML::MB_16);
    f.lstn->observe([&](ListenResult result)
    {
        REQUIRE(result.ok());
        f.server = result.transport();
        f.server->admit(
            [](ErrorOr<int> codecId) {REQUIRE(codecId.has_value());});
    });
    f.lstn->establish();
    f.cnct->establish([&](ErrorOr<Transporting::Ptr> transport)
    {
        REQUIRE(transport.has_value());
        f.client = *transport;
        CHECK( f.client->info().maxTxLength() == 16*1024*1024 );
    });
    f.run();

    // Start a send operation
    bool handlerInvoked = false;
    f.client->start(
        [&](ErrorOr<MessageBuffer> buf)
        {
            handlerInvoked = true;
        },
        nullptr);
    MessageBuffer message(f.client->info().maxTxLength(), 'a');
    f.client->send(message);
    REQUIRE_NOTHROW( f.cctx.poll() );
    f.cctx.reset();

    // Close the transport and check that the client handler was not invoked.
    f.client->stop();
    f.run();
    CHECK_FALSE( handlerInvoked );
}

//------------------------------------------------------------------------------
SCENARIO( "Unsupported serializer", "[Transport][Rawsock]" )
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
SCENARIO( "Connection denied by server", "[Transport][Rawsock]" )
{
GIVEN( "max length is unacceptable" )
{
    checkCannedServerHandshake(0x7f200000, TransportErrc::badLengthLimit,
                               TransportErrc::badLengthLimit);
}
GIVEN( "use of reserved bits" )
{
    checkCannedServerHandshake(0x7f300000, TransportErrc::badFeature,
                               TransportErrc::badFeature);
}
GIVEN( "maximum connections reached" )
{
    checkCannedServerHandshake(0x7f400000, TransportErrc::overloaded,
                               TransportErrc::overloaded);
}
GIVEN( "future error code" )
{
    checkCannedServerHandshake(0x7f500000, TransportErrc::failed,
                               TransportErrc::failed);
}
}

//------------------------------------------------------------------------------
SCENARIO( "Invalid server handshake", "[Transport][Rawsock]" )
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
SCENARIO( "Invalid client handshake", "[Transport][Rawsock]" )
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
SCENARIO( "Client sending a message longer than maximum",
          "[Transport][Rawsock]" )
{
GIVEN ( "a mock server under-reporting its maximum receive length" )
{
    IoContext ioctx;
    auto exec = ioctx.get_executor();
    auto strand = boost::asio::make_strand(exec);
    MessageBuffer tooLong(64*1024 + 1, 'A');

    Transporting::Ptr server;
    auto lstn = CannedHandshakeListener::create(exec, strand, tcpEndpoint,
                                                {jsonId});
    CannedHandshakeServerTransportConfig::cannedHostBytes() = 0x7F810000;
    lstn->observe(
        [&](ListenResult result)
        {
            REQUIRE( result.ok() );
            server = result.transport();
            server->admit(
                [](ErrorOr<int> codecId) {REQUIRE(codecId.has_value());});
        });
    lstn->establish();

    Transporting::Ptr client;
    auto cnct = TcpConnector::create(strand, tcpHost, jsonId);
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
        std::error_code clientError;
        std::error_code serverError;
        client->start(
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                clientError = message.error();
            },
            nullptr);

        server->start(
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                serverError = message.error();
            },
            nullptr);

        client->send(std::move(tooLong));

        THEN( "the server obtains an error while receiving" )
        {
            CHECK_NOTHROW( ioctx.run() );
            UNSCOPED_INFO("client error message:" << clientError.message());
            UNSCOPED_INFO("server error message:" << serverError.message());
            CHECK( clientError == TransportErrc::disconnected );
            CHECK( serverError == TransportErrc::inboundTooLong );
        }
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Server sending a message longer than maximum",
          "[Transport][Rawsock]" )
{
GIVEN ( "a mock client under-reporting its maximum receive length" )
{
    IoContext ioctx;
    auto exec = ioctx.get_executor();
    auto strand = boost::asio::make_strand(exec);
    MessageBuffer tooLong(64*1024 + 1, 'A');

    Transporting::Ptr server;
    auto lstn = TcpListener::create(exec, strand, tcpEndpoint, {jsonId});
    lstn->observe(
        [&](ListenResult result)
        {
            REQUIRE( result.ok() );
            server = result.transport();
            server->admit(
                [](ErrorOr<int> codecId) {REQUIRE(codecId.has_value());});
        });
    lstn->establish();

    auto cnct = CannedHandshakeConnector::create(strand, tcpHost, jsonId);
    CannedHandshakeConnectorConfig::cannedHostBytes() = 0x7F810000;
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
        std::error_code clientError;
        std::error_code serverError;
        client->start(
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                clientError = message.error();
            },
            nullptr);

        server->start(
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                serverError = message.error();
            },
            nullptr);

        server->send(tooLong);

        THEN( "the client obtains an error while receiving" )
        {
            CHECK_NOTHROW( ioctx.run() );
            UNSCOPED_INFO("client error message:" << clientError.message());
            UNSCOPED_INFO("server error message:" << serverError.message());
            CHECK( clientError == TransportErrc::inboundTooLong );
            CHECK( serverError == TransportErrc::disconnected );
        }
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Client sending an invalid message type", "[Transport][Rawsock]" )
{
GIVEN ( "A mock client that sends an invalid message type" )
{
    IoContext ioctx;
    auto exec = ioctx.get_executor();
    auto strand = boost::asio::make_strand(exec);

    auto lstn = TcpListener::create(exec, strand, tcpEndpoint, {jsonId});
    Transporting::Ptr server;
    lstn->observe(
        [&](ListenResult result)
        {
            REQUIRE( result.ok() );
            server = result.transport();
            server->admit(
                [](ErrorOr<int> codecId) {REQUIRE(codecId.has_value());});
        });
    lstn->establish();

    using MockConnector =
        RawsockConnector<
            BasicTcpConnectorConfig<RawsockClientTransport<BadMsgKindConfig>>>;

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
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                clientFailed = true;
            },
            nullptr);

        server->start(
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
SCENARIO( "Server sending an invalid message type", "[Transport][Rawsock]" )
{
GIVEN ( "A mock server that sends an invalid message type" )
{
    using MockListener =
        RawsockListener<
            BasicTcpListenerConfig<RawsockServerTransport<BadMsgKindConfig>>>;

    IoContext ioctx;
    auto exec = ioctx.get_executor();
    auto strand = boost::asio::make_strand(exec);
    auto lstn = MockListener::create(exec, strand, tcpEndpoint, {jsonId});
    Transporting::Ptr server;
    lstn->observe(
        [&](ListenResult result)
        {
            REQUIRE( result.ok() );
            server = result.transport();
            server->admit(
                [](ErrorOr<int> codecId) {REQUIRE(codecId.has_value());});
        });
    lstn->establish();

    auto cnct = TcpConnector::create(strand, tcpHost, jsonId);
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
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                CHECK( message.error() == TransportErrc::badCommand );
                clientFailed = true;
            },
            nullptr);

        server->start(
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

//------------------------------------------------------------------------------
TEST_CASE( "TCP server transport handshake timeout", "[Transport][Rawsock]" )
{
    IoContext ioctx;
    auto exec = ioctx.get_executor();
    auto strand = boost::asio::make_strand(exec);
    std::error_code serverError;

    auto lstn = TcpListener::create(exec, strand, tcpEndpoint, {jsonId});
    Transporting::Ptr server;
    lstn->observe(
        [&](ListenResult result)
        {
            REQUIRE( result.ok() );
            server = result.transport();
            server->admit(
                std::chrono::milliseconds(50),
                [&serverError](ErrorOr<int> codecId)
                {
                    if (!codecId.has_value())
                        serverError = codecId.error();
                });
        });
    lstn->establish();

    using boost::asio::ip::tcp;

    test::SilentClient client{ioctx};
    client.run(tcpEndpoint.port());

    ioctx.run();
    CHECK(client.readError() == boost::asio::error::eof);
    CHECK(serverError == TransportErrc::timeout);
}

//------------------------------------------------------------------------------
TEST_CASE( "TCP rawsocket heartbeat", "[Transport][Rawsock]" )
{
    IoContext ioctx;
    auto exec = ioctx.get_executor();
    auto strand = boost::asio::make_strand(exec);
    boost::asio::steady_timer timer{ioctx};

    using MockListener =
        RawsockListener<
            BasicTcpListenerConfig<
                RawsockServerTransport<MonitorPingPongConfig>>>;

    auto lstn = MockListener::create(exec, strand, tcpEndpoint, {jsonId});
    Transporting::Ptr server;
    lstn->observe(
        [&](ListenResult result)
        {
            REQUIRE( result.ok() );
            server = result.transport();
            server->admit(
                [](ErrorOr<int> codecId) {REQUIRE(codecId.has_value());});
        });
    lstn->establish();

    using MockConnector =
        RawsockConnector<
            BasicTcpConnectorConfig<
                RawsockClientTransport<MonitorPingPongConfig>>>;

    const std::chrono::milliseconds interval{50};
    const auto where = TcpHost{tcpLoopbackAddr, tcpTestPort}
                           .withHearbeatInterval(interval);

    MonitorPingPongConfig::clear();

    auto cnct = MockConnector::create(strand, where, jsonId);
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

    std::error_code clientError;
    client->start(
        [&clientError](ErrorOr<MessageBuffer> m)
        {
            if (!m)
            {
                clientError = m.error();
                UNSCOPED_INFO("client error code: " << m.error());
            }
        },
        nullptr);

    std::error_code serverError;
    server->start(
        [&serverError](ErrorOr<MessageBuffer> m)
        {
            if (!m)
            {
                serverError = m.error();
                UNSCOPED_INFO("server error code: " << m.error());
            }
        },
        nullptr);

    // Wait the expected time for 3 ping/pong exchanges and check that
    // they actually occurred.
    timer.expires_after(3*interval + interval/2);
    timer.async_wait([&ioctx](boost::system::error_code) {ioctx.stop();});
    ioctx.run();
    ioctx.restart();

    CHECK(!clientError);
    CHECK(!serverError);
    CHECK(MonitorPingPongConfig::pings().size() == 3);
    CHECK(MonitorPingPongConfig::pongs().size() == 3);
    CHECK_THAT(MonitorPingPongConfig::pings(),
               Catch::Matchers::Equals(MonitorPingPongConfig::pongs()));

    // Make the server stop echoing the correct pong and check that the client
    // fails due to heartbeat timeout.
    MonitorPingPongConfig::cannedPong() = MessageBuffer{0x12, 0x34, 0x56};
    timer.expires_after(2*interval);
    timer.async_wait([&ioctx](boost::system::error_code) {ioctx.stop();});
    ioctx.run();
    CHECK(clientError == TransportErrc::unresponsive);
    CHECK(serverError == TransportErrc::disconnected);
}
