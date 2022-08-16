/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTTEST_HPP
#define CPPWAMP_TRANSPORTTEST_HPP

#include <vector>
#include <catch2/catch.hpp>
#include <cppwamp/asiodefs.hpp>
#include <cppwamp/codec.hpp>
#include <cppwamp/error.hpp>
#include <cppwamp/rawsockoptions.hpp>
#include <cppwamp/transport.hpp>

using namespace wamp;

namespace
{

//------------------------------------------------------------------------------
constexpr unsigned short tcpTestPort = 9090;
constexpr const char tcpLoopbackAddr[] = "127.0.0.1";
constexpr const char udsTestPath[] = "cppwamptestuds";

//------------------------------------------------------------------------------
struct LoopbackFixtureBase
{
    // These members need to be already initialized when initializing
    // TcpLoopbackFixture and UdsLoopbackFixture
    AsioContext clientCtx;
    AsioContext serverCtx;
};

//------------------------------------------------------------------------------
template <typename TConnector, typename TListener>
struct LoopbackFixture
{
    using Connector    = TConnector;
    using Listener     = TListener;
    using Opener       = typename Connector::Establisher;
    using Acceptor     = typename Listener::Establisher;
    using Transport    = typename Connector::Transport;
    using TransportPtr = typename Transporting::Ptr;

    template <typename TServerCodecIds>
    LoopbackFixture(AsioContext& clientCtx,
                    AsioContext& serverCtx,
                    Opener&& opener,
                    int clientCodec,
                    RawsockMaxLength clientMaxRxLength,
                    Acceptor&& acceptor,
                    TServerCodecIds&& serverCodecs,
                    RawsockMaxLength serverMaxRxLength,
                    bool connected = true)
        : cctx(clientCtx),
          sctx(serverCtx),
          cnct(std::move(opener), clientCodec, clientMaxRxLength),
          lstn(std::move(acceptor), std::forward<TServerCodecIds>(serverCodecs),
               serverMaxRxLength)
    {
        if (connected)
            connect();
    }

