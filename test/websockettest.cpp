/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#if defined(CPPWAMP_TEST_HAS_WEB)

#include <catch2/catch.hpp>
#include <cppwamp/asiodefs.hpp>
#include <cppwamp/codecs/cbor.hpp>
#include <cppwamp/internal/websocketconnector.hpp>
#include <cppwamp/internal/websocketlistener.hpp>
#include <cppwamp/transports/websocketclient.hpp>
#include <cppwamp/transports/websocketserver.hpp>

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
                WebsocketClientLimits{}.withWampReadMsgSize(clientLimit)),
            clientCodec,
            wsEndpoint.withLimits(
                WebsocketServerLimits{}.withWampReadMsgSize(serverLimit)),
            serverCodecs,
            connected)
    {}

    void connect()
    {
        lstn->observe(
            [&](ListenResult result)
            {
                if (!result.ok())
                {
                    FAIL("LoopbackFixture ListenResult::error: "
                         << result.error());
                }
                auto transport = lstn->take();
                server = std::move(transport);
                server->admit(
                    [this](AdmitResult result)
                    {
                        auto s = result.status();
                        if (s != AdmitStatus::responded &&
                            s != AdmitStatus::wamp)
                        {
                            FAIL("LoopbackFixture AdmitResult::error: "
                                 << result.error());
                        }
                        if (result.status() == AdmitStatus::wamp)
                            serverCodec = result.codecId();
                    });
            });
        lstn->establish();

        cnct->establish(
            [&](ErrorOr<Transporting::Ptr> transportOrError)
            {
                if (!transportOrError)
                {
                    FAIL("LoopbackFixture ListenResult::error: "
                         << transportOrError.error());
                }
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
class MockWebsocketClient
    : public std::enable_shared_from_this<MockWebsocketClient>
{
public:
    using Ptr = std::shared_ptr<MockWebsocketClient>;

    template <typename E>
    static Ptr create(E&& exec, uint16_t port, std::string request)
    {
        return Ptr(new MockWebsocketClient(std::forward<E>(exec), port,
                                           std::move(request)));
    }

    void connect()
    {
        auto self = shared_from_this();
        resolver_.async_resolve(
            "localhost",
            std::to_string(port_),
            [this, self](boost::system::error_code ec,
                         Resolver::results_type endpoints)
            {
                if (check(ec))
                    onResolved(endpoints);
            });
    }

    void close()
    {
        socket_.close();
        connectCompleted_ = false;
    }

    bool connectCompleted() const {return connectCompleted_;}

    const std::string& response() const {return response_;}

    boost::system::error_code readError() const {return readError_;}

private:
    using Resolver = boost::asio::ip::tcp::resolver;
    using Socket = boost::asio::ip::tcp::socket;

    template <typename E>
    MockWebsocketClient(E&& exec, uint16_t port, std::string request)
        : resolver_(boost::asio::make_strand(exec)),
          socket_(resolver_.get_executor()),
          request_(std::move(request)),
          port_(port)
    {}

    bool check(boost::system::error_code ec, bool reading = true)
    {
        if (!ec)
            return true;

        connectCompleted_ = true;

        if (reading)
            readError_ = ec;

        namespace error = boost::asio::error;
        if (ec == error::eof ||
            ec == error::operation_aborted ||
            ec == error::connection_reset )
        {
            return false;
        }

        throw std::system_error{ec};
        return false;
    }

    void onResolved(const Resolver::results_type& endpoints)
    {
        auto self = shared_from_this();
        boost::asio::async_connect(
            socket_,
            endpoints,
            [this, self](boost::system::error_code ec, Socket::endpoint_type)
            {
                if (check(ec))
                    onConnected();
            });
    }

    void onConnected()
    {
        auto self = shared_from_this();
        boost::asio::async_write(
            socket_,
            boost::asio::const_buffer(&request_.front(), request_.size()),
            [this, self](boost::system::error_code ec, std::size_t)
            {
                if (check(ec))
                    onRequestWritten();
            });
    }

    void flush()
    {
        buffer_.clear();
        auto self = shared_from_this();

        boost::asio::async_read(
            socket_,
            boost::asio::dynamic_buffer(buffer_),
            [this, self](boost::system::error_code ec, std::size_t)
            {
                if (check(ec))
                    return flush();
                socket_.close();
                connectCompleted_ = true;
            });
    }

    void onRequestWritten()
    {
        response_.clear();
        auto self = shared_from_this();

        boost::asio::async_read(
            socket_,
            boost::asio::dynamic_buffer(response_),
            [this, self](boost::system::error_code ec, std::size_t)
            {
                if (check(ec))
                    flush();
            });
    }

    Resolver resolver_;
    Socket socket_;
    std::string request_;
    std::string response_;
    std::string buffer_;
    boost::system::error_code readError_;
    uint16_t port_ = 0;
    bool connectCompleted_;
};

//------------------------------------------------------------------------------
struct MalformedWebsocketUpgradeTestVector
{
    template <typename TErrc>
    MalformedWebsocketUpgradeTestVector(std::string info, TErrc errc,
                                        unsigned status, std::string request)
        : info(std::move(info)),
          request(std::move(request)),
          expectedError(make_error_code(errc)),
          expectedStatus(status)
    {}

    void run() const
    {
        INFO("Test case: " << info);

        IoContext ioctx;
        auto exec = ioctx.get_executor();
        auto strand = boost::asio::make_strand(exec);

        Transporting::Ptr server;
        auto lstn = std::make_shared<WebsocketListener>(
            exec, strand, wsEndpoint, CodecIdSet{jsonId});
        AdmitResult admitResult;
        lstn->observe(
            [&](ListenResult result)
            {
                REQUIRE( result.ok() );
                server = lstn->take();
                server->admit(
                    [&](AdmitResult r)
                    {
                        admitResult = r;
                        server->close();
                    } );
            });
        lstn->establish();

        auto client = MockWebsocketClient::create(ioctx, tcpTestPort, request);
        client->connect();

        ioctx.run();

        INFO("Response:\n" << client->response());

        CHECK( admitResult.status() == AdmitStatus::rejected );
        CHECK( admitResult.error() == expectedError );
        CHECK( client->connectCompleted() );

        auto status = std::to_string(expectedStatus);
        bool statusFound = client->response().find(status) != std::string::npos;
        CHECK( statusFound );
    }

    std::string info;
    std::string request;
    std::error_code expectedError;
    unsigned expectedStatus;
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
        auto transport = f.lstn->take();
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
                       WebsocketServerLimits{}.wampWriteMsgSize() );
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
               WebsocketClientLimits{}.wampWriteMsgSize() );
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
        f.server = f.lstn->take();
        f.server->admit(
            [&](AdmitResult result)
            {
                serverEc = result.error();
                f.server->close();
            });
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
            auto transport = f.lstn->take();
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
                           WebsocketServerLimits{}.wampWriteMsgSize() );
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
                   WebsocketClientLimits{}.wampWriteMsgSize() );
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
TEST_CASE( "Websocket shedding", "[Transport][Websocket]" )
{
    IoContext ioctx;
    auto exec = ioctx.get_executor();
    auto strand = boost::asio::make_strand(exec);

    Transporting::Ptr server;
    auto lstn = std::make_shared<WebsocketListener>(exec, strand, wsEndpoint,
                                                    CodecIdSet{jsonId});
    AdmitResult admitResult;
    lstn->observe(
        [&](ListenResult result)
        {
            REQUIRE( result.ok() );
            server = lstn->take();
            server->shed(
                [&](AdmitResult r)
                {
                    admitResult = r;
                    server->close();
                } );
        });
    lstn->establish();

    auto cnct = std::make_shared<WebsocketConnector>(strand, wsHost, jsonId);
    Transporting::Ptr client;
    std::error_code clientError;
    cnct->establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            if (!transport.has_value())
                clientError = transport.error();
        });

    ioctx.run();
    CHECK( admitResult.status() == AdmitStatus::shedded );
    CHECK( clientError == HttpStatus::serviceUnavailable );
}

