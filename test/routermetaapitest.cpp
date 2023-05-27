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

namespace Matchers = Catch::Matchers;

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
TEST_CASE( "WAMP session meta events", "[WAMP][Router]" )
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
        CHECK(joinedInfo.authId       == welcome.authId());
        CHECK(joinedInfo.authMethod   == welcome.authMethod());
        CHECK(joinedInfo.authProvider == welcome.authProvider());
        CHECK(joinedInfo.authRole     == welcome.authRole());
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
TEST_CASE( "WAMP session meta procedures", "[WAMP][Router]" )
{
    using SessionIdList = std::vector<SessionId>;

    IoContext ioctx;
    Session s1{ioctx};
    Session s2{ioctx};

    s1.observeIncidents(
        [](Incident i) {std::cout << i.toLogEntry() << std::endl;});
    s1.enableTracing();

    std::vector<Incident> incidents;
    s2.observeIncidents([&incidents](Incident i) {incidents.push_back(i);});

    spawn(ioctx, [&](YieldContext yield)
    {
        s1.connect(withTcp, yield).value();
        auto w1 = s1.join(Petition(testRealm), yield).value();
        s2.connect(withTcp, yield).value();
        auto w2 = s2.join(Petition(testRealm), yield).value();
        std::vector<String> inclusiveAuthRoleList{{"anonymous"}};
        std::vector<String> exclusiveAuthRoleList{{"exclusive"}};

        {
            INFO("wamp.session.count");
            Rpc rpc{"wamp.session.count"};

            auto count = s1.call(rpc, yield).value();
            REQUIRE(count.args().size() == 1);
            CHECK(count.args().front().to<int>() == 2);

            count = s1.call(rpc.withArgs(inclusiveAuthRoleList), yield).value();
            REQUIRE(count.args().size() == 1);
            CHECK(count.args().front().to<int>() == 2);

            count = s1.call(rpc.withArgs(exclusiveAuthRoleList), yield).value();
            REQUIRE(count.args().size() == 1);
            CHECK(count.args().front().to<int>() == 0);
        }

        {
            INFO("wamp.session.list");
            Rpc rpc{"wamp.session.list"};
            SessionIdList list;
            SessionIdList allSessionIds{w1.sessionId(), w2.sessionId()};
            SessionIdList noSessionIds{};

            auto result = s1.call(rpc, yield).value();
            REQUIRE(result.args().size() == 1);
            result.convertTo(list);
            CHECK_THAT(list, Matchers::Contains(allSessionIds));

            result = s1.call(rpc.withArgs(inclusiveAuthRoleList), yield).value();
            REQUIRE(result.args().size() == 1);
            result.convertTo(list);
            CHECK_THAT(list, Matchers::Contains(allSessionIds));

            result = s1.call(rpc.withArgs(exclusiveAuthRoleList), yield).value();
            REQUIRE(result.args().size() == 1);
            result.convertTo(list);
            CHECK_THAT(list, Matchers::Contains(noSessionIds));
        }

        {
            INFO("wamp.session.get");
            Rpc rpc{"wamp.session.get"};
            SessionJoinInfo info;

            auto result = s1.call(rpc.withArgs(w2.sessionId()), yield).value();
            REQUIRE(result.args().size() == 1);
            result.convertTo(info);
            CHECK(info.authId == w2.authId());
            CHECK(info.authMethod == w2.authMethod());
            CHECK(info.authProvider == w2.authProvider());
            CHECK(info.authRole == w2.authRole());
            CHECK(info.sessionId == w2.sessionId());

            auto resultOrError = s1.call(rpc.withArgs(0), yield);
            REQUIRE_FALSE(resultOrError.has_value());
            CHECK(resultOrError.error() == WampErrc::noSuchSession);
        }

        {
            INFO("wamp.session.kill");
            Rpc rpc{"wamp.session.kill"};
            incidents.clear();

            auto errc = WampErrc::invalidArgument;
            auto reasonUri = errorCodeToUri(errc);
            s1.call(rpc.withArgs(w2.sessionId())
                        .withKwargs({{"reason", reasonUri},
                                     {"message", "because"}}),
                    yield).value();

            while (incidents.empty() || s2.state() == SessionState::established)
                suspendCoro(yield);

            CHECK((s2.state() == SessionState::closed ||
                   s2.state() == SessionState::failed));
            const auto& i = incidents.front();
            CHECK((i.kind() == IncidentKind::closedByPeer ||
                   i.kind() == IncidentKind::abortedByPeer));
            CHECK(i.error() == errc );
            bool messageFound = i.message().find("because") !=
                                std::string::npos;
            CHECK(messageFound);

            auto resultOrError = s1.call(rpc.withArgs(0), yield);
            REQUIRE_FALSE(resultOrError.has_value());
            CHECK(resultOrError.error() == WampErrc::noSuchSession);

            s2.disconnect();
            s2.connect(withTcp, yield).value();
            w2 = s2.join(Petition(testRealm), yield).value();
        }

        {
            INFO("wamp.session.kill_by_authid");
            Rpc rpc{"wamp.session.kill_by_authid"};
            SessionIdList list;
            auto errc = WampErrc::invalidArgument;
            auto reasonUri = errorCodeToUri(errc);
            incidents.clear();

            auto result = s1.call(rpc.withArgs("bogus"), yield).value();
            result.convertTo(list);
            CHECK(list.empty());

            result = s1.call(rpc.withArgs(w2.authId().value())
                                .withKwargs({{"reason", reasonUri},
                                             {"message", "because"}}),
                                  yield).value();
            result.convertTo(list);
            CHECK_THAT(list, Matchers::Contains(SessionIdList{w2.sessionId()}));

            while (incidents.empty() || s2.state() == SessionState::established)
                suspendCoro(yield);

            CHECK((s2.state() == SessionState::closed ||
                   s2.state() == SessionState::failed));
            const auto& i = incidents.front();
            CHECK((i.kind() == IncidentKind::closedByPeer ||
                   i.kind() == IncidentKind::abortedByPeer));
            CHECK(i.error() == errc );
            bool messageFound = i.message().find("because") !=
                                std::string::npos;
            CHECK(messageFound);

            s2.disconnect();
            s2.connect(withTcp, yield).value();
            w2 = s2.join(Petition(testRealm), yield).value();
        }

        // Crossbar does not exclude the caller, as the spec requires.
        // It also returns an array instead of an integer.
        // https://github.com/crossbario/crossbar/issues/2082
        if (test::RouterFixture::enabled())
        {
            INFO("wamp.session.kill_by_authrole");
            Rpc rpc{"wamp.session.kill_by_authrole"};
            int count;
            auto errc = WampErrc::invalidArgument;
            auto reasonUri = errorCodeToUri(errc);
            incidents.clear();

            auto result = s1.call(rpc.withArgs("bogus"), yield).value();
            result.convertTo(count);
            CHECK(count == 0);

            result = s1.call(rpc.withArgs(w2.authRole().value())
                                .withKwargs({{"reason", reasonUri},
                                             {"message", "because"}}),
                             yield).value();
            result.convertTo(count);
            CHECK(count == 1);

            while (incidents.empty() || s2.state() == SessionState::established)
                suspendCoro(yield);

            CHECK(s1.state() == SessionState::established);
            CHECK((s2.state() == SessionState::closed ||
                   s2.state() == SessionState::failed));
            const auto& i = incidents.front();
            CHECK((i.kind() == IncidentKind::closedByPeer ||
                   i.kind() == IncidentKind::abortedByPeer));
            CHECK(i.error() == errc );
            bool messageFound = i.message().find("because") != std::string::npos;
            CHECK(messageFound);

            s2.disconnect();
            s2.connect(withTcp, yield).value();
            w2 = s2.join(Petition(testRealm), yield).value();
        }

        // Crossbar does not currently implement wamp.session.kill_all
        // https://github.com/crossbario/crossbar/issues/1602
        if (test::RouterFixture::enabled())
        {
            INFO("wamp.session.kill_all");
            Rpc rpc{"wamp.session.kill_all"};
            int count = 0;
            auto errc = WampErrc::invalidArgument;
            auto reasonUri = errorCodeToUri(errc);
            incidents.clear();

            auto result = s1.call(rpc.withKwargs({{"reason", reasonUri},
                                                  {"message", "because"}}),
                                  yield).value();
            result.convertTo(count);
            CHECK(count == 1);

            while (incidents.empty() || s2.state() == SessionState::established)
                suspendCoro(yield);

            CHECK(s1.state() == SessionState::established);
            CHECK((s2.state() == SessionState::closed ||
                   s2.state() == SessionState::failed));
            const auto& i = incidents.front();
            CHECK((i.kind() == IncidentKind::closedByPeer ||
                   i.kind() == IncidentKind::abortedByPeer));
            CHECK(i.error() == errc );
            bool messageFound = i.message().find("because") !=
                                std::string::npos;
            CHECK(messageFound);
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
TEST_CASE( "WAMP subscription meta events", "[WAMP][Router]" )
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
