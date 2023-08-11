/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <limits>
#include <catch2/catch.hpp>
#include <cppwamp/asiodefs.hpp>
#include <cppwamp/internal/websocketconnector.hpp>
#include <cppwamp/internal/websocketlistener.hpp>

using namespace wamp;
using namespace wamp::internal;

namespace
{

//------------------------------------------------------------------------------
using CodecIds = std::set<int>;

//------------------------------------------------------------------------------
constexpr auto maxSize = std::numeric_limits<std::size_t>::max();
constexpr auto jsonId = KnownCodecIds::json();
constexpr auto msgpackId = KnownCodecIds::msgpack();
constexpr unsigned short tcpTestPort = 9090;
constexpr const char tcpLoopbackAddr[] = "127.0.0.1";
auto wsHost = WebsocketHost{tcpLoopbackAddr, tcpTestPort};
auto wsEndpoint = WebsocketEndpoint{tcpTestPort};

//------------------------------------------------------------------------------
struct LoopbackFixture
{
    using Connector      = WebsocketConnector;
    using ClientSettings = WebsocketHost;
    using Listener       = WebsocketListener;
    using ServerSettings = WebsocketEndpoint;
    using Transport      = WebsocketTransport;

    LoopbackFixture(ClientSettings clientSettings,
                    int clientCodec,
                    ServerSettings serverSettings,
                    CodecIds serverCodecs,
                    bool connected = true)
    {
        cnct = Connector::create(boost::asio::make_strand(cctx),
                                 std::move(clientSettings), clientCodec);
        lstn = Listener::create(boost::asio::make_strand(sctx),
                                std::move(serverSettings),
                                std::move(serverCodecs));
        if (connected)
            connect();
    }

    LoopbackFixture(bool connected = true,
                    int clientCodec = jsonId,
                    CodecIds serverCodecs = {jsonId},
                    std::size_t clientMaxRxLength = 64*1024,
                    std::size_t serverMaxRxLength = 64*1024)
        : LoopbackFixture(wsHost.withMaxRxLength(clientMaxRxLength),
                          clientCodec,
                          wsEndpoint.withMaxRxLength(serverMaxRxLength),
                          serverCodecs,
                          connected)
    {}

    void connect()
    {
        lstn->establish(
            [&](ErrorOr<Transporting::Ptr> transportOrError)
            {
                auto transport = transportOrError.value();
                serverCodec = transport->info().codecId();
                server = std::move(transport);
            });

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
MessageBuffer makeMessageBuffer(const std::string& str)
{
    using MessageBufferByte = typename MessageBuffer::value_type;
    auto data = reinterpret_cast<const MessageBufferByte*>(str.data());
    return MessageBuffer(data, data + str.size());
}

//------------------------------------------------------------------------------
void checkConnection(LoopbackFixture& f, int expectedCodec,
                     size_t clientMaxRxLength = 64*1024,
                     size_t serverMaxRxLength = 64*1024)
{
    f.lstn->establish([&](ErrorOr<Transporting::Ptr> transportOrError)
    {
        REQUIRE( transportOrError.has_value() );
        auto transport = *transportOrError;
        REQUIRE( transport );
        CHECK( transport->info().codecId() == expectedCodec );
        CHECK( transport->info().maxRxLength() == serverMaxRxLength );
        CHECK( transport->info().maxTxLength() == maxSize );
        f.server = transport;
    });

    f.cnct->establish([&](ErrorOr<Transporting::Ptr> transportOrError)
    {
        REQUIRE( transportOrError.has_value() );
        auto transport = *transportOrError;
        REQUIRE( transport );
        CHECK( transport->info().codecId() == expectedCodec );
        CHECK( transport->info().maxRxLength() == clientMaxRxLength );
        CHECK( transport->info().maxTxLength() == maxSize );
        f.client = transport;
    });

    CHECK_NOTHROW( f.run() );
}

//------------------------------------------------------------------------------
void checkSendReply(LoopbackFixture& f,
                    Transporting::Ptr sender,
                    Transporting::Ptr receiver,
                    const MessageBuffer& message,
                    const MessageBuffer& reply)
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
void checkSendReply(LoopbackFixture& f, const MessageBuffer& message,
                    const MessageBuffer& reply)
{
    checkSendReply(f, f.client, f.server, message, reply);
}

//------------------------------------------------------------------------------
void checkConsecutiveSendReceive(LoopbackFixture& f, Transporting::Ptr& sender,
                                 Transporting::Ptr& receiver)
{
    std::vector<MessageBuffer> messages;
    for (int i=0; i<100; ++i)
        messages.emplace_back(i, 'A' + i);

    sender->start(
        [&](ErrorOr<MessageBuffer> buf)
        {
            REQUIRE( !buf );
            UNSCOPED_INFO( "error message: " << buf.error().message() );
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
                    sender->stop();
                }
            }
            else
            {
                UNSCOPED_INFO( "error message: " << buf.error().message() );
                CHECK( buf.error() == TransportErrc::aborted );
            }
        },
        nullptr);

    for (const auto& msg: messages)
        sender->send(msg);

    CHECK_NOTHROW( f.run() );
}

//------------------------------------------------------------------------------
void checkUnsupportedSerializer(LoopbackFixture& f)
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

} // anonymous namespace