    void connect()
    {
        lstn.establish(
            [&](ErrorOr<TransportPtr> transportOrError)
            {
                auto transport = transportOrError.value();
                serverCodec = transport->info().codecId;
                server = std::move(transport);
            });

        cnct.establish(
            [&](ErrorOr<TransportPtr> transportOrError)
            {
                auto transport = transportOrError.value();
                clientCodec =transport->info().codecId;
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
            sctx.poll();
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

    AsioContext& cctx;
    AsioContext& sctx;
    Connector cnct;
    Listener  lstn;
    int clientCodec;
    int serverCodec;
    TransportPtr client;
    TransportPtr server;
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
    using TransportPtr = typename Transporting::Ptr;
    f.lstn.establish([&](ErrorOr<TransportPtr> transportOrError)
    {
        REQUIRE( transportOrError.has_value() );
        auto transport = *transportOrError;
        REQUIRE( transport );
        CHECK( transport->info().codecId == expectedCodec );
        CHECK( transport->info().maxRxLength == serverMaxRxLength );
        CHECK( transport->info().maxTxLength == clientMaxRxLength );
        f.server = transport;
    });

    f.cnct.establish([&](ErrorOr<TransportPtr> transportOrError)
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
                    typename TFixture::TransportPtr sender,
                    typename TFixture::TransportPtr receiver,
                    const MessageBuffer& message,
                    const MessageBuffer& reply)
{
    bool receivedMessage = false;
    bool receivedReply = false;
    receiver->start(
        [&](MessageBuffer buf)
        {
            receivedMessage = true;
            CHECK( message == buf );
            receiver->send(reply);
        },
        [&](std::error_code ec)
        {
            CHECK( ec == TransportErrc::aborted );
        });

    sender->start(
        [&](MessageBuffer buf)
        {
            receivedReply = true;
            CHECK( reply == buf );
            f.disconnect();
        },
        [&](std::error_code ec)
        {
            CHECK( ec == TransportErrc::aborted );
        });

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
void checkCommunications(TFixture& f)
{
    using TransportPtr = typename TFixture::TransportPtr;

    TransportPtr sender = f.client;
    TransportPtr receiver = f.server;
    auto message = makeMessageBuffer("Hello");
    auto reply = makeMessageBuffer("World");
    bool receivedMessage = false;
    bool receivedReply = false;

    receiver->start(
        [&](MessageBuffer buf)
        {
            receivedMessage = true;
            CHECK( message == buf );
            receiver->send(reply);
        },
        [&](std::error_code ec)
        {
            CHECK( ec == TransportErrc::aborted );
        });

    sender->start(
        [&](MessageBuffer buf)
        {
            receivedReply = true;
            CHECK( reply == buf );
        },
        [&](std::error_code ec)
        {
            CHECK( ec == TransportErrc::aborted );
        });

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
    TransportPtr server2;
    TransportPtr client2;
    auto message2 = makeMessageBuffer("Hola");
    auto reply2 = makeMessageBuffer("Mundo");
    bool receivedMessage2 = false;
    bool receivedReply2 = false;
    message = makeMessageBuffer("Bonjour");
    reply = makeMessageBuffer("Le Monde");
    receivedMessage = false;
    receivedReply = false;

    f.lstn.establish(
        [&](ErrorOr<TransportPtr> transportOrError)
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

    f.cnct.establish(
        [&](ErrorOr<TransportPtr> transportOrError)
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
        [&](MessageBuffer buf)
        {
            receivedMessage2 = true;
            CHECK( message2 == buf );
            receiver2->send(reply2);
        },
        [&](std::error_code ec)
        {
            CHECK( ec == TransportErrc::aborted );
        });

    sender2->start(
        [&](MessageBuffer buf)
        {
            receivedReply2 = true;
            CHECK( reply2 == buf );
            sender2->close();
            receiver2->close();
        },
        [&](std::error_code ec)
        {
            CHECK( ec == TransportErrc::aborted );
        });

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
    REQUIRE_NOTHROW( f.run() );
}

//------------------------------------------------------------------------------
template <typename TFixture>
void checkConsecutiveSendReceive(
        TFixture& f,
        typename TFixture::TransportPtr& sender,
        typename TFixture::TransportPtr& receiver)
{
    std::vector<MessageBuffer> messages;
    for (int i=0; i<100; ++i)
        messages.emplace_back(i, 'A' + i);

    sender->start(
        [&](MessageBuffer)
        {
            FAIL( "Unexpected receive" );
        },
        [&](std::error_code ec)
        {
            CHECK( ec == TransportErrc::aborted );
        });

    size_t count = 0;

    receiver->start(
        [&](MessageBuffer buf)
        {
            REQUIRE( messages.at(count) == buf );
            if (++count == messages.size())
            {
                f.disconnect();
            }
        },
        [&](std::error_code ec)
        {
            CHECK( ec == TransportErrc::aborted );
        });

    for (const auto& msg: messages)
        sender->send(msg);

    CHECK_NOTHROW( f.run() );
}

//------------------------------------------------------------------------------
template <typename TFixture>
void checkCancelListen(TFixture& f)
{
    using TransportPtr = typename TFixture::TransportPtr;
    f.lstn.establish([&](ErrorOr<TransportPtr> transport)
    {
        CHECK( transport == makeUnexpectedError(TransportErrc::aborted) );
        CHECK_FALSE( transport );
    });
    f.lstn.cancel();
    CHECK_NOTHROW( f.run() );
}

//------------------------------------------------------------------------------
template <typename TFixture>
void checkCancelConnect(TFixture& f)
{
    using TransportPtr = typename TFixture::TransportPtr;
    WHEN( "the client cancels before the connection is established" )
    {
        f.lstn.establish([&](ErrorOr<TransportPtr> transport)
        {
            if (transport.has_value())
                f.server = *transport;
        });

        bool connectCanceled = false;
        bool connectCompleted = false;
        f.cnct.establish(
            [&](ErrorOr<TransportPtr> transport)
            {
                if (transport.has_value())
                {
                    connectCompleted = true;
                    f.client = *transport;
                }
                else
                {
                    CHECK( transport ==
                           makeUnexpectedError(TransportErrc::aborted) );
                    connectCanceled = true;
                }
                f.stop();
            });
        f.cctx.poll();
        f.cctx.reset();

        f.cnct.cancel();
        f.run();

        THEN( "the operation either aborts or completes" )
        {
            REQUIRE( (connectCanceled || connectCompleted) );
            if (connectCanceled)
            {
                CHECK_FALSE( f.client );
                CHECK_FALSE( f.server );
            }
            else if (connectCompleted)
                CHECK( f.client );
        }
    }
}

//------------------------------------------------------------------------------
template <typename TFixture>
void checkCancelReceive(TFixture& f)
{
    WHEN( "a receive operation is in progress" )
    {
        bool transportFailed = false;
        f.client->start(
            [&](MessageBuffer)
            {
                FAIL( "Unexpected receive" );
            },
            [&](std::error_code ec)
            {
                transportFailed = true;
                CHECK( ec == TransportErrc::aborted );
            });

        f.server->start(
            [&](MessageBuffer)
            {
                FAIL( "Unexpected receive" );
            },
            [&](std::error_code ec)
            {
                CHECK( ec == std::errc::no_such_file_or_directory );
            });

        f.cctx.poll();
        f.cctx.reset();

        AND_WHEN( "the client closes the transport" )
        {
            f.client->close();
            REQUIRE_NOTHROW( f.run() );

            THEN( "the operation is aborted" )
            {
                CHECK( transportFailed );
                CHECK_FALSE( f.client->isOpen() );
            }
        }
    }
}

//------------------------------------------------------------------------------
template <typename TFixture>
void checkCancelSend(TFixture& f)
{
    using TransportPtr = typename TFixture::TransportPtr;

    f.lstn.establish([&](ErrorOr<TransportPtr> transport)
    {
        REQUIRE(transport.has_value());
        f.server = *transport;
    });

    f.cnct.establish([&](ErrorOr<TransportPtr> transport)
    {
        REQUIRE(transport.has_value());
        f.client = *transport;
        CHECK( f.client->info().maxTxLength == 16*1024*1024 );
    });

    CHECK_NOTHROW( f.run() );

    WHEN( "a send operation is in progress" )
    {
        f.client->start(
            [&](MessageBuffer)
            {
                FAIL( "Unexpected receive" );
            },
            [&](std::error_code ec)
            {
                CHECK( ec == TransportErrc::aborted );
            });

        MessageBuffer message(f.client->info().maxTxLength, 'a');
        f.client->send(message);
        REQUIRE_NOTHROW( f.cctx.poll() );
        f.cctx.reset();

        AND_WHEN( "the client closes the transport" )
        {
            f.client->close();

            THEN( "the operation either aborts or completes" )
            {
                REQUIRE_NOTHROW( f.run() );
                CHECK_FALSE( f.client->isOpen() );
            }
        }
    }
}

} // anonymous namespace

#endif // CPPWAMP_TRANSPORTTEST_HPP
