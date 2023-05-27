/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#if defined(CPPWAMP_TEST_HAS_CORO)

#include <catch2/catch.hpp>
#include <cppwamp/json.hpp>
#include "cppwamp/realmobserver.hpp"
#include <cppwamp/session.hpp>
#include <cppwamp/spawn.hpp>
#include <cppwamp/tcp.hpp>
#include "routerfixture.hpp"

using namespace wamp;

namespace
{

const std::string testRealm = "cppwamp.test";
const unsigned short testPort = 12345;
const auto withTcp = TcpHost("localhost", testPort).withFormat(json);

//------------------------------------------------------------------------------
inline void suspendCoro(YieldContext& yield)
{
    auto exec = boost::asio::get_associated_executor(yield);
    boost::asio::post(exec, yield);
}

//------------------------------------------------------------------------------
struct TestRealmObserver : public RealmObserver
{
    void onRealmClosed(const Uri& u) override
    {
        realmClosedEvents.push_back(u);
    }

    void onJoin(const SessionDetails& s) override
    {
        joinEvents.push_back(s);
    }

    void onLeave(const SessionDetails& s) override
    {
        leaveEvents.push_back(s);
    }

    void onRegister(const SessionDetails& s,
                    const RegistrationDetails& r) override
    {
        registerEvents.push_back({s, r});
    }

    void onUnregister(const SessionDetails& s,
                      const RegistrationDetails& r) override
    {
        unregisterEvents.push_back({s, r});
    }

    void onSubscribe(const SessionDetails& s,
                     const SubscriptionDetails& d) override
    {
        subscribeEvents.push_back({s, d});
    }

    void onUnsubscribe(const SessionDetails& s,
                       const SubscriptionDetails& d) override
    {
        unsubscribeEvents.push_back({s, d});
    }

    void clear()
    {
        realmClosedEvents.clear();
        joinEvents.clear();
        leaveEvents.clear();
        registerEvents.clear();
        unregisterEvents.clear();
        subscribeEvents.clear();
        unsubscribeEvents.clear();
    }

    std::vector<Uri> realmClosedEvents;
    std::vector<SessionDetails> joinEvents;
    std::vector<SessionDetails> leaveEvents;
    std::vector<std::pair<SessionDetails, RegistrationDetails>> registerEvents;
    std::vector<std::pair<SessionDetails, RegistrationDetails>> unregisterEvents;
    std::vector<std::pair<SessionDetails, SubscriptionDetails>> subscribeEvents;
    std::vector<std::pair<SessionDetails, SubscriptionDetails>> unsubscribeEvents;
};

} // anomymous namespace


//------------------------------------------------------------------------------
TEST_CASE( "WAMP session meta events", "[WAMP][Router][thisone]" )
{
    IoContext ioctx;
    Session s1{ioctx};
    Session s2{ioctx};

    SessionJoinInfo joinedInfo;
    SessionLeftInfo leftInfo;

    auto onJoin = [&joinedInfo](Event event)
    {
        event.convertTo(joinedInfo);
    };

    auto onLeave = [&leftInfo](Event event)
    {
        leftInfo = parseSessionLeftInfo(event);
    };

    spawn(ioctx, [&](YieldContext yield)
    {
        s1.connect(withTcp, yield).value();
        s1.join(Petition(testRealm), yield).value();
        s1.subscribe(Topic{"wamp.session.on_join"}, onJoin, yield).value();
        s1.subscribe(Topic{"wamp.session.on_leave"}, onLeave, yield).value();

        s2.connect(withTcp, yield).value();
        auto welcome = s2.join(Petition(testRealm), yield).value();

        while (joinedInfo.sessionId == 0)
            suspendCoro(yield);
        CHECK(joinedInfo.authid       == welcome.authId());
        CHECK(joinedInfo.authmethod   == welcome.authMethod());
        CHECK(joinedInfo.authprovider == welcome.authProvider());
        CHECK(joinedInfo.authrole     == welcome.authRole());
        CHECK(joinedInfo.sessionId    == welcome.sessionId());

        s2.leave(yield).value();

        while (leftInfo.sessionId == 0)
            suspendCoro(yield);
        CHECK(leftInfo.sessionId == welcome.sessionId());

        // Crossbar only provides session ID
        if (test::RouterFixture::enabled())
        {
            CHECK(leftInfo.authid == welcome.authId());
            CHECK(leftInfo.authrole == welcome.authRole());
        }

        s2.disconnect();
        s1.disconnect();
    });

    ioctx.run();
}

