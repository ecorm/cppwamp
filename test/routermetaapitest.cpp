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
TEST_CASE( "WAMP meta events", "[WAMP][Router][thisone]" )
{
    IoContext ioctx;
    Session s1{ioctx};
    Session s2{ioctx};

    s1.observeIncidents(
        [](Incident i) {std::cout << i.toLogEntry() << std::endl;});
    s1.enableTracing();

    SECTION("Session meta events")
    {
        SessionJoinInfo joinedInfo;
        joinedInfo.sessionId = 0;

        SessionLeftInfo leftInfo;
        leftInfo.sessionId = 0;

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
            CHECK(joinedInfo.sessionId    == welcome.id());

            s2.leave(yield).value();

            while (leftInfo.sessionId == 0)
                suspendCoro(yield);
            CHECK(leftInfo.sessionId == welcome.id());

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
}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
