/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#if defined(CPPWAMP_TEST_HAS_CORO)

#include <catch2/catch.hpp>
#include <boost/asio/steady_timer.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/spawn.hpp>
#include <cppwamp/tcp.hpp>
#include "routerfixture.hpp"

using namespace wamp;

namespace
{

const std::string testRealm = "cppwamp.test-config";
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
    std::string info, IoContext& ioctx, DisclosurePolicy policy,
    bool expectedDisclosedByDefault,
    bool expectedDisclosedWhenOriginatorReveals,
    bool expectedDisclosedWhenOriginatorConceals)
{
    INFO(info);

    auto config = RealmConfig{testRealm}.withCallerDisclosure(policy);

    wamp::Router& router = test::RouterFixture::instance().router();
    test::ScopedRealm realm{router.openRealm(config).value()};
    Session s{ioctx};
    Invocation invocation;
    auto onInvocation = [&invocation](Invocation i)
    {
        invocation = std::move(i);
        return Result{};
    };

    spawn(ioctx, [&](YieldContext yield)
    {
        auto rpc = Rpc{"rpc"}.withArgs(42);
        s.connect(withTcp, yield).value();
        auto w = s.join(testRealm, yield).value();
        s.enroll(Procedure{"rpc"}, onInvocation, yield).value();
        s.call(rpc, yield).value();
        checkInvocationDisclosure("disclose_me unset", invocation, w,
                                  expectedDisclosedByDefault, yield);
        if (policy.producerDisclosureDisallowed())
        {
            auto ack = s.call(rpc.withDiscloseMe(), yield);
            CHECK(ack == makeUnexpectedError(WampErrc::discloseMeDisallowed));
        }
        else
        {
            s.call(rpc.withDiscloseMe(), yield).value();
            checkInvocationDisclosure("disclose_me=true", invocation, w,
                                      expectedDisclosedWhenOriginatorReveals,
                                      yield);
        }

        s.call(rpc.withDiscloseMe(false), yield).value();
        checkInvocationDisclosure("disclose_me=false", invocation, w,
                                  expectedDisclosedWhenOriginatorConceals,
                                  yield);
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
    std::string info, IoContext& ioctx, DisclosurePolicy policy,
    bool expectedDisclosedByDefault,
    bool expectedDisclosedWhenOriginatorReveals,
    bool expectedDisclosedWhenOriginatorConceals)
{
    INFO(info);

    auto config = RealmConfig{testRealm}.withPublisherDisclosure(policy);

    wamp::Router& router = test::RouterFixture::instance().router();
    test::ScopedRealm realm{router.openRealm(config).value()};
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

        if (policy.producerDisclosureDisallowed())
        {
            auto ack = s.publish(pub.withDiscloseMe(), yield);
            CHECK(ack == makeUnexpectedError(WampErrc::discloseMeDisallowed));
        }
        else
        {
            s.publish(pub.withDiscloseMe(), yield).value();
            checkEventDisclosure("disclose_me=true", event, w,
                                 expectedDisclosedWhenOriginatorReveals, yield);
        }

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
TEST_CASE( "Router call timeout forwarding config", "[WAMP][Router][Timeout]" )
{
    if (!test::RouterFixture::enabled())
        return;

    wamp::Router& router = test::RouterFixture::instance().router();
    test::RouterLogLevelGuard logLevelGuard(router.logLevel());
    router.setLogLevel(LogLevel::error);

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
        auto config = RealmConfig{testRealm}
                          .withCallTimeoutForwardingRule(rule);
        test::ScopedRealm realm{router.openRealm(config).value()};

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
TEST_CASE( "Router disclosure config", "[WAMP][Router]" )
{
    if (!test::RouterFixture::enabled())
        return;

    wamp::Router& router = test::RouterFixture::instance().router();
    test::RouterLogLevelGuard logLevelGuard(router.logLevel());
    router.setLogLevel(LogLevel::error);

    IoContext io;
    using D = Disclosure;
    static constexpr bool y = true;
    static constexpr bool n = false;
    const auto strictReveal = DisclosurePolicy{Disclosure::reveal}
                                  .withProducerDisclosureDisallowed();
    const auto strictConceal = DisclosurePolicy{Disclosure::conceal}
                                  .withProducerDisclosureDisallowed();

    SECTION("Caller disclosure")
    {
        checkCallerDisclosure("preset",        io, D::preset,     n, y, n);
        checkCallerDisclosure("producer",      io, D::producer,   n, y, n);
        checkCallerDisclosure("reveal",        io, D::reveal,     y, y, y);
        checkCallerDisclosure("conceal",       io, D::conceal,    n, n, n);
        checkCallerDisclosure("strictReveal",  io, strictReveal,  y, y, y);
        checkCallerDisclosure("strictConceal", io, strictConceal, n, n, n);
        io.stop();
    }

    SECTION("Publisher disclosure")
    {
        checkPublisherDisclosure("preset",        io, D::preset,     n, y, n);
        checkPublisherDisclosure("producer",      io, D::producer, n, y, n);
        checkPublisherDisclosure("reveal",        io, D::reveal,     y, y, y);
        checkPublisherDisclosure("conceal",       io, D::conceal,    n, n, n);
        checkPublisherDisclosure("strictReveal",  io, strictReveal,  y, y, y);
        checkPublisherDisclosure("strictConceal", io, strictConceal, n, n, n);
        io.stop();
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "Router meta API enable config", "[WAMP][Router]" )
{
    if (!test::RouterFixture::enabled())
        return;

    wamp::Router& router = test::RouterFixture::instance().router();
    test::RouterLogLevelGuard logLevelGuard(router.logLevel());
    router.setLogLevel(LogLevel::error);

    IoContext ioctx;
    Session s{ioctx};

    SECTION("Meta API disabled)")
    {
        auto config = RealmConfig{testRealm}.withMetaApiEnabled(false);
        test::ScopedRealm realm{router.openRealm(config).value()};

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
        auto config = RealmConfig{testRealm}.withMetaApiEnabled(true);
        test::ScopedRealm realm{router.openRealm(config).value()};

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

#endif // defined(CPPWAMP_TEST_HAS_CORO)