//------------------------------------------------------------------------------
TEST_CASE( "WAMP registration meta events", "[WAMP][Router]" )
{
    IoContext ioctx;
    Session s1{ioctx};
    Session s2{ioctx};

    SessionId regCreatedSessionId = 0;
    RegistrationInfo regInfo;
    SessionId registeredSessionId = 0;
    RegistrationId registrationId = 0;
    SessionId unregisteredSessionId = 0;
    RegistrationId unregisteredRegId = 0;
    SessionId regDeletedSessionId = 0;
    RegistrationId deletedRegistrationId = 0;

    auto onRegistrationCreated = [&](Event event)
    {
        event.convertTo(regCreatedSessionId, regInfo);
    };

    auto onRegister = [&](Event event)
    {
        event.convertTo(registeredSessionId, registrationId);
    };

    auto onUnregister = [&](Event event)
    {
        event.convertTo(unregisteredSessionId, unregisteredRegId);
    };

    auto onRegistrationDeleted = [&](Event event)
    {
        event.convertTo(regDeletedSessionId, deletedRegistrationId);
    };

    auto rpc = [](Invocation) -> Outcome {return {};};

    spawn(ioctx, [&](YieldContext yield)
    {
        namespace chrono = std::chrono;
        auto now = chrono::system_clock::now();
        auto before = now - chrono::seconds(60);
        auto after = now + chrono::seconds(60);

        s1.connect(withTcp, yield).value();
        s1.join(Petition(testRealm), yield).value();
        s1.subscribe(Topic{"wamp.registration.on_create"},
                     onRegistrationCreated, yield).value();
        s1.subscribe(Topic{"wamp.registration.on_register"}, onRegister,
                     yield).value();
        s1.subscribe(Topic{"wamp.registration.on_unregister"}, onUnregister,
                     yield).value();
        s1.subscribe(Topic{"wamp.registration.on_delete"},
                     onRegistrationDeleted, yield).value();

        s2.connect(withTcp, yield).value();
        auto welcome = s2.join(Petition(testRealm), yield).value();
        auto reg = s2.enroll(Procedure{"rpc"}, rpc, yield).value();
        while (regInfo.id == 0 || registrationId == 0)
            suspendCoro(yield);
        CHECK(regCreatedSessionId == welcome.sessionId());
        CHECK(regInfo.uri == "rpc");
        CHECK(regInfo.created > before);
        CHECK(regInfo.created < after);
        CHECK(regInfo.id == reg.id());
        CHECK(regInfo.matchPolicy == MatchPolicy::exact);
        CHECK(regInfo.invocationPolicy == InvocationPolicy::single);
        CHECK(registeredSessionId == welcome.sessionId());
        CHECK(registrationId == reg.id());

        reg.unregister();
        while (unregisteredRegId == 0 || deletedRegistrationId == 0)
            suspendCoro(yield);
        CHECK(unregisteredSessionId == welcome.sessionId());
        CHECK(unregisteredRegId == reg.id());
        CHECK(regDeletedSessionId == welcome.sessionId());
        CHECK(deletedRegistrationId == reg.id());

        s2.disconnect();
        s1.disconnect();
    });

    ioctx.run();
}

