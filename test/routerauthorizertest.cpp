/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#if defined(CPPWAMP_TEST_HAS_CORO)

#include <catch2/catch.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/spawn.hpp>
#include <cppwamp/tcp.hpp>
#include "routerfixture.hpp"

using namespace wamp;

namespace
{

const std::string testRealm = "cppwamp.test-authorizer";
const unsigned short testPort = 12345;
const auto withTcp = TcpHost("localhost", testPort).withFormat(json);

//------------------------------------------------------------------------------
struct TestAuthorizer : public Authorizer
{
    TestAuthorizer()
        : topic("empty"), pub("empty"), proc("empty"), rpc("empty")
    {}

    void onAuthorize(Topic t, AuthorizationRequest a) override
    {
        topic = t;
        info = a.info();
        a.authorize(std::move(t), canSubscribe);
    }

    void onAuthorize(Pub p, AuthorizationRequest a) override
    {
        pub = p;
        info = a.info();
        a.authorize(std::move(p), canPublish);
    }

    void onAuthorize(Procedure p, AuthorizationRequest a) override
    {
        proc = p;
        info = a.info();
        a.authorize(std::move(p), canRegister);
    }

    void onAuthorize(Rpc r, AuthorizationRequest a) override
    {
        rpc = r;
        info = a.info();
        a.authorize(std::move(r), canCall);
    }

    void clear()
    {
        canSubscribe = true;
        canPublish = true;
        canRegister = true;
        canCall = true;

        topic = Topic{"empty"};
        pub = Pub{"empty"};
        proc = Procedure{"empty"};
        rpc = Rpc{"empty"};

        info = {};
    }

    Authorization canSubscribe;
    Authorization canPublish;
    Authorization canRegister;
    Authorization canCall;
    Topic topic;
    Pub pub;
    Procedure proc;
    Rpc rpc;
    SessionInfo info;
};

} // anomymous namespace


//------------------------------------------------------------------------------
TEST_CASE( "Router dynamic authorizer", "[WAMP][Router]" )
{
    if (!test::RouterFixture::enabled())
        return;

    wamp::Router& router = test::RouterFixture::instance().router();
    test::RouterLogLevelGuard logLevelGuard(router.logLevel());
    router.setLogLevel(LogLevel::error);

    IoContext ioctx;
    auto auth = std::make_shared<TestAuthorizer>();
    auth->bindExecutor(ioctx.get_executor());
    auto config =
        RealmConfig{testRealm}.withAuthorizer(auth)
                              .withCallerDisclosure(DisclosureRule::reveal)
                              .withPublisherDisclosure(DisclosureRule::conceal);
    test::ScopedRealm realm{router.openRealm(config).value()};

    Event event;
    auto onEvent = [&event](Event e) {event = std::move(e);};

    spawn(ioctx, [&](YieldContext yield)
    {
        Session s{ioctx};
        s.connect(withTcp, yield).value();
        auto welcome = s.join(testRealm, yield).value();

        {
            INFO("Subscribe authorized")
            auth->clear();
            auto sub = s.subscribe(Topic{"topic1"}, [](Event){}, yield);
            CHECK(auth->topic.uri() == "topic1");
            CHECK(auth->info.sessionId() == welcome.sessionId());
            CHECK(sub.has_value());
        }

        {
            INFO("Subscribe denied")
            auth->clear();
            auth->canSubscribe = false;
            auto sub = s.subscribe(Topic{"topic2"}, [](Event){}, yield);
            CHECK(auth->topic.uri() == "topic2");
            CHECK(auth->info.sessionId() == welcome.sessionId());
            REQUIRE_FALSE(sub.has_value());
            CHECK(sub.error() == WampErrc::authorizationDenied);
        }

        {
            INFO("Subscribe authorization failed")
            auth->clear();
            auth->canSubscribe = WampErrc::authorizationFailed;
            auto sub = s.subscribe(Topic{"topic3"}, [](Event){}, yield);
            CHECK(auth->topic.uri() == "topic3");
            CHECK(auth->info.sessionId() == welcome.sessionId());
            REQUIRE_FALSE(sub.has_value());
            CHECK(sub.error() == WampErrc::authorizationFailed);
        }

        {
            INFO("Subscribe denied with custom error")
            auth->clear();
            auth->canSubscribe = WampErrc::invalidUri;
            auto sub = s.subscribe(Topic{"topic4"}, [](Event){}, yield);
            CHECK(auth->topic.uri() == "topic4");
            CHECK(auth->info.sessionId() == welcome.sessionId());
            REQUIRE_FALSE(sub.has_value());
            CHECK(sub.error() == WampErrc::authorizationFailed);
        }

        {
            INFO("Publish authorized")
            auth->clear();
            event = {};
            ScopedSubscription sub =
                s.subscribe(Topic{"topic5"}, onEvent, yield).value();
            auto ack = s.publish(Pub{"topic5"}.withArgs(42)
                                              .withExcludeMe(false), yield);
            CHECK(ack.has_value());
            CHECK(auth->pub.uri() == "topic5");
            CHECK(auth->info.sessionId() == welcome.sessionId());

            while (event.args().empty())
                test::suspendCoro(yield);
            CHECK_FALSE(event.publisher().has_value());
        }

        {
            INFO("Publish denied")
            auth->clear();
            auth->canPublish = false;
            auto ack = s.publish(Pub{"topic6"}.withArgs(42), yield);
            REQUIRE_FALSE(ack.has_value());
            CHECK(ack.error() == WampErrc::authorizationDenied);
        }

        {
            INFO("Publish authorized with overriden disclosure rule")
            auth->clear();
            auth->canPublish =
                Authorization().withDisclosure(DisclosureRule::reveal);
            event = {};
            ScopedSubscription sub =
                s.subscribe(Topic{"topic7"}, onEvent, yield).value();
            auto ack = s.publish(Pub{"topic7"}.withArgs(42)
                                              .withExcludeMe(false), yield);
            CHECK(ack.has_value());
            CHECK(auth->pub.uri() == "topic7");
            CHECK(auth->info.sessionId() == welcome.sessionId());

            while (event.args().empty())
                test::suspendCoro(yield);
            CHECK(event.publisher() == welcome.sessionId());
        }

        {
            INFO("Publish failed due to overriden strict disclosure rule")
            auth->clear();
            auth->canPublish =
                Authorization().withDisclosure(DisclosureRule::strictConceal);
            event = {};
            auto ack = s.publish(Pub{"topic8"}.withArgs(42)
                                     .withExcludeMe(false)
                                     .withDiscloseMe(), yield);
            REQUIRE_FALSE(ack.has_value());
            CHECK(ack.error() == WampErrc::discloseMeDisallowed);
        }

        s.disconnect();
    });

    ioctx.run();
}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