//------------------------------------------------------------------------------
TEST_CASE( "Websocket client aborting", "[Transport][Websocket]" )
{
    LoopbackFixture f;
    auto abortMessage = makeMessageBuffer("abort");
    std::error_code clientError;
    std::error_code serverError;
    std::error_code abortError;
    bool abortHandlerInvoked = false;

    f.client->start(
        [&](ErrorOr<MessageBuffer> buf)
        {
            if (buf.has_value())
            {
                f.client->abort(
                    abortMessage,
                    make_error_code(WampErrc::authenticationDenied),
                    [&](std::error_code ec)
                    {
                        abortHandlerInvoked = true;
                        abortError = ec;
                    });
            }
            else
            {
                clientError = buf.error();
                f.client->close();
            }
        },
        nullptr);

    MessageBuffer rxMessage;
    f.server->start(
        [&](ErrorOr<MessageBuffer> buf)
        {
            if (buf.has_value())
            {
                rxMessage = buf.value();
            }
            else
            {
                serverError = buf.error();
                f.server->close();
            }
        },
        nullptr);

    f.server->send(makeMessageBuffer("Hello"));

    REQUIRE_NOTHROW( f.run() );

    CHECK( serverError == TransportErrc::ended );
    CHECK_FALSE( abortError );
    CHECK( rxMessage == abortMessage );
    CHECK( abortHandlerInvoked );
    CHECK_FALSE( abortError );
    if (clientError)
        CHECK( clientError == TransportErrc::aborted );}

