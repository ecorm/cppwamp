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
#include <cppwamp/transports/tcp.hpp>
#include "routerfixture.hpp"

using namespace wamp;

namespace
{

const std::string testRealm = "cppwamp.test-options";
const unsigned short testPort = 12345;
const auto withTcp = TcpHost("localhost", testPort).withFormat(json);

//------------------------------------------------------------------------------
void checkInvocationDisclosure(std::string info, Invocation& inv,
                               const Welcome& welcome, bool expectedDisclosed,
                               YieldContext yield)
{
    INFO(info);

    while (inv.args().empty())
        test::suspendCoro(yield);

    if (expectedDisclosed)
    {
        CHECK(inv.caller() == welcome.sessionId());
        CHECK(inv.callerAuthId() == welcome.authId());
        CHECK(inv.callerAuthRole() == welcome.authRole());
    }
    else
    {
        CHECK_FALSE(inv.caller().has_value());
        CHECK_FALSE(inv.callerAuthId().has_value());
        CHECK_FALSE(inv.callerAuthRole().has_value());
    }

    inv = Invocation{};
}

//------------------------------------------------------------------------------
void checkCallerDisclosure(
    std::string info, IoContext& ioctx, Disclosure policy,
    bool expectedDisclosedByDefault,
    bool expectedDisclosedWhenOriginatorReveals,
    bool expectedDisclosedWhenOriginatorConceals,
    bool expectedDisclosedByDefaultWhenCalleeRequestsDisclosure,
    bool expectedDisclosedWhenOriginatorRevealsAndCalleeRequestsDisclosure,
    bool expectedDisclosedWhenOriginatorConcealsAndCalleeRequestsDisclosure)
{
    INFO(info);

    auto options = RealmOptions{testRealm}.withCallerDisclosure(policy);

    wamp::Router& router = test::RouterFixture::instance().router();
    test::ScopedRealm realm{router.openRealm(options).value()};
    Session s{ioctx};
    Invocation invocation;
    auto onInvocation = [&invocation](Invocation i)
    {
        invocation = std::move(i);
        return Result{};
    };

    spawn(ioctx, [&](YieldContext yield)
    {
        s.connect(withTcp, yield).value();
        auto w = s.join(testRealm, yield).value();

        {
            INFO("With callee not requesting disclosure");
            auto rpc = Rpc{"rpc1"}.withArgs(42);
            s.enroll(Procedure{"rpc1"}, onInvocation, yield).value();

            s.call(rpc, yield).value();
            checkInvocationDisclosure("disclose_me unset", invocation, w,
                                      expectedDisclosedByDefault, yield);

            s.call(rpc.withDiscloseMe(), yield).value();
            checkInvocationDisclosure("disclose_me=true", invocation, w,
                                      expectedDisclosedWhenOriginatorReveals,
                                      yield);

            s.call(rpc.withDiscloseMe(false), yield).value();
            checkInvocationDisclosure("disclose_me=false", invocation, w,
                                      expectedDisclosedWhenOriginatorConceals,
                                      yield);
        }

        {
            INFO("With callee requesting disclosure");
            auto rpc = Rpc{"rpc2"}.withArgs(42);
            s.enroll(Procedure{"rpc2"}.withDiscloseCaller(),
                     onInvocation, yield).value();
            s.call(rpc, yield).value();
            checkInvocationDisclosure(
                "disclose_me unset", invocation, w,
                expectedDisclosedByDefaultWhenCalleeRequestsDisclosure, yield);

            s.call(rpc.withDiscloseMe(), yield).value();
            checkInvocationDisclosure(
                "disclose_me=true", invocation, w,
                expectedDisclosedWhenOriginatorRevealsAndCalleeRequestsDisclosure,
                yield);

            s.call(rpc.withDiscloseMe(false), yield).value();
            checkInvocationDisclosure(
                "disclose_me=false", invocation, w,
                expectedDisclosedWhenOriginatorConcealsAndCalleeRequestsDisclosure,
                yield);
        }

        s.disconnect();
    });

    ioctx.run();
    ioctx.restart();
}

//------------------------------------------------------------------------------
void checkEventDisclosure(std::string info, Event& event,
                          const Welcome& welcome, bool expectedDisclosed,
                          YieldContext yield)
{
    INFO(info);

    while (event.args().empty())
        test::suspendCoro(yield);

    if (expectedDisclosed)
    {
        CHECK(event.publisher() == welcome.sessionId());
        CHECK(event.publisherAuthId() == welcome.authId());
        CHECK(event.publisherAuthRole() == welcome.authRole());
    }
    else
    {
        CHECK_FALSE(event.publisher().has_value());
        CHECK_FALSE(event.publisherAuthId().has_value());
        CHECK_FALSE(event.publisherAuthRole().has_value());
    }

    event = Event{};
}

//------------------------------------------------------------------------------
void checkPublisherDisclosure(
    std::string info, IoContext& ioctx, Disclosure policy,
    bool expectedDisclosedByDefault,
    bool expectedDisclosedWhenOriginatorReveals,
    bool expectedDisclosedWhenOriginatorConceals)
{
    INFO(info);

    auto options = RealmOptions{testRealm}.withPublisherDisclosure(policy);

    wamp::Router& router = test::RouterFixture::instance().router();
    test::ScopedRealm realm{router.openRealm(options).value()};
    Session s{ioctx};
    Event event;
    auto onEvent = [&event](Event e) { event = std::move(e); };

    spawn(ioctx, [&](YieldContext yield)
    {
        auto pub = Pub{"topic"}.withExcludeMe(false).withArgs(42);

        s.connect(withTcp, yield).value();
        auto w = s.join(testRealm, yield).value();
        s.subscribe(Topic{"topic"}, onEvent, yield).value();

        s.publish(pub, yield).value();
        checkEventDisclosure("disclose_me unset", event, w,
                             expectedDisclosedByDefault, yield);

        s.publish(pub.withDiscloseMe(), yield).value();
        checkEventDisclosure("disclose_me=true", event, w,
                             expectedDisclosedWhenOriginatorReveals, yield);

        s.publish(pub.withDiscloseMe(false), yield).value();
        checkEventDisclosure("disclose_me=false", event, w,
                             expectedDisclosedWhenOriginatorConceals,
                             yield);

        s.disconnect();
    });

    ioctx.run();
    ioctx.restart();
}

} // anomymous namespace


