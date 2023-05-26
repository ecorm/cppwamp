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
TEST_CASE( "WAMP registration meta events", "[WAMP][Router][thisone]" )
{
    IoContext ioctx;
    Session s1{ioctx};
    Session s2{ioctx};

    SessionId registrationCreatedSessionId = 0;
    RegistrationInfo registrationInfo;
    SessionId registeredSessionId = 0;
    RegistrationId registrationId = 0;
    SessionId unregisteredSessionId = 0;
    RegistrationId unregisteredRegistrationId = 0;
    SessionId registrationDeletedSessionId = 0;
    RegistrationId registrationDeletatedRegistrationId = 0;

    auto onRegistrationCreated = [&](Event event)
    {
        event.convertTo(registrationCreatedSessionId,
                        registrationInfo);
    };

    auto onRegister = [&](Event event)
    {
        event.convertTo(registeredSessionId, registrationId);
    };

    auto onUnregister = [&](Event event)
    {
        event.convertTo(unregisteredSessionId, unregisteredRegistrationId);
    };

    auto onRegistrationDeleted = [&](Event event)
    {
        event.convertTo(registrationDeletedSessionId,
                        registrationDeletatedRegistrationId);
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

        while (registrationInfo.id == 0 || registrationId == 0)
            suspendCoro(yield);

        CHECK(registrationCreatedSessionId == welcome.sessionId());
        CHECK(registrationInfo.uri == "rpc");
        CHECK(registrationInfo.created > before);
        CHECK(registrationInfo.created < after);
        CHECK(registrationInfo.id == reg.id());
        CHECK(registrationInfo.matchPolicy == MatchPolicy::exact);
        CHECK(registrationInfo.invocationPolicy == InvocationPolicy::single);

        CHECK(registeredSessionId == welcome.sessionId());
        CHECK(registrationId == reg.id());

        reg.unregister();

        while (unregisteredRegistrationId == 0 ||
               registrationDeletatedRegistrationId == 0)
        {
            suspendCoro(yield);
        }

        CHECK(unregisteredSessionId == welcome.sessionId());
        CHECK(unregisteredRegistrationId == reg.id());
        CHECK(registrationDeletedSessionId == welcome.sessionId());
        CHECK(registrationDeletatedRegistrationId == reg.id());

        s2.disconnect();
        s1.disconnect();
    });

    ioctx.run();
}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