//------------------------------------------------------------------------------
TEST_CASE( "Websocket server aborting", "[Transport][Websocket]" )
{
    LoopbackFixture f;
    auto abortMessage = makeMessageBuffer("abort");
    std::error_code clientError;
    std::error_code serverError;
    std::error_code abortError;
    bool abortHandlerInvoked = false;

    MessageBuffer rxMessage;
    f.client->start(
        [&](ErrorOr<MessageBuffer> buf)
        {
            if (buf.has_value())
            {
                rxMessage = buf.value();
            }
            else
            {
                clientError = buf.error();
                f.client->close();
            }
        },
        nullptr);

    f.server->start(
        [&](ErrorOr<MessageBuffer> buf)
        {
            if (buf.has_value())
            {
                f.server->abort(
                    abortMessage,
                    make_error_code(WampErrc::authenticationDenied),
                    [&](std::error_code ec)
                    {
                        abortHandlerInvoked = true;
                        abortError = ec;
                    });
            }
            else
            {
                serverError = buf.error();
                f.server->close();
            }
        },
        nullptr);

    f.client->send(makeMessageBuffer("Hello"));

    REQUIRE_NOTHROW( f.run() );

    CHECK( clientError == TransportErrc::ended );
    CHECK_FALSE( abortError );
    CHECK( rxMessage == abortMessage );
    CHECK( abortHandlerInvoked );
    CHECK_FALSE( abortError );
    if (serverError)
        CHECK( serverError == TransportErrc::aborted );}

//------------------------------------------------------------------------------
TEST_CASE( "Graceful Websocket shutdown", "[Transport][Websocket]" )
{
    LoopbackFixture f;
    std::error_code clientError;
    std::error_code serverError;
    std::error_code shutdownError;
    bool shutdownHandlerInvoked = false;

    f.client->start(
        [&](ErrorOr<MessageBuffer> buf)
        {
            if (buf.has_value())
            {
                f.client->shutdown(
                    {},
                    [&](std::error_code ec)
                    {
                        shutdownHandlerInvoked = true;
                        shutdownError = ec;
                    });
            }
            else
            {
                clientError = buf.error();
                f.client->close();
            }
        },
        nullptr);

    f.server->start(
        [&](ErrorOr<MessageBuffer> buf)
        {
            if (!buf.has_value())
            {
                serverError = buf.error();
                f.server->close();
            }
        },
        nullptr);

    f.server->send(makeMessageBuffer("Hello"));

    REQUIRE_NOTHROW( f.run() );

    CHECK( serverError == TransportErrc::ended );
    CHECK( shutdownHandlerInvoked );
    CHECK_FALSE( shutdownError );
    if (clientError)
        CHECK( clientError == TransportErrc::aborted );}

