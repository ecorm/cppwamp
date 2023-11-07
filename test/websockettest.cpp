/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#if defined(CPPWAMP_TEST_HAS_WEB)

#include <limits>
#include <catch2/catch.hpp>
#include <cppwamp/asiodefs.hpp>
#include <cppwamp/codecs/cbor.hpp>
#include <cppwamp/internal/websocketconnector.hpp>
#include <cppwamp/internal/websocketlistener.hpp>
#include <cppwamp/transports/websocketclient.hpp>
#include <cppwamp/transports/websocketserver.hpp>
#include "silentclient.hpp"

#if defined(CPPWAMP_TEST_HAS_CORO)
#include <cppwamp/session.hpp>
#include <cppwamp/spawn.hpp>
#include "routerfixture.hpp"
#endif

using namespace wamp;
using namespace wamp::internal;

namespace
{

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

    LoopbackFixture(ClientSettings clientSettings,
                    int clientCodec,
                    ServerSettings serverSettings,
                    CodecIdSet serverCodecs,
                    bool connected = true)
    {
        cnct = std::make_shared<Connector>(
            boost::asio::make_strand(cctx), std::move(clientSettings),
            clientCodec);
        lstn = std::make_shared<Listener>(
            sctx.get_executor(), boost::asio::make_strand(sctx),
            std::move(serverSettings), std::move(serverCodecs));
        if (connected)
            connect();
    }

    LoopbackFixture(bool connected = true,
                    int clientCodec = jsonId,
                    CodecIdSet serverCodecs = {jsonId},
                    std::size_t clientLimit = 64*1024,
                    std::size_t serverLimit = 64*1024)
        : LoopbackFixture(
            wsHost.withLimits(
                WebsocketClientLimits{}.withRxMsgSize(clientLimit)),
            clientCodec,
            wsEndpoint.withLimits(
                WebsocketServerLimits{}.withRxMsgSize(serverLimit)),
            serverCodecs,
            connected)
    {}