//------------------------------------------------------------------------------
TEST_CASE( "WAMP subscription meta events", "[WAMP][Router][thisone]" )
{
    IoContext ioctx;
    Session s1{ioctx};
    Session s2{ioctx};
    Session s3{ioctx};
    Session s4{ioctx};

    SessionId subCreatedSessionId = 0;
    SubscriptionInfo subInfo;
    SessionId subscribedSessionId = 0;
    RegistrationId subscriptionId = 0;
    SessionId unsubscribedSessionId = 0;
    RegistrationId unsubscribedSubId = 0;
    SessionId deletedSessionId = 0;
    RegistrationId deletedSubId = 0;

    auto onSubscriptionCreated = [&](Event event)
    {
        event.convertTo(subCreatedSessionId, subInfo);
    };

    auto onSubscribe = [&](Event event)
    {
        event.convertTo(subscribedSessionId, subscriptionId);
    };

    auto onUnsubscribe = [&](Event event)
    {
        event.convertTo(unsubscribedSessionId, unsubscribedSubId);
    };

    auto onSubDeleted = [&](Event event)
    {
        event.convertTo(deletedSessionId, deletedSubId);
    };

    spawn(ioctx, [&](YieldContext yield)
    {
        namespace chrono = std::chrono;
        auto now = chrono::system_clock::now();
        auto before = now - chrono::seconds(60);
        auto after = now + chrono::seconds(60);
        s1.connect(withTcp, yield).value();
        s1.join(Petition(testRealm), yield).value();
        s1.subscribe(Topic{"wamp.subscription.on_create"},
                     onSubscriptionCreated, yield).value();
        s1.subscribe(Topic{"wamp.subscription.on_subscribe"}, onSubscribe,
                     yield).value();
        s1.subscribe(Topic{"wamp.subscription.on_unsubscribe"}, onUnsubscribe,
                     yield).value();
        s1.subscribe(Topic{"wamp.subscription.on_delete"},
                     onSubDeleted, yield).value();

        s2.connect(withTcp, yield).value();
        auto welcome2 = s2.join(Petition(testRealm), yield).value();
        auto sub2 = s2.subscribe(Topic{"exact"}, [](Event) {}, yield).value();

        while (subInfo.id == 0 || subscriptionId == 0)
            suspendCoro(yield);
        CHECK(subCreatedSessionId == welcome2.sessionId());
        CHECK(subInfo.uri == "exact");
        CHECK(subInfo.created > before);
        CHECK(subInfo.created < after);
        CHECK(subInfo.id == sub2.id());
        CHECK(subInfo.matchPolicy == MatchPolicy::exact);
        CHECK(subscribedSessionId == welcome2.sessionId());
        CHECK(subscriptionId == sub2.id());

        subInfo.id = 0;
        subscriptionId = 0;
        s3.connect(withTcp, yield).value();
        auto welcome3 = s3.join(Petition(testRealm), yield).value();
        auto sub3 = s3.subscribe(
            Topic{"prefix"}.withMatchPolicy(MatchPolicy::prefix),
            [](Event) {},
            yield).value();

        while (subInfo.id == 0 || subscriptionId == 0)
            suspendCoro(yield);
        CHECK(subCreatedSessionId == welcome3.sessionId());
        CHECK(subInfo.created > before);
        CHECK(subInfo.created < after);
        CHECK(subInfo.uri == "prefix");
        CHECK(subInfo.id == sub3.id());
        CHECK(subInfo.matchPolicy == MatchPolicy::prefix);
        CHECK(subscribedSessionId == welcome3.sessionId());
        CHECK(subscriptionId == sub3.id());

        subscriptionId = 0;
        s4.connect(withTcp, yield).value();
        auto welcome4 = s4.join(Petition(testRealm), yield).value();
        auto sub4 = s4.subscribe(
            Topic{"prefix"}.withMatchPolicy(MatchPolicy::prefix),
            [](Event) {},
            yield).value();

        while (subscriptionId == 0) suspendCoro(yield);
        CHECK(subscribedSessionId == welcome4.sessionId());
        CHECK(subscriptionId == sub4.id());

        sub3.unsubscribe();
        while (unsubscribedSubId == 0) {suspendCoro(yield);}
        CHECK(unsubscribedSessionId == welcome3.sessionId());
        CHECK(unsubscribedSubId == sub3.id());
        CHECK(deletedSessionId == 0);
        CHECK(deletedSubId == 0);

        unsubscribedSubId = 0;
        sub4.unsubscribe();
        while (unsubscribedSubId == 0 || deletedSubId == 0)
            suspendCoro(yield);
        CHECK(unsubscribedSessionId == welcome4.sessionId());
        CHECK(unsubscribedSubId == sub4.id());
        CHECK(deletedSessionId == welcome4.sessionId());
        CHECK(deletedSubId == sub4.id());

        unsubscribedSubId = 0;
        deletedSubId = 0;
        sub2.unsubscribe();
        while (unsubscribedSubId == 0 || deletedSubId == 0)
            suspendCoro(yield);
        CHECK(unsubscribedSessionId == welcome2.sessionId());
        CHECK(unsubscribedSubId == sub2.id());
        CHECK(deletedSessionId == welcome2.sessionId());
        CHECK(deletedSubId == sub2.id());

        s4.disconnect();
        s3.disconnect();
        s2.disconnect();
        s1.disconnect();
    });

    ioctx.run();
}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