//------------------------------------------------------------------------------
TEST_CASE( "Websocket shutdown during send", "[Transport][Websocket]" )
{
    constexpr unsigned bigLength = 16*1024*1024;
    LoopbackFixture f(true, jsonId, {jsonId}, bigLength, bigLength);
    MessageBuffer bigMessage(bigLength, 'A');
    std::error_code clientError;
    std::error_code serverRxError;
    std::error_code serverTxError;
    std::error_code shutdownError;
    unsigned messageCount = 0;
    bool shutdownHandlerInvoked = false;

    f.client->start(
        [&](ErrorOr<MessageBuffer> buf)
        {
            if (buf.has_value())
            {
                ++messageCount;
                f.client->shutdown(
                    {},
                    [&](std::error_code ec)
                    {
                        shutdownHandlerInvoked = true;
                        shutdownError = ec;
                    });
            }
            else
            {
                clientError = buf.error();
                f.client->close();
            }
        },
        nullptr);

    f.server->start(
        [&](ErrorOr<MessageBuffer> buf)
        {
            if (!buf.has_value())
            {
                serverRxError = buf.error();
                f.server->close();
            }
        },
        [&](std::error_code ec) {serverTxError = ec;});

    f.server->send(makeMessageBuffer("Hello"));
    f.server->send(bigMessage);

    f.run();

    CHECK( messageCount == 1 );
    CHECK( serverRxError == TransportErrc::ended );
    CHECK( shutdownHandlerInvoked );
    CHECK_FALSE( serverTxError );
    CHECK_FALSE( shutdownError );
    if (clientError)
        CHECK( clientError == TransportErrc::aborted );
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
            f.server = f.lstn->take();
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
        f.server = f.lstn->take();
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
TEST_CASE( "Invalid Websocket request-target", "[Transport][Websocket][thisone]" )
{
    auto host = wsHost;
    host.withTarget("/foo^bar");
    LoopbackFixture f(host, jsonId, wsEndpoint, {jsonId}, false);
    std::error_code serverEc;
    std::error_code clientEc;

    f.lstn->observe(
        [&](ListenResult result)
        {
            REQUIRE( result.ok() );
            f.server = f.lstn->take();
            f.server->admit(
                [&](AdmitResult result)
                {
                    serverEc = result.error();
                    f.server->close();
                });
        });
    f.lstn->establish();

    f.cnct->establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            if (!transport.has_value())
                clientEc = transport.error();
        });

    f.run();
    CHECK( serverEc == TransportErrc::badHandshake );
    CHECK( clientEc == HttpStatus::badRequest );
}