//------------------------------------------------------------------------------
TEST_CASE( "Router call timeout forwarding options", "[WAMP][Router][Timeout]" )
{
    if (!test::RouterFixture::enabled())
        return;

    wamp::Router& router = test::RouterFixture::instance().router();
    IoContext ioctx;
    boost::asio::steady_timer timer{ioctx};
    Session s{ioctx};

    auto onCall = [&timer](Invocation inv) -> Outcome
    {
        auto timeout =
            inv.timeout().value_or(Invocation::CalleeTimeoutDuration{});

        if (timeout.count() != 0)
            return Result{timeout.count()};

        timer.expires_after(std::chrono::milliseconds(20));
        timer.async_wait(
            [inv](boost::system::error_code) mutable
            {
                inv.yield(Result{null});
            });
        return deferment;
    };

    auto runTest = [&](CallTimeoutForwardingRule rule,
                       bool expectedForwardedWhenAsked,
                       bool expectedForwardedWhenNotAsked)
    {
        auto options = RealmOptions{testRealm}
                          .withCallTimeoutForwardingRule(rule);
        test::ScopedRealm realm{router.openRealm(options).value()};

        spawn(ioctx, [&](YieldContext yield)
        {
            std::chrono::milliseconds timeout{10};
            s.connect(withTcp, yield).value();
            s.join(testRealm, yield).value();
            s.enroll(Procedure{"rpc1"}.withForwardTimeout(),
                     onCall, yield).value();
            auto result = s.call(Rpc{"rpc1"}.withDealerTimeout(timeout), yield);
            if (expectedForwardedWhenAsked)
            {
                REQUIRE(result.has_value());
                REQUIRE(result->args().size() == 1);
                CHECK(result->args().at(0) == timeout.count());
            }
            else
            {
                REQUIRE_FALSE(result.has_value());
                CHECK(result.error() == WampErrc::cancelled);
            }

            s.enroll(Procedure{"rpc2"}, onCall, yield).value();
            result = s.call(Rpc{"rpc2"}.withDealerTimeout(timeout), yield);
            if (expectedForwardedWhenNotAsked)
            {
                REQUIRE(result.has_value());
                REQUIRE(result->args().size() == 1);
                CHECK(result->args().at(0) == timeout.count());
            }
            else
            {
                REQUIRE_FALSE(result.has_value());
                CHECK(result.error() == WampErrc::cancelled);
            }

            s.disconnect();
        });

        ioctx.run();
    };

    SECTION("CallTimeoutForwardingRule::perRegistration")
    {
        runTest(CallTimeoutForwardingRule::perRegistration, true, false);
    }

    SECTION("CallTimeoutForwardingRule::perFeature")
    {
        runTest(CallTimeoutForwardingRule::perFeature, true, true);
    }

    SECTION("CallTimeoutForwardingRule::never")
    {
        runTest(CallTimeoutForwardingRule::never, false, false);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "Router disclosure options", "[WAMP][Router]" )
{
    if (!test::RouterFixture::enabled())
        return;

    IoContext io;
    using D = Disclosure;
    static constexpr bool y = true;
    static constexpr bool n = false;

    SECTION("Caller disclosure")
    {
        checkCallerDisclosure("preset",   io, D::preset,   n, y, n, n, y, n);
        checkCallerDisclosure("producer", io, D::producer, n, y, n, n, y, n);
        checkCallerDisclosure("consumer", io, D::consumer, n, n, n, y, y, y);
        checkCallerDisclosure("either",   io, D::either,   n, y, n, y, y, y);
        checkCallerDisclosure("both",     io, D::both,     n, n, n, n, y, n);
        checkCallerDisclosure("reveal",   io, D::reveal,   y, y, y, y, y, y);
        checkCallerDisclosure("conceal",  io, D::conceal,  n, n, n, n, n, n);
        io.stop();
    }

    SECTION("Publisher disclosure")
    {
        checkPublisherDisclosure("preset",   io, D::preset,   n, y, n);
        checkPublisherDisclosure("producer", io, D::producer, n, y, n);
        checkPublisherDisclosure("consumer", io, D::consumer, n, n, n);
        checkPublisherDisclosure("either",   io, D::either,   n, y, n);
        checkPublisherDisclosure("both",     io, D::both,     n, n, n);
        checkPublisherDisclosure("reveal",   io, D::reveal,   y, y, y);
        checkPublisherDisclosure("conceal",  io, D::conceal,  n, n, n);
        io.stop();
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "Router meta API enable options", "[WAMP][Router]" )
{
    if (!test::RouterFixture::enabled())
        return;

    wamp::Router& router = test::RouterFixture::instance().router();
    IoContext ioctx;
    Session s{ioctx};

    SECTION("Meta API disabled)")
    {
        auto options = RealmOptions{testRealm}.withMetaApiEnabled(false);
        test::ScopedRealm realm{router.openRealm(options).value()};

        spawn(ioctx, [&](YieldContext yield)
        {
            s.connect(withTcp, yield).value();
            s.join(testRealm, yield).value();
            auto result = s.call(Rpc{"wamp.session.count"}, yield);
            CHECK(result == makeUnexpectedError(WampErrc::noSuchProcedure));
            s.disconnect();
        });
        ioctx.run();
        ioctx.restart();
    }

    SECTION("Meta API enabled)")
    {
        auto options = RealmOptions{testRealm}.withMetaApiEnabled(true);
        test::ScopedRealm realm{router.openRealm(options).value()};

        spawn(ioctx, [&](YieldContext yield)
        {
            s.connect(withTcp, yield).value();
            s.join(testRealm, yield).value();
            auto result = s.call(Rpc{"wamp.session.count"}, yield);
            REQUIRE(result.has_value());
            REQUIRE(!result->args().empty());
            CHECK(result->args().at(0) == 1);
            s.disconnect();
        });
        ioctx.run();
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "Router challenge timeout option", "[WAMP][Router]" )
{
    if (!test::RouterFixture::enabled())
        return;

    IoContext ioctx;
    Session s{ioctx};

    spawn(ioctx, [&](YieldContext yield)
    {
        s.connect(TcpHost{"localhost", 23456}.withFormat(json), yield).value();
        auto petition = Petition{"cppwamp.authtest"}
                            .withAuthMethods({"ticket"})
                            .withAuthId("alice");
        auto welcome = s.join(petition, [](Challenge) {}, yield);
        REQUIRE_FALSE(welcome.has_value());
        CHECK(welcome.error() == WampErrc::timeout);
        s.disconnect();
    });
    ioctx.run();
}

//------------------------------------------------------------------------------
TEST_CASE( "Router connection limit option", "[WAMP][Router]" )
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
    ServerCloseGuard serverGuard{"tcp45678"};
    router.openServer(ServerOptions("tcp45678", wamp::TcpEndpoint{45678},
                                    wamp::json).withConnectionLimit(2));

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
    auto where = TcpHost{"localhost", 45678}.withFormat(json);

    spawn(ioctx, [&](YieldContext yield)
    {
        timer.expires_after(std::chrono::milliseconds(100));
        timer.async_wait(yield);
        s1.connect(where, yield).value();
        s2.connect(where, yield).value();
        auto w = s3.connect(where, yield);
        REQUIRE_FALSE(w.has_value());
        CHECK(w.error() == TransportErrc::overloaded);
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
