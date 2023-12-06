/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <algorithm>
#include <cstring>
#include <vector>
#include <boost/asio/steady_timer.hpp>
#include <catch2/catch.hpp>
#include <cppwamp/asiodefs.hpp>
#include <cppwamp/codec.hpp>
#include <cppwamp/errorcodes.hpp>
#include <cppwamp/transport.hpp>
#include <cppwamp/internal/tcpconnector.hpp>
#include <cppwamp/internal/tcplistener.hpp>
#include <cppwamp/internal/udsconnector.hpp>
#include <cppwamp/internal/udslistener.hpp>
#include "mockrawsockpeer.hpp"

using namespace wamp;
using namespace wamp::internal;

namespace
{

//------------------------------------------------------------------------------
constexpr auto jsonId = KnownCodecIds::json();
constexpr auto msgpackId = KnownCodecIds::msgpack();
constexpr unsigned short tcpTestPort = 9090;
constexpr const char tcpLoopbackAddr[] = "127.0.0.1";
constexpr const char udsTestPath[] = "cppwamptestuds";

const auto tcpHost =
    TcpHost{tcpLoopbackAddr, tcpTestPort}.withLimits(
        RawsockClientLimits{}.withRxMsgSize(64*1024));

const auto tcpEndpoint =
    TcpEndpoint{tcpTestPort}.withLimits(
    RawsockServerLimits{}.withReadMsgSize(64*1024));

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
        cnct = std::make_shared<Connector>(
            boost::asio::make_strand(cctx), std::move(clientSettings),
            clientCodec);
        lstn = std::make_shared<Listener>(
            sctx.get_executor(), boost::asio::make_strand(sctx),
            std::move(serverSettings), std::move(serverCodecs));
        if (connected)
            connect();
    }

    void connect()
    {
        lstn->observe(
            [&](ListenResult result)
            {
                REQUIRE(result.ok());
                auto transport = lstn->take();
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
                REQUIRE(transportOrError.has_value());
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
struct TcpLoopbackFixture : public LoopbackFixture<TcpConnector, TcpListener>
{
    TcpLoopbackFixture(
                bool connected = true,
                int clientCodec = jsonId,
                CodecIdSet serverCodecs = {jsonId},
                std::size_t clientLimit = 64*1024,
                std::size_t serverLimit = 64*1024 )
        : LoopbackFixture(
              TcpHost{tcpLoopbackAddr, tcpTestPort}
                .withLimits(RawsockClientLimits{}.withRxMsgSize(clientLimit)),
              clientCodec,
              TcpEndpoint{tcpTestPort}
                .withLimits(RawsockServerLimits{}.withReadMsgSize(serverLimit)),
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
                std::size_t clientLimit = 64*1024,
                std::size_t serverLimit = 64*1024 )
        : LoopbackFixture(
              UdsHost{udsTestPath}
                .withLimits(RawsockClientLimits{}.withRxMsgSize(clientLimit)),
              clientCodec,
              UdsEndpoint{udsTestPath}
                .withLimits(RawsockServerLimits{}.withReadMsgSize(serverLimit)),
              serverCodecs,
              connected )
    {}
};

//------------------------------------------------------------------------------
MessageBuffer makeMessageBuffer(const std::string& str)
{
    using MessageBufferByte = typename MessageBuffer::value_type;
    auto data = reinterpret_cast<const MessageBufferByte*>(str.data());
    return MessageBuffer(data, data + str.size());
}

//------------------------------------------------------------------------------
enum class ServerTimeoutMonitorTestEvent
{
    start,
    heartbeat,
    startRead,
    updateRead,
    endRead,
    startWrite,
    updateWrite,
    endWrite,
    check
};

//------------------------------------------------------------------------------
struct ServerTimeoutMonitorTestVector
{
    using Event = ServerTimeoutMonitorTestEvent;

    ServerTimeoutMonitorTestVector(unsigned ms, Event ev)
        : milliseconds(ms), event(ev)
    {}

    ServerTimeoutMonitorTestVector(unsigned ms, Event ev, std::size_t bytes)
        : milliseconds(ms), event(ev), bytesTransferred(bytes)
    {}

    ServerTimeoutMonitorTestVector(unsigned ms, Event ev, TransportErrc errc)
        : milliseconds(ms), event(ev), status(errc)
    {}

    unsigned milliseconds;
    ServerTimeoutMonitorTestEvent event;
    std::size_t bytesTransferred = 0;
    TransportErrc status = TransportErrc::success;
};

//------------------------------------------------------------------------------
void checkServerTimeoutMonitor(
    const TcpEndpoint& endpoint,
    const std::vector<ServerTimeoutMonitorTestVector>& testVectors)
{
    using E = ServerTimeoutMonitorTestEvent;
    using Monitor = ServerTimeoutMonitor<TcpEndpoint>;
    using std::chrono::seconds;

    auto sharedEndpoint = std::make_shared<TcpEndpoint>(std::move(endpoint));
    Monitor monitor{sharedEndpoint};

    auto start = std::chrono::steady_clock::now();
    unsigned testSetNumber = 0;

    for (const auto& vec: testVectors)
    {
        INFO("For test set number " << testSetNumber <<
             ", time index " << vec.milliseconds);
        auto now = start + std::chrono::milliseconds(vec.milliseconds);
        auto bytes = vec.bytesTransferred;

        switch (vec.event)
        {
        case E::start:
            ++testSetNumber;
            start = std::chrono::steady_clock::now();
            monitor = Monitor{sharedEndpoint};
            monitor.start(now);
            break;;

        case E::heartbeat:   monitor.heartbeat(now);          break;
        case E::startRead:   monitor.startRead(now);          break;
        case E::updateRead:  monitor.updateRead(now, bytes);  break;
        case E::endRead:     monitor.endRead(now);            break;
        case E::startWrite:  monitor.startWrite(now, true);   break;
        case E::updateWrite: monitor.updateWrite(now, bytes); break;
        case E::endWrite:    monitor.endWrite(now, true);     break;

        case E::check:
            CHECK(monitor.check(now) == vec.status);
            break;
        }
    }
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
        auto transport = f.lstn->take();
        REQUIRE( transport != nullptr );
        f.server = transport;
        f.server->admit(
            [=](AdmitResult result)
            {
                REQUIRE(result.status() == AdmitStatus::wamp);
                CHECK(result.codecId() == expectedCodec);
                CHECK( transport->info().codecId() == expectedCodec );
                CHECK( transport->info().receiveLimit() == serverMaxRxLength );
                CHECK( transport->info().sendLimit() == clientMaxRxLength );
            });
    });
    f.lstn->establish();

    f.cnct->establish([&](ErrorOr<Transporting::Ptr> transportOrError)
    {
        REQUIRE( transportOrError.has_value() );
        auto transport = *transportOrError;
        REQUIRE( transport != nullptr );
        CHECK( transport->info().codecId() == expectedCodec );
        CHECK( transport->info().receiveLimit() == clientMaxRxLength );
        CHECK( transport->info().sendLimit() == serverMaxRxLength );
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
        f.server = f.lstn->take();
        f.server->admit(
            [&](AdmitResult result)
            {
                serverEc = result.error();
                f.server->close();
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
    uint32_t cannedHandshake, TransportErrc expectedClientErrc)
{
    IoContext ioctx;
    auto exec = ioctx.get_executor();
    auto strand = boost::asio::make_strand(exec);

    auto server = test::MockRawsockServer::create(exec, tcpTestPort,
                                                  cannedHandshake);
    server->start();

    std::error_code clientEc;
    auto cnct = std::make_shared<TcpConnector>(strand, tcpHost, jsonId);
    cnct->establish(
        [&clientEc, &server](ErrorOr<Transporting::Ptr> transport)
        {
            if (!transport.has_value())
                clientEc = transport.error();
            server->close();
        });

    CHECK_NOTHROW( ioctx.run() );
    CHECK( clientEc == expectedClientErrc );
}

//------------------------------------------------------------------------------
void checkCannedClientHandshake(uint32_t cannedHandshake,
                                TransportErrc expectedServerCode)
{
    IoContext ioctx;
    auto exec = ioctx.get_executor();
    auto strand = boost::asio::make_strand(exec);
    auto lstn = std::make_shared<TcpListener>(exec, strand, tcpEndpoint,
                                              CodecIdSet{jsonId});
    Transporting::Ptr server;
    std::error_code serverEc;

    lstn->observe(
        [&](ListenResult result)
        {
            REQUIRE( result.ok() );
            server = lstn->take();
            server->admit(
                [&serverEc, &server](AdmitResult result)
                {
                    serverEc = result.error();
                    server->close();
                });
        });
    lstn->establish();

    auto client = test::MockRawsockClient::create(ioctx, tcpTestPort,
                                                  cannedHandshake);
    client->connect();
    CHECK_NOTHROW( ioctx.run() );
    ioctx.restart();

    client->start();
    CHECK_NOTHROW( ioctx.run() );
    CHECK( serverEc == expectedServerCode );
}

} // anonymous namespace

//------------------------------------------------------------------------------
TEST_CASE( "ServerTimeoutMonitor", "[Transport]" )
{
    using E = ServerTimeoutMonitorTestEvent;
    using std::chrono::seconds;

    static constexpr auto ok = TransportErrc::success;

    SECTION("Silence timeouts")
    {
        auto endpoint = TcpEndpoint(tcpTestPort).withLimits(
            TcpEndpoint::Limits{}
                .withReadTimeout( {  seconds{ 5}, 100, seconds{15}})
                .withWriteTimeout({  seconds{10}, 100, seconds{20}})
                .withSilenceTimeout( seconds{300}));

        std::vector<ServerTimeoutMonitorTestVector> testVectors
        {
            // No reads/writes/heartbeats
            {     0, E::start},
            {     1, E::check,       ok},
            {299999, E::check,       ok},
            {300000, E::check,       TransportErrc::silenceTimeout},
            {300001, E::check,       TransportErrc::silenceTimeout},

            // Delayed by read
            {     0, E::start},
            {  1000, E::startRead},
            {  2000, E::updateRead,  50},
            {  3000, E::endRead},
            {  8000, E::check,       ok},
            {302999, E::check,       ok},
            {303000, E::check,       TransportErrc::silenceTimeout},

            // Delayed by write
            {     0, E::start},
            {  1000, E::startWrite},
            {  2000, E::updateWrite, 50},
            {  3000, E::endWrite},
            {  8000, E::check,       ok},
            {302999, E::check,       ok},
            {303000, E::check,       TransportErrc::silenceTimeout},

            // Delayed by heartbeat
            {     0, E::start},
            {  1000, E::heartbeat},
            {300999, E::check,       ok},
            {301000, E::check,       TransportErrc::silenceTimeout},
        };

        checkServerTimeoutMonitor(endpoint, testVectors);
    }

    SECTION("Loiter timeouts")
    {
        auto endpoint = TcpEndpoint(tcpTestPort).withLimits(
            TcpEndpoint::Limits{}
                .withReadTimeout( {  seconds{ 5}, 100, seconds{15}})
                .withWriteTimeout({  seconds{10}, 100, seconds{20}})
                .withSilenceTimeout( seconds{300})
                .withLoiterTimeout(  seconds{600}));

        std::vector<ServerTimeoutMonitorTestVector> testVectors
        {
            // Delayed by read
            {     0, E::start},
            {  1000, E::startRead},
            {  2000, E::updateRead,  50},
            {  3000, E::endRead},
            {  8000, E::check,       ok},
            {200000, E::heartbeat},
            {500000, E::heartbeat},
            {602999, E::check,       ok},
            {603000, E::check,       TransportErrc::loiterTimeout},

            // Delayed by write
            {     0, E::start},
            {  1000, E::startWrite},
            {  2000, E::updateWrite, 50},
            {  3000, E::endWrite},
            {  8000, E::check,       ok},
            {200000, E::heartbeat},
            {500000, E::heartbeat},
            {602999, E::check,       ok},
            {603000, E::check,       TransportErrc::loiterTimeout}
        };

        checkServerTimeoutMonitor(endpoint, testVectors);
    }

    SECTION("Overstay timeouts")
    {
        auto endpoint = TcpEndpoint(tcpTestPort).withLimits(
            TcpEndpoint::Limits{}
                .withReadTimeout( {   seconds{ 5}, 100, seconds{15}})
                .withWriteTimeout({   seconds{10}, 100, seconds{20}})
                .withSilenceTimeout(  seconds{300})
                .withLoiterTimeout(   seconds{600})
                .withOverstayTimeout( seconds{900}));

        std::vector<ServerTimeoutMonitorTestVector> testVectors
        {
            // Not delayed by anything
            {     0, E::start},
            {  1000, E::startRead},
            {  3000, E::endRead},
            {  8000, E::check,       ok},
            { 11000, E::startWrite},
            { 13000, E::endWrite},
            { 18000, E::check,       ok},
            {200000, E::heartbeat},
            {500000, E::heartbeat},
            {601000, E::startWrite},
            {603000, E::endWrite},
            {700000, E::heartbeat},
            {899999, E::check,       ok},
            {900000, E::check,       TransportErrc::overstayTimeout},

            // Does not interrupt a read in progress
            {     0, E::start},
            {  1000, E::startRead},
            {  3000, E::endRead},
            {  8000, E::check,       ok},
            {200000, E::heartbeat},
            {500000, E::heartbeat},
            {601000, E::startRead},
            {603000, E::endRead},
            {700000, E::heartbeat},
            {899500, E::startRead},
            {900499, E::check,       ok},
            {900500, E::endRead},
            {900501, E::check,       TransportErrc::overstayTimeout},

            // Does not interrupt a write in progress
            {     0, E::start},
            {  1000, E::startWrite},
            {  3000, E::endWrite},
            {  8000, E::check,       ok},
            {200000, E::heartbeat},
            {500000, E::heartbeat},
            {601000, E::startWrite},
            {603000, E::endWrite},
            {700000, E::heartbeat},
            {899500, E::startWrite},
            {900499, E::check,       ok},
            {900500, E::endWrite},
            {900501, E::check,       TransportErrc::overstayTimeout},
        };

        checkServerTimeoutMonitor(endpoint, testVectors);
    }

    SECTION("Non-progressive read timeouts")
    {
        auto endpoint = TcpEndpoint(tcpTestPort).withLimits(
            TcpEndpoint::Limits{}.withReadTimeout({seconds{5}}));

        std::vector<ServerTimeoutMonitorTestVector> testVectors
        {
            // No bytes transferred
            {     0, E::start},
            {     0, E::startRead},
            {     1, E::check,       ok},
            {  4999, E::check,       ok},
            {  5000, E::check,       TransportErrc::readTimeout},
            {  5001, E::check,       TransportErrc::readTimeout},

            // Incomplete read
            {     0, E::start},
            {     0, E::startRead},
            {  1000, E::updateRead,  100},
            {  1001, E::check,       ok},
            {  4999, E::check,       ok},
            {  5000, E::check,       TransportErrc::readTimeout},

            // Read completed
            {     0, E::start},
            {     0, E::startRead},
            {  1000, E::updateRead,  100},
            {  2000, E::endRead},
            {  2001, E::check,       ok},
            { 10000, E::check,       ok}
         };

        checkServerTimeoutMonitor(endpoint, testVectors);
    }

    SECTION("Progressive read timeouts")
    {
        auto endpoint = TcpEndpoint(tcpTestPort).withLimits(
            TcpEndpoint::Limits{}
                .withReadTimeout({seconds{5}, 100, seconds{15}}));

        std::vector<ServerTimeoutMonitorTestVector> testVectors
        {
            // No bytes transferred
            {     0, E::start},
            {     0, E::startRead},
            {     1, E::check,       ok},
            {  4999, E::check,       ok},
            {  5000, E::check,       TransportErrc::readTimeout},
            {  5001, E::check,       TransportErrc::readTimeout},

            // Not enough bytes transferred to delay
            {     0, E::start},
            {     0, E::startRead},
            {  1000, E::updateRead,  99},
            {  1001, E::check,       ok},
            {  4999, E::check,       ok},
            {  5000, E::check,       TransportErrc::readTimeout},

            // Exact number of bytes transferred to delay by 1s
            {     0, E::start},
            {     0, E::startRead},
            {  1000, E::updateRead,  100},
            {  1001, E::check,       ok},
            {  5999, E::check,       ok},
            {  6000, E::check,       TransportErrc::readTimeout},

            // Unused rate bytes carried over
            {     0, E::start},
            {     0, E::startRead},
            {  1000, E::updateRead,   99},
            {  2000, E::updateRead,  101},
            {  6999, E::check,       ok},
            {  7000, E::check,       TransportErrc::readTimeout},
        };

        checkServerTimeoutMonitor(endpoint, testVectors);
    }

    SECTION("Non-progressive write timeouts")
    {
        auto endpoint = TcpEndpoint(tcpTestPort).withLimits(
            TcpEndpoint::Limits{}.withWriteTimeout({seconds{10}}));

        std::vector<ServerTimeoutMonitorTestVector> testVectors
        {
            // No bytes transferred
            {     0, E::start},
            {     0, E::startWrite},
            {     1, E::check,       ok},
            {  9999, E::check,       ok},
            { 10000, E::check,       TransportErrc::writeTimeout},
            { 10001, E::check,       TransportErrc::writeTimeout},

            // Incomplete write
            {     0, E::start},
            {     0, E::startWrite},
            {  1000, E::updateWrite, 100},
            {  1001, E::check,       ok},
            {  9999, E::check,       ok},
            { 10000, E::check,       TransportErrc::writeTimeout},

            // Write completed
            {     0, E::start},
            {     0, E::startWrite},
            {  1000, E::updateWrite, 100},
            {  2000, E::endWrite},
            {  2001, E::check,       ok},
            { 20000, E::check,       ok}
        };

        checkServerTimeoutMonitor(endpoint, testVectors);
    }

    SECTION("Progressive write timeouts")
    {
        auto endpoint = TcpEndpoint(tcpTestPort).withLimits(
            TcpEndpoint::Limits{}
                .withWriteTimeout({seconds{10}, 100, seconds{20}}));

        std::vector<ServerTimeoutMonitorTestVector> testVectors
        {
             // No bytes transferred
             {     0, E::start},
             {     0, E::startWrite},
             {     1, E::check,       ok},
             {  9999, E::check,       ok},
             { 10000, E::check,       TransportErrc::writeTimeout},
             { 10001, E::check,       TransportErrc::writeTimeout},

             // Not enough bytes transferred to delay
             {     0, E::start},
             {     0, E::startWrite},
             {  1000, E::updateWrite, 99},
             {  1001, E::check,       ok},
             {  9999, E::check,       ok},
             { 10000, E::check,       TransportErrc::writeTimeout},

             // Exact number of bytes transferred to delay by 1s
             {     0, E::start},
             {     0, E::startWrite},
             {  1000, E::updateWrite, 100},
             {  1001, E::check,       ok},
             { 10999, E::check,       ok},
             { 11000, E::check,       TransportErrc::writeTimeout},

             // Unused rate bytes carried over
             {     0, E::start},
             {     0, E::startWrite},
             {  1000, E::updateWrite,  99},
             {  2000, E::updateWrite, 101},
             { 11999, E::check,       ok},
             { 12000, E::check,       TransportErrc::writeTimeout},
         };

        checkServerTimeoutMonitor(endpoint, testVectors);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "RawsockHandshake Parsing", "[Transport][Rawsock]" )
{
    struct TestVector
    {
        uint32_t bits;
        size_t sizeLimit;
        int codecId;
        uint16_t reserved;
        TransportErrc errorCode;
        bool hasMagicOctet;
        bool hasError;
    };

    using E = TransportErrc;
    constexpr bool y = true;
    constexpr bool n = false;
    constexpr auto json = KnownCodecIds::json();
    constexpr auto msgp = KnownCodecIds::msgpack();
    constexpr auto cbor = KnownCodecIds::cbor();

    // Bitfield:
    // Client: 7fLSRRRR
    // Server: 7fE0RRRR

    // Errors:
    // 0: illegal (must not be used)
    // 1: serializer unsupported
    // 2: maximum message length unacceptable
    // 3: use of reserved bits (unsupported feature)
    // 4: maximum connection count reached

    std::vector<TestVector> testVectors
    {
        // bits         size  codec reserved  error        magic?  error?
        //             limit                  code
        {0x00000000,      512,  0x0, 0x0000, E::success,        n, y},
        {0x7EFFFFFF, 16777216,  0xF, 0xFFFF, E::failed,         n, n},
        {0x7F000000,      512,  0x0, 0x0000, E::success,        y, y},
        {0x7F000001,      512,  0x0, 0x0001, E::success,        y, y},
        {0x7F00FFFF,      512,  0x0, 0xFFFF, E::success,        y, y},
        {0x7F010000,      512, json, 0x0000, E::success,        y, n},
        {0x7F010001,      512, json, 0x0001, E::success,        y, n},
        {0x7F01FFFF,      512, json, 0xFFFF, E::success,        y, n},
        {0x7F020000,      512, msgp, 0x0000, E::success,        y, n},
        {0x7F020001,      512, msgp, 0x0001, E::success,        y, n},
        {0x7F02FFFF,      512, msgp, 0xFFFF, E::success,        y, n},
        {0x7F030000,      512, cbor, 0x0000, E::success,        y, n},
        {0x7F030001,      512, cbor, 0x0001, E::success,        y, n},
        {0x7F03FFFF,      512, cbor, 0xFFFF, E::success,        y, n},
        {0x7F0F0000,      512,  0xF, 0x0000, E::success,        y, n},
        {0x7F0F0001,      512,  0xF, 0x0001, E::success,        y, n},
        {0x7F0FFFFF,      512,  0xF, 0xFFFF, E::success,        y, n},
        {0x7F100000,     1024,  0x0, 0x0000, E::badSerializer,  y, y},
        {0x7F200000,     2048,  0x0, 0x0000, E::badLengthLimit, y, y},
        {0x7F300000,     4096,  0x0, 0x0000, E::badFeature,     y, y},
        {0x7F400000,     8192,  0x0, 0x0000, E::shedded,        y, y},
        {0x7F500000,    16384,  0x0, 0x0000, E::failed,         y, y},
        {0x7FE00000,  8388608,  0x0, 0x0000, E::failed,         y, y},
        {0x7FF00000, 16777216,  0x0, 0x0000, E::failed,         y, y},
        {0x7FFFFFFF, 16777216,  0xF, 0xFFFF, E::failed,         y, n},
        {0x80000000,      512,  0x0, 0x0000, E::success,        n, y},
        {0xFFFFFFFF, 16777216,  0xF, 0xFFFF, E::failed,         n, n},
    };

    for (const auto& vec: testVectors)
    {
        INFO("For bits=0x" << std::setfill('0') << std::setw(8) << std::right
                           << std::hex << vec.bits);
        RawsockHandshake hs{vec.bits};
        CHECK( hs.sizeLimit() == vec.sizeLimit );
        CHECK( hs.codecId() == vec.codecId );
        CHECK( hs.reserved() == vec.reserved );
        CHECK( hs.errorCode() == vec.errorCode );
        CHECK( hs.hasMagicOctet() == vec.hasMagicOctet );
        CHECK( hs.hasError() == vec.hasError );
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "RawsockHandshake Generation", "[Transport][Rawsock]" )
{
    struct TestVector
    {
        int codecId;
        size_t sizeLimit;
        uint32_t bits;
    };

    constexpr auto json = KnownCodecIds::json();
    constexpr auto msgp = KnownCodecIds::msgpack();
    constexpr auto cbor = KnownCodecIds::cbor();
    constexpr auto maxSize = std::numeric_limits<size_t>::max();

    // Bitfield:
    // Client: 7fLSRRRR

    std::vector<TestVector> testVectors
    {
        { 0x0,  0x00000000, 0x7F000000},
        { 0x0,  0x00FFFFFF, 0x7FF00000},
        { 0x0,  0x01000000, 0x7FF00000},
        {json,  0x00000000, 0x7F010000},
        {json,  0x00000001, 0x7F010000},
        {json,  0x000001FF, 0x7F010000},
        {json,  0x00000200, 0x7F010000},
        {json,  0x00000201, 0x7F110000},
        {json,  0x000003FF, 0x7F110000},
        {json,  0x00000400, 0x7F110000},
        {json,  0x00000401, 0x7F210000},
        {json,  0x000007FF, 0x7F210000},
        {json,  0x00000800, 0x7F210000},
        {json,  0x00200001, 0x7FD10000},
        {json,  0x003FFFFF, 0x7FD10000},
        {json,  0x00400000, 0x7FD10000},
        {json,  0x00400001, 0x7FE10000},
        {json,  0x007FFFFF, 0x7FE10000},
        {json,  0x00800000, 0x7FE10000},
        {json,  0x00800001, 0x7FF10000},
        {json,  0x00FFFFFF, 0x7FF10000},
        {json,  0x01000000, 0x7FF10000},
        {json,     maxSize, 0x7FF10000},
        {msgp,  0x00000000, 0x7F020000},
        {msgp,  0x00FFFFFF, 0x7FF20000},
        {cbor,  0x00000000, 0x7F030000},
        {cbor,  0x00FFFFFF, 0x7FF30000},
        { 0x4,  0x00000000, 0x7F040000},
        { 0x4,  0x00FFFFFF, 0x7FF40000},
        { 0x7,  0x00000000, 0x7F070000},
        { 0x7,  0x00FFFFFF, 0x7FF70000}
    };

    for (const auto& vec: testVectors)
    {
        INFO("For codec=" << vec.codecId <<
             ", sizeLimit=0x" << std::setfill('0') << std::setw(8) << std::right
                              << std::hex << vec.sizeLimit);
        auto hs = RawsockHandshake{}.setCodecId(vec.codecId)
                                    .setSizeLimit(vec.sizeLimit);
        CHECK( hs.toHostOrder() == vec.bits );
    }

    // Bitfield:
    // Server: 7fE0RRRR

    // Errors:
    // 0: illegal (must not be used)
    // 1: serializer unsupported
    // 2: maximum message length unacceptable
    // 3: use of reserved bits (unsupported feature)
    // 4: maximum connection count reached

    CHECK( RawsockHandshake::eUnsupportedFormat().toHostOrder() == 0x7F100000 );
    CHECK( RawsockHandshake::eUnacceptableLimit().toHostOrder() == 0x7F200000 );
    CHECK( RawsockHandshake::eReservedBitsUsed().toHostOrder()  == 0x7F300000 );
    CHECK( RawsockHandshake::eMaxConnections().toHostOrder()    == 0x7F400000 );
}

//------------------------------------------------------------------------------
TEST_CASE( "RawsockHeader", "[Transport][Rawsock]" )
{
    struct TestVector
    {
        TestVector(TransportFrameKind frameKind, size_t length, uint32_t bits)
            : frameKind(frameKind), length(length), bits(bits)
        {}

        TestVector(int frameKind, size_t length, uint32_t bits)
            : TestVector(static_cast<TransportFrameKind>(frameKind), length,
                         bits)
        {}

        TransportFrameKind frameKind;
        size_t length;
        uint32_t bits;
    };

    static auto wampFrame = TransportFrameKind::wamp;
    static auto pingFrame = TransportFrameKind::ping;
    static auto pongFrame = TransportFrameKind::pong;

    std::vector<TestVector> testVectors
    {
        {wampFrame, 0x00000000, 0x00000000},
        {wampFrame, 0x00000001, 0x00000001},
        {wampFrame, 0x00FFFFFF, 0x00FFFFFF},
        {wampFrame, 0x01000000, 0x08000000},
        {pingFrame, 0x00000000, 0x01000000},
        {pingFrame, 0x00000001, 0x01000001},
        {pingFrame, 0x00FFFFFF, 0x01FFFFFF},
        {pingFrame, 0x01000000, 0x09000000},
        {pongFrame, 0x00000000, 0x02000000},
        {pongFrame, 0x00000001, 0x02000001},
        {pongFrame, 0x00FFFFFF, 0x02FFFFFF},
        {pongFrame, 0x01000000, 0x0A000000},
        {0x03,      0x00000000, 0x03000000},
        {0x03,      0x00000001, 0x03000001},
        {0x03,      0x00FFFFFF, 0x03FFFFFF},
        {0x03,      0x01000000, 0x0B000000},
        {0x07,      0x00000000, 0x07000000},
        {0x07,      0x00000001, 0x07000001},
        {0x07,      0x00FFFFFF, 0x07FFFFFF},
        {0x07,      0x01000000, 0x0F000000},
    };

    for (unsigned i=0; i<testVectors.size(); ++i)
    {
        INFO("For test vector index " << i);
        const auto& vec = testVectors[i];
        auto hdr = RawsockHeader{}.setFrameKind(vec.frameKind)
                                  .setLength(vec.length);
        CHECK( hdr.frameKind() == vec.frameKind );
        CHECK( hdr.length() == vec.length );
        CHECK( hdr.toHostOrder() == vec.bits );

        RawsockHeader hdr2{vec.bits};
        CHECK( hdr2.frameKind() == vec.frameKind );
        CHECK( hdr2.length() == vec.length );
        CHECK( hdr2.toHostOrder() == vec.bits );
    }
}

//------------------------------------------------------------------------------
SCENARIO( "Normal connection", "[Transport][Rawsock]" )
{
GIVEN( "an unconnected TCP connector/listener pair" )
{
    WHEN( "the client and server use JSON" )
    {
        TcpLoopbackFixture f(false, jsonId, {jsonId}, 32*1024, 128*1024);
        checkConnection(f, jsonId, 32*1024, 128*1024);
    }
    WHEN( "the client uses JSON and the server supports both" )
    {
        TcpLoopbackFixture f(false, jsonId, {jsonId, msgpackId},
                             32*1024, 128*1024 );
        checkConnection(f, jsonId, 32*1024, 128*1024);
    }
    WHEN( "the client and server use Msgpack" )
    {
        TcpLoopbackFixture f(false, msgpackId, {msgpackId}, 32*1024, 128*1024 );
        checkConnection(f, msgpackId, 32*1024, 128*1024);
    }
    WHEN( "the client uses Msgpack and the server supports both" )
    {
        TcpLoopbackFixture f(false, msgpackId, {jsonId, msgpackId},
                             32*1024, 128*1024);
        checkConnection(f, msgpackId, 32*1024, 128*1024);
    }
}
GIVEN( "an unconnected UDS connector/listener pair" )
{
    WHEN( "the client and server use JSON" )
    {
        UdsLoopbackFixture f(false, jsonId, {jsonId}, 32*1024, 128*1024 );
        checkConnection(f, jsonId, 32*1024, 128*1024);
    }
    WHEN( "the client uses JSON and the server supports both" )
    {
        UdsLoopbackFixture f(false, jsonId, {jsonId, msgpackId},
                             32*1024, 128*1024 );
        checkConnection(f, jsonId, 32*1024, 128*1024);
    }
    WHEN( "the client and server use Msgpack" )
    {
        UdsLoopbackFixture f(false, msgpackId, {msgpackId}, 32*1024, 128*1024);
        checkConnection(f, msgpackId, 32*1024, 128*1024);
    }
    WHEN( "the client uses Msgpack and the server supports both" )
    {
        UdsLoopbackFixture f(false, msgpackId, {jsonId, msgpackId},
                             32*1024, 128*1024);
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
            auto transport = f.lstn->take();
            REQUIRE( transport != nullptr );
            server2 = transport;
            server2->admit(
                [=, &f](AdmitResult result)
                {
                    REQUIRE(result.status() == AdmitStatus::wamp);
                    CHECK( result.codecId() == KnownCodecIds::json() );
                    CHECK( transport->info().codecId() == KnownCodecIds::json() );
                    CHECK( transport->info().receiveLimit() == 64*1024 );
                    CHECK( transport->info().sendLimit() == 64*1024 );
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
            CHECK( transport->info().sendLimit() == 64*1024 );
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
    const MessageBuffer message(f.client->info().receiveLimit(), 'm');
    const MessageBuffer reply(f.server->info().receiveLimit(), 'r');;
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
TEST_CASE( "Raw socket shedding", "[Transport][Rawsock]" )
{
    IoContext ioctx;
    auto exec = ioctx.get_executor();
    auto strand = boost::asio::make_strand(exec);

    Transporting::Ptr server;
    auto lstn = std::make_shared<TcpListener>(exec, strand, tcpEndpoint,
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

    auto cnct = std::make_shared<TcpConnector>(strand, tcpHost, jsonId);
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
    CHECK( clientError == TransportErrc::shedded );
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE( "Raw socket client aborting", "[Transport][Rawsock]",
                    TcpLoopbackFixture, UdsLoopbackFixture )
{
    TestType f;
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
                f.server->shutdown({}, [](std::error_code ec) {});
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

    CHECK( clientError == TransportErrc::ended );
    CHECK( serverError == TransportErrc::ended );
    CHECK_FALSE( abortError );
    CHECK( rxMessage == abortMessage );
    CHECK( abortHandlerInvoked );
    CHECK_FALSE( abortError );
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE( "Raw socket server aborting", "[Transport][Rawsock]",
                    TcpLoopbackFixture, UdsLoopbackFixture )
{
    TestType f;
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
                f.client->shutdown({}, [](std::error_code ec) {});
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
    CHECK( serverError == TransportErrc::ended );
    CHECK_FALSE( abortError );
    CHECK( rxMessage == abortMessage );
    CHECK( abortHandlerInvoked );
    CHECK_FALSE( abortError );
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE( "Graceful raw socket shutdown", "[Transport][Rawsock]",
                    TcpLoopbackFixture, UdsLoopbackFixture )
{
    TestType f;
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

    CHECK( clientError == TransportErrc::ended );
    CHECK( serverError == TransportErrc::ended );
    CHECK( shutdownHandlerInvoked );
    CHECK_FALSE( shutdownError );
}

//------------------------------------------------------------------------------
TEST_CASE( "Raw socket shutdown during send", "[Transport][Rawsock]" )
{
    constexpr unsigned bigLength = 16*1024*1024;
    TcpLoopbackFixture f(true, jsonId, {jsonId}, bigLength, bigLength);
    MessageBuffer bigMessage(bigLength, 'A');
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
    f.server->send(bigMessage);

    f.run();

    CHECK( clientError == TransportErrc::ended );
    CHECK( serverError == TransportErrc::ended );
    CHECK( shutdownHandlerInvoked );
    CHECK_FALSE( shutdownError );
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
            REQUIRE( !buf );
            serverError = buf.error();
        },
        nullptr);

    f.cctx.poll();
    f.cctx.reset();

    // Close the transport while the receive operation is in progress,
    // and check the client handler received an TransportErrc::aborted error.
    f.client->close();
    REQUIRE_NOTHROW( f.run() );
    CHECK( clientError == TransportErrc::aborted );
    CHECK_FALSE( !serverError );
}

//------------------------------------------------------------------------------
TEMPLATE_TEST_CASE( "Cancel send", "[Transport][Rawsock]",
                    TcpLoopbackFixture, UdsLoopbackFixture )
{
    // The size of transmission is set to maximum to increase the likelyhood
    // of the operation being aborted, rather than completed.
    constexpr unsigned bigLength = 16*1024*1024;
    TestType f(false, jsonId, {jsonId}, bigLength, bigLength);
    f.lstn->observe([&](ListenResult result)
    {
        REQUIRE(result.ok());
        f.server = f.lstn->take();
        f.server->admit(
            [&](AdmitResult r)
            {
                REQUIRE(r.status() == AdmitStatus::wamp);
                CHECK( f.server->info().sendLimit() == bigLength );
            });
    });
    f.lstn->establish();
    f.cnct->establish([&](ErrorOr<Transporting::Ptr> transport)
    {
        REQUIRE(transport.has_value());
        f.client = *transport;
        CHECK( f.client->info().sendLimit() == bigLength );
    });
    f.run();

    // Start a send operation
    std::error_code clientError;
    f.client->start(
        [&](ErrorOr<MessageBuffer> buf)
        {
            if (!buf.has_value())
                clientError = buf.error();
        },
        nullptr);
    MessageBuffer message(bigLength, 'a');
    f.client->send(message);
    REQUIRE_NOTHROW( f.cctx.poll() );
    f.cctx.reset();

    // Close the transport and check that the client handler received an
    // TransportErrc::aborted error.
    f.client->close();
    f.run();
    CHECK( clientError == TransportErrc::aborted );
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
    checkCannedServerHandshake(0x7f200000, TransportErrc::badLengthLimit);
}
GIVEN( "use of reserved bits" )
{
    checkCannedServerHandshake(0x7f300000, TransportErrc::badFeature);
}
GIVEN( "maximum connections reached" )
{
    checkCannedServerHandshake(0x7f400000, TransportErrc::shedded);
}
GIVEN( "future error code" )
{
    checkCannedServerHandshake(0x7f500000, TransportErrc::failed);
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
    checkCannedClientHandshake(0xff710000, TransportErrc::badHandshake);
}
GIVEN( "a client that uses a zeroed magic octet" )
{
    checkCannedClientHandshake(0x00710000, TransportErrc::badHandshake);
}
GIVEN( "a client that uses reserved bits" )
{
    checkCannedClientHandshake(0x7f710001, TransportErrc::badFeature);
}
}

//------------------------------------------------------------------------------
SCENARIO( "Client sending a message longer than maximum",
          "[Transport][Rawsock]" )
{
GIVEN ( "mock client sending a message exceeding the server's maximum length" )
{
    IoContext ioctx;
    auto exec = ioctx.get_executor();
    auto strand = boost::asio::make_strand(exec);
    std::string tooLong(64*1024 + 1, 'A');

    Transporting::Ptr server;
    auto lstn = std::make_shared<TcpListener>(exec, strand, tcpEndpoint,
                                              CodecIdSet{jsonId});
    lstn->observe(
        [&](ListenResult result)
        {
            REQUIRE( result.ok() );
            server = lstn->take();
            server->admit(
                [](AdmitResult r) {REQUIRE(r.status() == AdmitStatus::wamp);});
        });
    lstn->establish();

    auto client = test::MockRawsockClient::create(ioctx, tcpTestPort);
    client->load({{tooLong}});
    client->connect();

    CHECK_NOTHROW( ioctx.run() );
    ioctx.restart();
    REQUIRE( server );

    WHEN( "the client sends a message that exceeds the server's maximum" )
    {
        client->start();

        std::error_code serverError;
        server->start(
            [&](ErrorOr<MessageBuffer> message)
            {
                REQUIRE( !message );
                serverError = message.error();
                server->close();
            },
            nullptr);

        THEN( "the server obtains an error while receiving" )
        {
            CHECK_NOTHROW( ioctx.run() );
            UNSCOPED_INFO("server error message:" << serverError.message());
            CHECK( serverError == TransportErrc::inboundTooLong );
        }
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Server sending a message longer than maximum",
          "[Transport][Rawsock]" )
{
GIVEN ( "a mock server sending a message exceeding the client's limit" )
{
    IoContext ioctx;
    auto exec = ioctx.get_executor();
    auto strand = boost::asio::make_strand(exec);
    std::string tooLong(64*1024 + 1, 'A');

    auto server = test::MockRawsockServer::create(exec, tcpTestPort);
    server->load({{{tooLong}}});
    server->start();

    auto limits = RawsockClientLimits{}.withRxMsgSize(tooLong.size() - 1);
    auto host = tcpHost;
    host.withLimits(limits);
    auto cnct = std::make_shared<TcpConnector>(strand, host, jsonId);
    Transporting::Ptr client;
    cnct->establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            REQUIRE( transport.has_value() );
            client = std::move(*transport);
            ioctx.stop();
        });

    CHECK_NOTHROW( ioctx.run() );
    ioctx.restart();
    REQUIRE( client );

    WHEN( "the server sends a message that exceeds the client's maximum" )
    {
        std::error_code clientError;
        client->start(
            [&](ErrorOr<MessageBuffer> message)
            {
                if (!message.has_value() && !clientError)
                    clientError = message.error();
                server->close();
            },
            nullptr);
        client->send(makeMessageBuffer("Hello"));

        THEN( "the client obtains an error while receiving" )
        {
            CHECK_NOTHROW( ioctx.run() );
            UNSCOPED_INFO("client error message:" << clientError.message());
            CHECK( clientError == TransportErrc::inboundTooLong );
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

    auto lstn = std::make_shared<TcpListener>(exec, strand, tcpEndpoint,
                                              CodecIdSet{jsonId});
    Transporting::Ptr server;
    lstn->observe(
        [&](ListenResult result)
        {
            REQUIRE( result.ok() );
            server = lstn->take();
            server->admit(
                [](AdmitResult r) {REQUIRE(r.status() == AdmitStatus::wamp);});
        });
    lstn->establish();

    auto client = test::MockRawsockClient::create(ioctx, tcpTestPort);
    std::string payload{"Hello"};
    auto badFrameKind =
        static_cast<TransportFrameKind>(
            static_cast<int>(TransportFrameKind::pong) + 1);
    auto badHeader = RawsockHeader{}.setFrameKind(badFrameKind)
                                    .setLength(payload.size()).toHostOrder();
    client->load({{payload, badHeader}});
    client->connect();

    CHECK_NOTHROW( ioctx.run() );
    ioctx.restart();
    REQUIRE( server );

    WHEN( "the client sends an invalid message to the server" )
    {
        std::error_code serverError;
        client->start();

        server->start(
            [&](ErrorOr<MessageBuffer> message)
            {
                if (!message.has_value())
                    serverError = message.error();
                server->close();
            },
            nullptr);

        THEN( "the server obtains an error while receiving" )
        {
            CHECK_NOTHROW( ioctx.run() );
            CHECK( serverError == TransportErrc::badCommand );
        }
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Server sending an invalid message type", "[Transport][Rawsock]" )
{
GIVEN ( "A mock server that sends an invalid message type" )
{
    IoContext ioctx;
    auto exec = ioctx.get_executor();
    auto strand = boost::asio::make_strand(exec);

    auto server = test::MockRawsockServer::create(exec, tcpTestPort);
    auto badKind = static_cast<TransportFrameKind>(
        static_cast<int>(TransportFrameKind::pong) + 1);
    server->load({{"World", badKind}});
    server->start();

    auto cnct = std::make_shared<TcpConnector>(strand, tcpHost, jsonId);
    Transporting::Ptr client;
    cnct->establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            REQUIRE( transport.has_value() );
            client = std::move(*transport);
            ioctx.stop();
        });

    CHECK_NOTHROW( ioctx.run() );
    ioctx.restart();
    REQUIRE( server );
    REQUIRE( client );

    WHEN( "the server sends an invalid message to the client" )
    {
        std::error_code clientError;
        client->start(
            [&](ErrorOr<MessageBuffer> message)
            {
                if (!message.has_value())
                    clientError = message.error();
                server->close();
            },
            nullptr);


        auto msg = makeMessageBuffer("Hello");
        client->send(msg);

        THEN( "the client obtains an error while receiving" )
        {
            CHECK_NOTHROW( ioctx.run() );
            CHECK( clientError == TransportErrc::badCommand );
        }
    }
}
}

//------------------------------------------------------------------------------
TEST_CASE( "TCP rawsocket client pings", "[Transport][Rawsock]" )
{
    IoContext ioctx;
    auto exec = ioctx.get_executor();
    auto strand = boost::asio::make_strand(exec);
    boost::asio::steady_timer timer{ioctx};

    auto server = test::MockRawsockServer::create(exec, tcpTestPort);
    server->start();

    const std::chrono::milliseconds interval{50};
    const auto where = TcpHost{tcpLoopbackAddr, tcpTestPort}
                           .withHearbeatInterval(interval);
    auto cnct = std::make_shared<TcpConnector>(strand, where, jsonId);
    Transporting::Ptr client;
    cnct->establish(
        [&](ErrorOr<Transporting::Ptr> transport)
        {
            REQUIRE( transport.has_value() );
            client = *transport;
            ioctx.stop();
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

    // Wait the expected time for 3 ping/pong exchanges and check that
    // they actually occurred.
    timer.expires_after(3*interval + interval/2);
    timer.async_wait([&ioctx](boost::system::error_code) {ioctx.stop();});
    ioctx.run();
    ioctx.restart();

    CHECK(!clientError);
    auto serverSessions = server->sessions();
    REQUIRE(!serverSessions.empty());
    auto session = serverSessions.front().lock();
    REQUIRE_FALSE(!session);

    const auto& pings = session->pings();
    REQUIRE(pings.size() == 3);
    const auto& firstPing = pings.front();
    REQUIRE(firstPing.size() == 16);
    std::string transportId{firstPing.begin(), firstPing.begin() + 8};
    for (unsigned i=1; i<=pings.size(); ++i)
    {
        INFO("For ping #" << i);
        const auto& ping = pings.at(i-1);
        REQUIRE(ping.size() == 16);
        CHECK(std::equal(ping.begin(), ping.begin()+8,
                         transportId.begin(), transportId.end()));
        uint64_t sequenceNumber = 0;
        std::memcpy(&sequenceNumber, ping.data() + 8, 8);
        sequenceNumber = wamp::internal::endian::bigToNative64(sequenceNumber);
        CHECK(sequenceNumber == i);
    }

    // Make the server stop echoing the correct pong and check that the client
    // fails due to heartbeat timeout.
    session->setPong("ABC");
    timer.expires_after(2*interval);
    timer.async_wait([&ioctx](boost::system::error_code) {ioctx.stop();});
    ioctx.run();
    CHECK(clientError == TransportErrc::unresponsive);
}

//------------------------------------------------------------------------------
TEST_CASE( "TCP rawsocket server pongs", "[Transport][Rawsock]" )
{
    IoContext ioctx;
    auto exec = ioctx.get_executor();
    auto strand = boost::asio::make_strand(exec);
    boost::asio::steady_timer timer{ioctx};

    auto lstn = std::make_shared<TcpListener>(exec, strand, tcpEndpoint,
                                              CodecIdSet{jsonId});
    Transporting::Ptr server;
    lstn->observe(
        [&](ListenResult result)
        {
            REQUIRE( result.ok() );
            server = lstn->take();
            server->admit(
                [](AdmitResult r) {REQUIRE(r.status() == AdmitStatus::wamp);});
        });
    lstn->establish();

    auto client = test::MockRawsockClient::create(ioctx, tcpTestPort);
    std::vector<test::MockRawsockFrame> pings =
    {
        {"A",   TransportFrameKind::ping},
        {"BC",  TransportFrameKind::ping},
        {"DEF", TransportFrameKind::ping},
    };
    client->load(pings);
    client->connect();

    CHECK_NOTHROW( ioctx.run() );
    ioctx.restart();
    REQUIRE( server );

    std::error_code serverError;
    server->start(
        [&](ErrorOr<MessageBuffer> buf)
        {
            if (!buf.has_value())
                serverError = buf.error();
        },
        nullptr);

    client->start();

    while (client->inFrames().size() < pings.size())
        ioctx.poll();

    CHECK(!serverError);

    for (unsigned i=0; i<pings.size(); ++i)
    {
        INFO("For ping #" << i+1);
        const auto& ping = pings.at(i);
        const auto& frame = client->inFrames().at(i);
        auto header = RawsockHeader::fromBigEndian(frame.header);
        CHECK(header.frameKind() == TransportFrameKind::pong);
        CHECK(frame.payload == ping.payload);
    }

    server->close();
    ioctx.run();
}