    void connect()
    {
        lstn->observe(
            [&](ListenResult result)
            {
                auto transport = result.transport();
                server = std::move(transport);
                server->admit(
                    [this](AdmitResult result)
                    {
                        if (result.status() == AdmitStatus::wamp)
                            serverCodec = result.codecId();
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
        server->close();
        client->close();
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
void suspendCoro(YieldContext yield)
{
    auto exec = boost::asio::get_associated_executor(yield);
    boost::asio::post(exec, yield);
}

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
    f.lstn->observe([&](ListenResult result)
    {
        REQUIRE( result.ok() );
        auto transport = result.transport();
        REQUIRE( transport );
        f.server = transport;
        f.server->admit(
            [=](AdmitResult result)
            {
                REQUIRE( result.status() == AdmitStatus::wamp );
                CHECK( result.codecId() == expectedCodec );
                CHECK( transport->info().codecId() == expectedCodec );
                CHECK( transport->info().receiveLimit() == serverMaxRxLength );
                CHECK( transport->info().sendLimit() ==
                       WebsocketServerLimits{}.txMsgSize() );
            });
    });
    f.lstn->establish();

    f.cnct->establish([&](ErrorOr<Transporting::Ptr> transportOrError)
    {
        REQUIRE( transportOrError.has_value() );
        auto transport = *transportOrError;
        REQUIRE( transport );
        CHECK( transport->info().codecId() == expectedCodec );
        CHECK( transport->info().receiveLimit() == clientMaxRxLength );
        CHECK( transport->info().sendLimit() ==
               WebsocketClientLimits{}.txMsgSize() );
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
                    sender->close();
                }
            }
            else
            {
                UNSCOPED_INFO( "error message: " << buf.error().message() );
                CHECK( buf.error() == TransportErrc::disconnected );
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
    std::error_code serverEc;
    std::error_code clientEc;

    f.lstn->observe([&](ListenResult result)
    {
        REQUIRE( result.ok() );
        f.server = result.transport();
        f.server->admit(
            [&serverEc](AdmitResult result) {serverEc = result.error();});
    });
    f.lstn->establish();

    f.cnct->establish([&](ErrorOr<Transporting::Ptr> transport)
    {
        if (!transport.has_value())
            clientEc = transport.error();
    });

    CHECK_NOTHROW( f.run() );
    CHECK( serverEc == TransportErrc::badSerializer );
    CHECK( clientEc == HttpStatus::badRequest );
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
        if (!f.sctx.stopped())
            f.sctx.poll();
        if (!f.cctx.stopped())
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
                [=, &f](AdmitResult result)
                {
                    REQUIRE( result.status() == AdmitStatus::wamp );
                    CHECK( result.codecId() == KnownCodecIds::json() );
                    CHECK( transport->info().codecId() == KnownCodecIds::json() );
                    CHECK( transport->info().receiveLimit() == 64*1024 );
                    CHECK( transport->info().sendLimit() ==
                           WebsocketServerLimits{}.txMsgSize() );
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
            CHECK( transport->info().receiveLimit() == 64*1024 );
            CHECK( transport->info().sendLimit() ==
                   WebsocketClientLimits{}.txMsgSize() );
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
                sender2->close();
                receiver2->close();
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
        if (!f.sctx.stopped())
            f.sctx.poll();
        if (!f.cctx.stopped())
            f.cctx.poll();
    }
    f.sctx.reset();
    f.cctx.reset();

    CHECK( receivedMessage );
    CHECK( receivedReply );
    CHECK( receivedMessage2 );
    CHECK( receivedReply2 );

    f.disconnect();
    REQUIRE_NOTHROW( f.run() );
}

//------------------------------------------------------------------------------
TEST_CASE( "Consecutive websocket send/receive", "[Transport][Websocket]" )
{
    {
        LoopbackFixture f{true, msgpackId, {msgpackId}};
        checkConsecutiveSendReceive(f, f.client, f.server);
    }
    {
        LoopbackFixture f{true, msgpackId, {msgpackId}};
        checkConsecutiveSendReceive(f, f.server, f.client);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "Maximum length websocket messages", "[Transport][Websocket]" )
{
    LoopbackFixture f;
    const MessageBuffer message(f.client->info().receiveLimit(), 'm');
    const MessageBuffer reply(f.server->info().receiveLimit(), 'r');;
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
TEST_CASE( "Cancel websocket connect", "[Transport][Websocket]" )
{
    bool listenCompleted = false;
    std::error_code listenEc;
    LoopbackFixture f(false);
    f.lstn->observe([&](ListenResult result)
    {
        if (result.ok())
        {
            f.server = result.transport();
            f.server->admit(
                [&](AdmitResult result)
                {
                    listenCompleted = true;
                    listenEc = result.error();
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
        CHECK_FALSE( f.server );
    }
    else if (connectCompleted)
        CHECK( f.client );
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
TEST_CASE( "Cancel websocket receive", "[Transport][Websocket]" )
{
    LoopbackFixture f;
    std::error_code clientError;
    f.client->start(
        [&](ErrorOr<MessageBuffer> buf)
        {
            if (!buf.has_value())
                clientError = buf.error();
        },
        nullptr);

    std::error_code serverError;
    f.server->start(
        [&](ErrorOr<MessageBuffer> buf)
        {
            if (!buf.has_value())
                serverError = buf.error();
        },
        nullptr);

    f.cctx.poll();
    f.cctx.reset();

    // Close the transport while the receive operation is in progress,
    // and check the client handler emits an 'aborted' error.
    f.client->close();
    REQUIRE_NOTHROW( f.run() );
    CHECK( clientError == TransportErrc::aborted );
    CHECK( serverError == TransportErrc::disconnected );
}

//------------------------------------------------------------------------------
TEST_CASE( "Cancel websocket send", "[Transport][Websocket]" )
{
    // The size of transmission is set large to increase the likelyhood
    // of the operation being aborted, rather than completed.
    LoopbackFixture f(false, jsonId, {jsonId}, 16*1024*1024, 16*1024*1024);
    f.lstn->observe([&](ListenResult result)
    {
        REQUIRE(result.ok());
        f.server = result.transport();
        f.server->admit(
            [](AdmitResult r) {REQUIRE(r.status() == AdmitStatus::wamp);});
    });
    f.lstn->establish();
    f.cnct->establish([&](ErrorOr<Transporting::Ptr> transport)
    {
        REQUIRE(transport.has_value());
        f.client = *transport;
    });
    f.run();
    f.server->start(
        [&](ErrorOr<MessageBuffer> buf) {},
        nullptr);

    // Start a send operation
    std::error_code clientRxError;
    std::error_code clientTxError;
    f.client->start(
        [&](ErrorOr<MessageBuffer> buf)
        {
            if (!buf.has_value())
                clientRxError = buf.error();
        },
        [&](std::error_code ec) {clientTxError = ec;});
    MessageBuffer message(f.server->info().receiveLimit(), 'a');
    f.client->send(message);
    REQUIRE_NOTHROW( f.cctx.poll() );
    f.cctx.reset();

    // Close the transport and check that either of the client handlers emit an
    // 'aborted' error.
    f.client->close();
    f.run();
    if (clientRxError)
        CHECK( clientRxError == TransportErrc::aborted );
    else
        CHECK( clientTxError == TransportErrc::aborted );
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

    SECTION("Client sending overly long message")
    {
        client->send(std::move(tooLong));

        CHECK_NOTHROW( f.run() );
        UNSCOPED_INFO("client error message:" << clientError.message());
        UNSCOPED_INFO("server error message:" << serverError.message());
        CHECK( clientError == TransportErrc::outboundTooLong );
        CHECK( serverError == TransportErrc::inboundTooLong );
    }

    SECTION("Server sending overly long message")
    {
        server->send(std::move(tooLong));

        CHECK_NOTHROW( f.run() );
        UNSCOPED_INFO("client error message:" << clientError.message());
        UNSCOPED_INFO("server error message:" << serverError.message());
        CHECK( clientError == TransportErrc::inboundTooLong );
        CHECK( serverError == TransportErrc::outboundTooLong );
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "Websocket server transport handshake timeout",
           "[Transport][Websocket]" )
{
    IoContext ioctx;
    auto exec = ioctx.get_executor();
    auto strand = boost::asio::make_strand(exec);
    std::error_code serverError;

    auto limits = WebsocketServerLimits{}
                      .withHandshakeTimeout(std::chrono::milliseconds(50));
    auto lstn = std::make_shared<WebsocketListener>(
        exec, strand, wsEndpoint.withLimits(limits), CodecIdSet{jsonId});
    Transporting::Ptr server;
    lstn->observe(
        [&](ListenResult result)
        {
            REQUIRE( result.ok() );
            server = result.transport();
            server->admit(
                [&serverError](AdmitResult r) {serverError = r.error();});
        });
    lstn->establish();

    using boost::asio::ip::tcp;

    test::SilentClient client{ioctx};
    client.run(wsEndpoint.port());

    ioctx.run();
    CHECK(client.readError() == boost::asio::error::eof);
    CHECK(serverError == TransportErrc::timeout);
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

#if defined(CPPWAMP_TEST_HAS_CORO)

//------------------------------------------------------------------------------
TEST_CASE( "WAMP session using Websocket transport",
           "[WAMP][Basic][Websocket]" )
{
    IoContext ioctx;
    Session s(ioctx);
    const auto wish = WebsocketHost{tcpLoopbackAddr, 34567}.withFormat(cbor);

    Invocation invocation;
    Event event;

    auto rpc =
        [&invocation](Invocation i) -> Outcome
        {
            invocation = i;
            return Result{i.args().at(0)};
        };

    auto onEvent = [&event](Event e) {event = e;};

    spawn(ioctx, [&](YieldContext yield)
    {
        s.connect(wish, yield).value();
        s.join(Petition{"cppwamp.test"}, yield).value();
        auto reg = s.enroll(Procedure{"rpc"}, rpc, yield).value();
        auto sub = s.subscribe(Topic{"topic"}, onEvent, yield).value();

        auto result = s.call(Rpc{"rpc"}.withArgs(42), yield).value();
        REQUIRE(result.args().size() == 1);
        CHECK(result.args().front() == 42);
        REQUIRE(invocation.args().size() == 1);
        CHECK(invocation.args().front() == 42);

        s.publish(Pub{"topic"}.withArgs("foo").withExcludeMe(false),
                  yield).value();
        while (event.args().empty())
            suspendCoro(yield);
        REQUIRE(event.args().size() == 1);
        CHECK(event.args().front() == "foo");

        s.unregister(reg, yield).value();
        s.unsubscribe(sub, yield).value();

        s.leave(yield).value();
        bool disconnected = s.disconnect(yield).value();
        CHECK(disconnected);

        s.connect(wish, yield).value();
        s.join(Petition{"cppwamp.test"}, yield).value();
        s.disconnect();
    });

    ioctx.run();
}

//------------------------------------------------------------------------------
TEST_CASE( "Router websocket connection limit option", "[WAMP][Router]" )
{
    if (!test::RouterFixture::enabled())
        return;

    struct ServerCloseGuard
    {
        std::string name;

        ~ServerCloseGuard()
        {
            test::RouterFixture::instance().router().closeServer(name);
        }
    };

    auto& routerFixture = test::RouterFixture::instance();
    auto& router = routerFixture.router();
    ServerCloseGuard serverGuard{"ws45678"};
    router.openServer(ServerOptions("ws45678", wamp::WebsocketEndpoint{45678},
                                    wamp::cbor).withConnectionLimit(2));

    IoContext ioctx;
    std::vector<LogEntry> logEntries;
    auto logSnoopGuard = routerFixture.snoopLog(
        ioctx.get_executor(),
        [&logEntries](LogEntry e) {logEntries.push_back(e);});
    auto logLevelGuard = routerFixture.supressLogLevel(LogLevel::critical);
    boost::asio::steady_timer timer{ioctx};
    Session s1{ioctx};
    Session s2{ioctx};
    Session s3{ioctx};
    auto where = WebsocketHost{"localhost", 45678}.withFormat(cbor);

    spawn(ioctx, [&](YieldContext yield)
    {
        timer.expires_after(std::chrono::milliseconds(100));
        timer.async_wait(yield);
        s1.connect(where, yield).value();
        s2.connect(where, yield).value();
        auto w = s3.connect(where, yield);
        REQUIRE_FALSE(w.has_value());
        CHECK(w.error() == HttpStatus::serviceUnavailable);
        s3.disconnect();
        while (logEntries.empty())
            test::suspendCoro(yield);
        CHECK(logEntries.front().message().find("connection limit"));

        s2.disconnect();
        timer.expires_after(std::chrono::milliseconds(50));
        timer.async_wait(yield);
        w = s3.connect(where, yield);
        CHECK(w.has_value());
        s1.disconnect();
    });
    ioctx.run();
}

#endif // defined(CPPWAMP_TEST_HAS_CORO)

#endif // defined(CPPWAMP_TEST_HAS_WEB)