//------------------------------------------------------------------------------
TEST_CASE( "Normal websocket connection", "[Transport][Websocket]" )
{
    SECTION( "the client and server use JSON" )
    {
        LoopbackFixture f(false, jsonId, {jsonId}, 32*1024, 128*1024);
        checkConnection(f, jsonId, 32*1024, 128*1024);
    }
    SECTION( "the client uses JSON and the server supports both" )
    {
        LoopbackFixture f(false, jsonId, {jsonId, msgpackId},
                          32*1024, 128*1024 );
        checkConnection(f, jsonId, 32*1024, 128*1024);
    }
    SECTION( "the client and server use Msgpack" )
    {
        LoopbackFixture f(false, msgpackId, {msgpackId}, 32*1024, 128*1024);
        checkConnection(f, msgpackId, 32*1024, 128*1024);
    }
    SECTION( "the client uses Msgpack and the server supports both" )
    {
        LoopbackFixture f(false, msgpackId, {jsonId, msgpackId},
                          32*1024, 128*1024);
        checkConnection(f, msgpackId, 32*1024, 128*1024);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "Normal websocket communications", "[Transport][Websocket]" )
{
    LoopbackFixture f;
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

    f.lstn->establish(
        [&](ErrorOr<Transporting::Ptr> transportOrError)
        {
            REQUIRE( transportOrError.has_value() );
            auto transport = *transportOrError;
            REQUIRE( transport );
            CHECK( transport->info().codecId() == KnownCodecIds::json() );
            CHECK( transport->info().maxRxLength() == 64*1024 );
            CHECK( transport->info().maxTxLength() == maxSize );
            server2 = transport;
            f.sctx.stop();
        });

    f.cnct->establish(
        [&](ErrorOr<Transporting::Ptr> transportOrError)
        {
            REQUIRE( transportOrError.has_value() );
            auto transport = *transportOrError;
            REQUIRE( transport );
            CHECK( transport->info().codecId() == KnownCodecIds::json() );
            CHECK( transport->info().maxRxLength() == 64*1024 );
            CHECK( transport->info().maxTxLength() == maxSize );
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

    while (!receivedReply || !receivedReply2) // Stuck here
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
TEST_CASE( "Consecutive websocket send/receive", "[Transport][Websocket]" )
{
    {
        LoopbackFixture f;
        checkConsecutiveSendReceive(f, f.client, f.server);
    }
    {
        LoopbackFixture f;
        checkConsecutiveSendReceive(f, f.server, f.client);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "Maximum length websocket messages", "[Transport][Websocket]" )
{
    LoopbackFixture f;
    const MessageBuffer message(f.client->info().maxRxLength(), 'm');
    const MessageBuffer reply(f.server->info().maxRxLength(), 'r');;
    checkSendReply(f, message, reply);
}

//------------------------------------------------------------------------------
TEST_CASE( "Zero length websocket messages", "[Transport][Websocket]" )
{
    const MessageBuffer message;
    const MessageBuffer reply;

    LoopbackFixture f;
    checkSendReply(f, message, reply);
}

//------------------------------------------------------------------------------
TEST_CASE( "Cancel websocket listen", "[Transport][Websocket]" )
{
    auto message = makeMessageBuffer("Hello");
    auto reply = makeMessageBuffer("World");

    LoopbackFixture f(false);
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
TEST_CASE( "Cancel websocket connect", "[Transport][Websocket]" )
{
    bool listenCompleted = false;
    LoopbackFixture f(false);
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
TEST_CASE( "Cancel websocket receive", "[Transport][Websocket]" )
{
    LoopbackFixture f;
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
TEST_CASE( "Cancel websocket send", "[Transport][Websocket]" )
{
    // The size of transmission is set large to increase the likelyhood
    // of the operation being aborted, rather than completed.
    LoopbackFixture f(false, jsonId, {jsonId}, 16*1024*1024, 16*1024*1024);
    f.lstn->establish([&](ErrorOr<Transporting::Ptr> transport)
    {
        REQUIRE(transport.has_value());
        f.server = *transport;
    });
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
TEST_CASE( "Unsupported websocket serializer", "[Transport][Websocket]" )
{
SECTION( "a JSON client and a Msgpack server" )
{
    LoopbackFixture f(false, jsonId, {msgpackId});
    checkUnsupportedSerializer(f);
}
SECTION( "a Msgpack client and a JSON server" )
{
    LoopbackFixture f(false, msgpackId, {jsonId});
    checkUnsupportedSerializer(f);
}
}

//------------------------------------------------------------------------------
TEST_CASE( "Peer sending a websocket message longer than maximum",
           "[Transport][Websocket]" )
{
    LoopbackFixture f;
    Transporting::Ptr client = f.client;
    Transporting::Ptr server = f.server;
    MessageBuffer tooLong(64*1024 + 1, 'A');

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
            serverFailed = true;
        },
        nullptr);

    SECTION("Client sending overly long message")
    {
        client->send(std::move(tooLong));

        CHECK_NOTHROW( f.run() );
        CHECK( clientFailed );
        CHECK( serverFailed );
    }

    SECTION("Server sending overly long message")
    {
        server->send(std::move(tooLong));

        CHECK_NOTHROW( f.run() );
        CHECK( clientFailed );
        CHECK( serverFailed );
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "Websocket heartbeat", "[Transport][Websocket]" )
{
    const std::chrono::milliseconds interval{50};

    LoopbackFixture f{wsHost.withHearbeatInterval(interval), jsonId,
                      wsEndpoint, {jsonId}, true};
    Transporting::Ptr client = f.client;
    Transporting::Ptr server = f.server;
    boost::asio::steady_timer timer{f.cctx};

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
    // no error occurred.
    timer.expires_after(3*interval + interval/2);
    timer.async_wait([&f](boost::system::error_code) {f.stop();});
    f.run();

    CHECK(!clientError);
    CHECK(!serverError);
}
