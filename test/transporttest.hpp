/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTTEST_HPP
#define CPPWAMP_TRANSPORTTEST_HPP

#include <iostream>
#include <vector>
#include <catch2/catch.hpp>
#include <cppwamp/asiodefs.hpp>
#include <cppwamp/codec.hpp>
#include <cppwamp/error.hpp>
#include <cppwamp/rawsockoptions.hpp>

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
    AsioContext clientService;
    AsioContext serverService;
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
    using TransportPtr = std::shared_ptr<Transport>;

    template <typename TServerCodecIds>
    LoopbackFixture(AsioContext& clientService,
                    AsioContext& serverService,
                    Opener&& opener,
                    int clientCodec,
                    RawsockMaxLength clientMaxRxLength,
                    Acceptor&& acceptor,
                    TServerCodecIds&& serverCodecs,
                    RawsockMaxLength serverMaxRxLength,
                    bool connected = true)
        : cctx(clientService),
          sctx(serverService),
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
            [&](std::error_code ec, int codec, TransportPtr transport)
            {
                if (ec)
                    throw error::Failure(ec);
                serverCodec = codec;
                server = std::move(transport);
            });

        cnct.establish(
            [&](std::error_code ec, int codec, TransportPtr transport)
            {
                if (ec)
                    throw error::Failure(ec);
                clientCodec = codec;
                client = transport;
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
template <typename TFixture>
void checkConnection(TFixture& f, int expectedCodec,
        size_t clientMaxRxLength = 64*1024, size_t serverMaxRxLength = 64*1024)
{
    using TransportPtr = typename TFixture::TransportPtr;
    f.lstn.establish([&](std::error_code ec, int codec,
                         TransportPtr transport)
    {
        REQUIRE_FALSE( ec );
        REQUIRE( transport );
        CHECK( transport->maxReceiveLength() == serverMaxRxLength );
        CHECK( transport->maxSendLength()    == clientMaxRxLength );
        CHECK( codec                         == expectedCodec );
        f.server = transport;
    });

    f.cnct.establish([&](std::error_code ec, int codec,
                         TransportPtr transport)
    {
        REQUIRE_FALSE( ec );
        REQUIRE( transport );
        CHECK( transport->maxReceiveLength() == clientMaxRxLength );
        CHECK( transport->maxSendLength()    == serverMaxRxLength );
        CHECK( codec                         == expectedCodec );
        f.client = transport;
    });

    CHECK_NOTHROW( f.run() );
}

//------------------------------------------------------------------------------
template <typename TFixture>
void checkSendReply(TFixture& f,
                    typename TFixture::TransportPtr sender,
                    typename TFixture::TransportPtr receiver,
                    const std::string& message,
                    const std::string& reply)
{
    using Transport = typename TFixture::Transport;
    using Buffer    = typename Transport::Buffer;

    bool receivedMessage = false;
    bool receivedReply = false;
    receiver->start(
        [&](Buffer buf)
        {
            receivedMessage = true;
            CHECK( message == buf->data() );
            auto sendBuf = receiver->getBuffer();
            sendBuf->write(reply.data(), reply.size());
            receiver->send(std::move(sendBuf));
        },
        [&](std::error_code ec)
        {
            CHECK( ec == TransportErrc::aborted );
        });

    sender->start(
        [&](Buffer buf)
        {
            receivedReply = true;
            CHECK( reply == buf->data() );
            f.disconnect();
        },
        [&](std::error_code ec)
        {
            CHECK( ec == TransportErrc::aborted );
        });

    auto sendBuf = sender->getBuffer();
    sendBuf->write(message.data(), message.size());
    sender->send(std::move(sendBuf));

    REQUIRE_NOTHROW( f.run() );

    CHECK( receivedMessage );
    CHECK( receivedReply );
}

//------------------------------------------------------------------------------
template <typename TFixture>
void checkSendReply(TFixture& f, const std::string& message,
                    const std::string& reply)
{
    checkSendReply(f, f.client, f.server, message, reply);
}

//------------------------------------------------------------------------------
template <typename TFixture>
void checkCommunications(TFixture& f)
{
    using TransportPtr = typename TFixture::TransportPtr;
    using Transport    = typename TFixture::Transport;
    using Buffer       = typename Transport::Buffer;

    TransportPtr sender = f.client;
    TransportPtr receiver = f.server;
    std::string message = "Hello";
    std::string reply = "World";
    bool receivedMessage = false;
    bool receivedReply = false;

    receiver->start(
        [&](Buffer buf)
        {
            receivedMessage = true;
            CHECK( message == buf->data() );
            auto sendBuf = receiver->getBuffer();
            sendBuf->write(reply.data(), reply.size());
            receiver->send(std::move(sendBuf));
        },
        [&](std::error_code ec)
        {
            CHECK( ec == TransportErrc::aborted );
        });

    sender->start(
        [&](Buffer buf)
        {
            receivedReply = true;
            CHECK( reply == buf->data() );
        },
        [&](std::error_code ec)
        {
            CHECK( ec == TransportErrc::aborted );
        });

    auto sendBuf = sender->getBuffer();
    sendBuf->write(message.data(), message.size());
    sender->send(std::move(sendBuf));

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
    std::string message2 = "Hola";
    std::string reply2 = "Mundo";
    bool receivedMessage2 = false;
    bool receivedReply2 = false;
    message = "Bonjour";
    reply = "Le Monde";
    receivedMessage = false;
    receivedReply = false;

    f.lstn.establish(
        [&](std::error_code ec, int codec, TransportPtr transport)
        {
            REQUIRE_FALSE( ec );
            REQUIRE( transport );
            CHECK( transport->maxReceiveLength() == 64*1024 );
            CHECK( transport->maxSendLength()    == 64*1024 );
            CHECK( codec                         == KnownCodecIds::json() );
            server2 = transport;
            f.sctx.stop();
        });

    f.cnct.establish(
        [&](std::error_code ec, int codec, TransportPtr transport)
        {
            REQUIRE_FALSE( ec );
            REQUIRE( transport );
            CHECK( transport->maxReceiveLength() == 64*1024 );
            CHECK( transport->maxSendLength()    == 64*1024 );
            CHECK( codec                         == KnownCodecIds::json() );
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
        [&](Buffer buf)
        {
            receivedMessage2 = true;
            CHECK( message2 == buf->data() );
            auto sendBuf = receiver2->getBuffer();
            sendBuf->write(reply2.data(), reply2.size());
            receiver2->send(std::move(sendBuf));
        },
        [&](std::error_code ec)
        {
            CHECK( ec == TransportErrc::aborted );
        });

    sender2->start(
        [&](Buffer buf)
        {
            receivedReply2 = true;
            CHECK( reply2 == buf->data() );
            sender2->close();
            receiver2->close();
        },
        [&](std::error_code ec)
        {
            CHECK( ec == TransportErrc::aborted );
        });

    sendBuf = sender->getBuffer();
    sendBuf->write(message.data(), message.size());
    sender->send(std::move(sendBuf));

    sendBuf = sender2->getBuffer();
    sendBuf->write(message2.data(), message2.size());
    sender2->send(std::move(sendBuf));

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
    using Buffer = typename TFixture::Transport::Buffer;

    std::vector<std::string> messages;
    for (int i=0; i<100; ++i)
        messages.emplace_back(i, 'A' + i);

    sender->start(
        [&](Buffer)
        {
            FAIL( "Unexpected receive" );
        },
        [&](std::error_code ec)
        {
            CHECK( ec == TransportErrc::aborted );
        });

    size_t count = 0;

    receiver->start(
        [&](Buffer buf)
        {
            REQUIRE( messages.at(count) == buf->data() );
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
    {
        auto buf = sender->getBuffer();
        buf->write(msg.data(), msg.size());
        sender->send(std::move(buf));
    }

    CHECK_NOTHROW( f.run() );
}

//------------------------------------------------------------------------------
template <typename TFixture>
void checkCancelListen(TFixture& f)
{
    using TransportPtr = typename TFixture::TransportPtr;
    f.lstn.establish([&](std::error_code ec, int, TransportPtr transport)
    {
        CHECK( ec == TransportErrc::aborted );
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
        f.lstn.establish([&](std::error_code ec, int, TransportPtr transport)
        {
            if (!ec)
            {
                CHECK( transport );
                f.server = transport;
            }
            else
                CHECK_FALSE( transport );
        });

        bool connectCanceled = false;
        bool connectCompleted = false;
        f.cnct.establish(
            [&](std::error_code ec, int, TransportPtr transport)
            {
                REQUIRE(( !ec || ec == TransportErrc::aborted ));
                if (ec)
                {
                    connectCanceled = true;
                    CHECK_FALSE( transport );
                }
                else
                {
                    connectCompleted = true;
                    CHECK( transport );
                    f.client = transport;
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
    using Buffer = typename TFixture::Transport::Buffer;
    WHEN( "a receive operation is in progress" )
    {
        bool transportFailed = false;
        f.client->start(
            [&](Buffer)
            {
                FAIL( "Unexpected receive" );
            },
            [&](std::error_code ec)
            {
                transportFailed = true;
                CHECK( ec == TransportErrc::aborted );
            });

        f.server->start(
            [&](Buffer)
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
    using Buffer       = typename TFixture::Transport::Buffer;

    f.lstn.establish([&](std::error_code ec, int, TransportPtr transport)
    {
        REQUIRE_FALSE( ec );
        REQUIRE( transport );
        f.server = transport;
    });

    f.cnct.establish([&](std::error_code ec, int, TransportPtr transport)
    {
        REQUIRE_FALSE( ec );
        REQUIRE( transport );
        f.client = transport;
        CHECK( f.client->maxSendLength() == 16*1024*1024 );
    });

    CHECK_NOTHROW( f.run() );

    WHEN( "a send operation is in progress" )
    {
        f.client->start(
            [&](Buffer)
            {
                FAIL( "Unexpected receive" );
            },
            [&](std::error_code ec)
            {
                CHECK( ec == TransportErrc::aborted );
            });

        auto buf = f.client->getBuffer();
        std::string str(f.client->maxSendLength(), 'a');
        buf->write(str.data(), str.size());
        f.client->send(buf);
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
