/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#if defined(CPPWAMP_TEST_HAS_CORO)

#include <catch2/catch.hpp>
#include <boost/asio/steady_timer.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/spawn.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcpclient.hpp>
#include <cppwamp/transports/tcpserver.hpp>
#include "routerfixture.hpp"
#include "mockrawsockpeer.hpp"

using namespace wamp;

namespace
{

const std::string testRealm = "cppwamp.test-options";
const unsigned short testPort = 12345;
const auto withTcp = TcpHost("localhost", testPort).withFormat(json);

} // anomymous namespace


//------------------------------------------------------------------------------
TEST_CASE( "Router transport timeouts", "[WAMP][Router]" )
{
    using std::chrono::milliseconds;
    using FrameKind = TransportFrameKind;

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
    ServerCloseGuard serverGuard{"tcp45678"};

    // Not feasible to test write timeout without external software
    auto tcp = wamp::TcpEndpoint{45678}.withLimits(
        TcpEndpoint::Limits{}
            .withHandshakeTimeout( milliseconds{100})
            .withReadTimeout({     milliseconds{100}})
            .withSilenceTimeout(   milliseconds{200})
            .withLoiterTimeout(    milliseconds{300})
            .withLingerTimeout(    milliseconds{100}));

    router.openServer(
        ServerOptions("tcp45678", tcp, wamp::json)
            .withMonitoringInterval(milliseconds(50)));

    IoContext ioctx;
    std::vector<AccessLogEntry> logEntries;
    auto logSnoopGuard = routerFixture.snoopAccessLog(
        ioctx.get_executor(),
        [&logEntries](AccessLogEntry e)
        {
            if (!e.action.errorUri.empty())
                logEntries.push_back(e);}
        );
    auto logLevelGuard = routerFixture.supressLogLevel(LogLevel::critical);
    boost::asio::steady_timer timer{ioctx};
    auto client = test::MockRawsockClient::create(ioctx, 45678);

    {
        INFO("handshake timeout")

        client->inhibitHandshake(true);

        spawn(ioctx, [&](YieldContext yield)
        {
            client->connect();

            while (logEntries.empty() || !client->readError())
                test::suspendCoro(yield);
            CHECK(logEntries.back().action.errorUri ==
                  errorCodeToUri(make_error_code(TransportErrc::handshakeTimeout)));
            CHECK(client->readError() == boost::asio::error::eof);
            client->close();
        });
        ioctx.run();
        ioctx.restart();
    }

    {
        INFO("read timeout")

        logEntries.clear();
        client->clear();

        client->load(
        {
            {"[1,\"cppwamp.test\",{}]"},  // HELLO
            {"[32,", FrameKind::wamp, 16} // Incomplete SUBSCRIBE
        });

        spawn(ioctx, [&](YieldContext yield)
        {
            client->connect();
            while (!client->connected())
                test::suspendCoro(yield);
            client->start();

            while (logEntries.empty() || !client->readError())
                test::suspendCoro(yield);
            CHECK(logEntries.back().action.errorUri ==
                  errorCodeToUri(make_error_code(TransportErrc::readTimeout)));
            CHECK(client->readError() == boost::asio::error::eof);
            client->close();
        });
        ioctx.run();
        ioctx.restart();
    }

    {
        INFO("silence timeout")

        logEntries.clear();
        client->clear();

        client->load(
        {
            {"[1,\"cppwamp.test\",{}]"}, // HELLO
            {"[16,1,{\"acknowledge\":true},\"pub\"]", FrameKind::wamp}, // PUBLISH
            {"Heartbeat", FrameKind::ping, milliseconds{100}}
        });

        spawn(ioctx, [&](YieldContext yield)
        {
            client->connect();
            while (!client->connected())
                test::suspendCoro(yield);
            client->start();

            while (logEntries.empty() || !client->readError())
                test::suspendCoro(yield);
            CHECK(logEntries.back().action.errorUri ==
                  errorCodeToUri(
                      make_error_code(TransportErrc::silenceTimeout)));
            CHECK(client->readError() == boost::asio::error::eof);
            CHECK(client->inFrames().size() == 3);
            client->close();
        });
        ioctx.run();
        ioctx.restart();
    }

    {
        INFO("loiter timeout")

        logEntries.clear();
        client->clear();

        client->load(
        {
            {"[1,\"cppwamp.test\",{}]"}, // HELLO
            {"[16,1,{\"acknowledge\":true},\"pub\"]", FrameKind::wamp}, // PUBLISH
            {"Heartbeat1", FrameKind::ping, milliseconds{100}},
            {"Heartbeat2", FrameKind::ping, milliseconds{100}},
            {"Heartbeat3", FrameKind::ping, milliseconds{150}},
        });

        spawn(ioctx, [&](YieldContext yield)
        {
            client->connect();
            while (!client->connected())
                test::suspendCoro(yield);
            client->start();

            while (logEntries.empty() || !client->readError())
                test::suspendCoro(yield);
            CHECK(logEntries.back().action.errorUri ==
                  errorCodeToUri(
                      make_error_code(TransportErrc::loiterTimeout)));
            CHECK(client->readError() == boost::asio::error::eof);
            REQUIRE(client->inFrames().size() >= 4);
            CHECK(client->inFrames().at(3).payload == "Heartbeat2");
            if (client->inFrames().size() > 4)
            CHECK(client->inFrames().at(4).payload != "Heartbeat3");
            client->close();
        });
        ioctx.run();
        ioctx.restart();
    }

    {
        INFO("linger timeout via abort")

        logEntries.clear();
        client->clear();
        client->inhibitLingeringClose(true);
        client->load(
        {
            {"x"} // Malformed WAMP message
        });

        spawn(ioctx, [&](YieldContext yield)
        {
            client->connect();
            while (!client->connected())
                test::suspendCoro(yield);
            client->start();

            while (logEntries.size() < 2 || !client->readError())
              test::suspendCoro(yield);
            CHECK(logEntries.back().action.errorUri ==
                errorCodeToUri(make_error_code(TransportErrc::lingerTimeout)));
            CHECK(client->readError() == boost::asio::error::eof);
            client->close();
        });
        ioctx.run();
        ioctx.restart();
    }

    {
        INFO("linger timeout via admit rejection")

        logEntries.clear();
        client->clear();
        client->setHandshake(0xbad);
        client->inhibitLingeringClose(true);

        spawn(ioctx, [&](YieldContext yield)
        {
            client->connect();

            while (logEntries.size() < 2 || !client->readError())
                test::suspendCoro(yield);
            CHECK(logEntries.back().action.errorUri ==
                  errorCodeToUri(make_error_code(TransportErrc::lingerTimeout)));
            CHECK(client->readError() == boost::asio::error::eof);
            client->close();
        });
        ioctx.run();
        ioctx.restart();
    }
}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