//------------------------------------------------------------------------------
TEST_CASE( "Peer sending a websocket message longer than maximum",
           "[Transport][Websocket][thisone]" )
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
            client->close();
        },
        nullptr);

    server->start(
        [&](ErrorOr<MessageBuffer> message)
        {
            REQUIRE( !message );
            serverError = message.error();
            server->close();
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

//------------------------------------------------------------------------------
TEST_CASE( "Malformed Websocket Upgrade Request", "[Transport][Websocket]" )
{
    using HE = boost::beast::http::error;
    using WE = boost::beast::websocket::error;
    using TE = TransportErrc;

    std::vector<MalformedWebsocketUpgradeTestVector> testVectors =
    {
        {
            "Random garbage", HE::bad_method, 400,
            "a8gpsn3-g=bdsao;fdbgvmvii9fs\r\n"
            "\r\n"
        },
        {
            "Non-existent method", WE::bad_method, 400,
            "BAD / HTTP/1.1\r\n"
            "Host: localhost:9090\r\n"
            "Connection: Upgrade\r\n"
            "Upgrade: websocket\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "Sec-WebSocket-Key: QUFBQUFBQUFBQUFBQUFBQQ==\r\n"
            "Sec-WebSocket-Protocol: wamp.2.json\r\n"
            "\r\n"
        },
        {
            "Invalid method", WE::bad_method, 400,
            "POST / HTTP/1.1\r\n"
            "Host: localhost:9090\r\n"
            "Connection: Upgrade\r\n"
            "Upgrade: websocket\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "Sec-WebSocket-Key: QUFBQUFBQUFBQUFBQUFBQQ==\r\n"
            "Sec-WebSocket-Protocol: wamp.2.json\r\n"
            "Content-Length: 0\r\n"
            "\r\n"
        },
        {
            "Bad HTTP version label", HE::bad_version, 400,
            "GET / BOGUS/1.1\r\n"
            "Host: localhost:9090\r\n"
            "Connection: Upgrade\r\n"
            "Upgrade: bogus\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "Sec-WebSocket-Key: QUFBQUFBQUFBQUFBQUFBQQ==\r\n"
            "Sec-WebSocket-Protocol: wamp.2.json\r\n"
            "\r\n"
        },
        {
            "Bad HTTP version number", WE::bad_http_version, 400,
            "GET / HTTP/1.0\r\n"
            "Host: localhost:9090\r\n"
            "Connection: Upgrade\r\n"
            "Upgrade: bogus\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "Sec-WebSocket-Key: QUFBQUFBQUFBQUFBQUFBQQ==\r\n"
            "Sec-WebSocket-Protocol: wamp.2.json\r\n"
            "\r\n"
        },
        {
            "No Host field", WE::no_host, 400,
            "GET / HTTP/1.1\r\n"
            "Connection: Upgrade\r\n"
            "Upgrade: websocket\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "Sec-WebSocket-Key: QUFBQUFBQUFBQUFBQUFBQQ==\r\n"
            "Sec-WebSocket-Protocol: wamp.2.json\r\n"
            "\r\n"
        },
        {
            "No Connection field", WE::no_connection_upgrade, 426,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:9090\r\n"
            "Upgrade: bogus\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "Sec-WebSocket-Key: QUFBQUFBQUFBQUFBQUFBQQ==\r\n"
            "Sec-WebSocket-Protocol: wamp.2.json\r\n"
            "\r\n"
        },
        {
            "Bad Connection field", WE::no_connection_upgrade, 426,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:9090\r\n"
            "Connection: keep-alive\r\n"
            "Upgrade: bogus\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "Sec-WebSocket-Key: QUFBQUFBQUFBQUFBQUFBQQ==\r\n"
            "Sec-WebSocket-Protocol: wamp.2.json\r\n"
            "\r\n"
        },
        {
            "No Upgrade field", WE::no_upgrade, 400,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:9090\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "Sec-WebSocket-Key: QUFBQUFBQUFBQUFBQUFBQQ==\r\n"
            "Sec-WebSocket-Protocol: wamp.2.json\r\n"
            "\r\n"
        },
        {
            "Bad Upgrade field", WE::no_upgrade_websocket, 400,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:9090\r\n"
            "Connection: Upgrade\r\n"
            "Upgrade: HTTP/2\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "Sec-WebSocket-Key: QUFBQUFBQUFBQUFBQUFBQQ==\r\n"
            "Sec-WebSocket-Protocol: wamp.2.json\r\n"
            "\r\n"
        },
        {
            "No Sec-WebSocket-Version", WE::no_sec_version, 400,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:9090\r\n"
            "Connection: Upgrade\r\n"
            "Upgrade: websocket\r\n"
            "Sec-WebSocket-Key: QUFBQUFBQUFBQUFBQUFBQQ==\r\n"
            "Sec-WebSocket-Protocol: wamp.2.json\r\n"
            "\r\n"
        },
        {
            "Bad Sec-WebSocket-Version", WE::bad_sec_version, 400,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:9090\r\n"
            "Connection: Upgrade\r\n"
            "Upgrade: websocket\r\n"
            "Sec-WebSocket-Version: bogus\r\n"
            "Sec-WebSocket-Key: QUFBQUFBQUFBQUFBQUFBQQ==\r\n"
            "Sec-WebSocket-Protocol: wamp.2.json\r\n"
            "\r\n"
        },
        {
            "Obsolete Sec-WebSocket-Version", WE::bad_sec_version, 400,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:9090\r\n"
            "Connection: Upgrade\r\n"
            "Upgrade: websocket\r\n"
            "Sec-WebSocket-Version: 12\r\n"
            "Sec-WebSocket-Key: QUFBQUFBQUFBQUFBQUFBQQ==\r\n"
            "Sec-WebSocket-Protocol: wamp.2.json\r\n"
            "\r\n"
        },
        {
            "No Sec-WebSocket-Key", WE::no_sec_key, 400,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:9090\r\n"
            "Connection: Upgrade\r\n"
            "Upgrade: websocket\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "Sec-WebSocket-Protocol: wamp.2.json\r\n"
            "\r\n"
        },
        {
            "Bad Sec-WebSocket-Key", WE::bad_sec_key, 400,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:9090\r\n"
            "Connection: Upgrade\r\n"
            "Upgrade: websocket\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQdGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Protocol: wamp.2.json\r\n"
            "\r\n"
        },
        {
            "No Sec-WebSocket-Protocol", TE::noSerializer, 400,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:9090\r\n"
            "Connection: Upgrade\r\n"
            "Upgrade: websocket\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "Sec-WebSocket-Key: QUFBQUFBQUFBQUFBQUFBQQ==\r\n"
            "\r\n"
        },
        {
            "Bad Sec-WebSocket-Protocol", TE::badSerializer, 400,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:9090\r\n"
            "Connection: Upgrade\r\n"
            "Upgrade: websocket\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "Sec-WebSocket-Key: QUFBQUFBQUFBQUFBQUFBQQ==\r\n"
            "Sec-WebSocket-Protocol: bogus\r\n"
            "\r\n"
        },
    };

    for (const auto& vec: testVectors)
        vec.run();
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
        s.join("cppwamp.test", yield).value();
        auto reg = s.enroll("rpc", rpc, yield).value();
        auto sub = s.subscribe("topic", onEvent, yield).value();

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
        s.join("cppwamp.test", yield).value();
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
                                    wamp::cbor).withSoftConnectionLimit(2));

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
        s3.disconnect();
    });
    ioctx.run();
}

#endif // defined(CPPWAMP_TEST_HAS_CORO)

#endif // defined(CPPWAMP_TEST_HAS_WEB)
