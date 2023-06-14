/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#if defined(CPPWAMP_TEST_HAS_CORO)

#include <catch2/catch.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <cppwamp/asiodefs.hpp>
#include <cppwamp/json.hpp>
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
    static std::shared_ptr<TestRealmObserver> create()
    {
        return std::make_shared<TestRealmObserver>();
    }

    void onRealmClosed(Uri u) override
    {
        realmClosedEvents.push_back(std::move(u));
    }

    void onJoin(SessionInfo::ConstPtr s) override
    {
        joinEvents.push_back(*s);
    }

    void onLeave(SessionInfo::ConstPtr s) override
    {
        leaveEvents.push_back(*s);
    }

    void onRegister(SessionInfo::ConstPtr s, RegistrationInfo r) override
    {
        registerEvents.push_back({*s, std::move(r)});
    }

    void onUnregister(SessionInfo::ConstPtr s, RegistrationInfo r) override
    {
        unregisterEvents.push_back({*s, std::move(r)});
    }

    void onSubscribe(SessionInfo::ConstPtr s, SubscriptionInfo i) override
    {
        subscribeEvents.push_back({*s, std::move(i)});
    }

    void onUnsubscribe(SessionInfo::ConstPtr s, SubscriptionInfo i) override
    {
        unsubscribeEvents.push_back({*s, std::move(i)});
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
    std::vector<SessionInfo> joinEvents;
    std::vector<SessionInfo> leaveEvents;
    std::vector<std::pair<SessionInfo, RegistrationInfo>> registerEvents;
    std::vector<std::pair<SessionInfo, RegistrationInfo>> unregisterEvents;
    std::vector<std::pair<SessionInfo, SubscriptionInfo>> subscribeEvents;
    std::vector<std::pair<SessionInfo, SubscriptionInfo>> unsubscribeEvents;
};

//------------------------------------------------------------------------------
struct RouterLogLevelGuard
{
    explicit RouterLogLevelGuard(LogLevel level) : level_(level) {}

    ~RouterLogLevelGuard()
    {
        test::RouterFixture::instance().router().setLogLevel(level_);
    }

private:
    LogLevel level_;
};

//------------------------------------------------------------------------------
void checkSessionDetails(const SessionInfo& s, const Welcome& w,
                         const Uri& realmUri)
{
    CHECK(s.realmUri() == realmUri);
    CHECK(s.auth().id() == w.authId());
    CHECK(s.auth().role() == w.authRole());
    CHECK(s.auth().method() == w.authMethod());
    CHECK(s.auth().provider() == w.authProvider());
    CHECK(s.sessionId() == w.sessionId());
    CHECK(s.features().supports(ClientFeatures::provided()));

    auto t = s.transport();
    CHECK(t["protocol"] == String{"TCP"});
    CHECK(t["server"] == String{"tcp12345"});
    auto ipv = t["ip_version"];
    CHECK((ipv == 4 || ipv == 6));
    CHECK(wamp::isNumber(t["port"]));
    auto addr = t["address"];
    REQUIRE(addr.is<String>());
    CHECK_FALSE(addr.as<String>().empty());
    if (ipv == 4)
        CHECK(wamp::isNumber(t["numeric_address"]));
}

//------------------------------------------------------------------------------
void checkRegistrationDetails(
    const RegistrationInfo& r,
    const Uri& uri,
    std::chrono::system_clock::time_point when,
    RegistrationId rid,
    std::set<SessionId> callees)
{
    std::chrono::seconds margin(60);
    CHECK(r.uri == uri);
    CHECK(r.created > (when - margin));
    CHECK(r.created < (when + margin));
    CHECK(r.id == rid);
    CHECK(r.matchPolicy == MatchPolicy::exact);
    CHECK(r.invocationPolicy == InvocationPolicy::single);
    CHECK(r.callees == callees);
}

//------------------------------------------------------------------------------
void checkSubscriptionInfo(
    const SubscriptionInfo& s,
    const Uri& uri,
    std::chrono::system_clock::time_point when,
    SubscriptionId subId,
    std::set<SessionId> subscribers)
{
    std::chrono::seconds margin(60);
    CHECK(s.uri == uri);
    CHECK(s.created > (when - margin));
    CHECK(s.created < (when + margin));
    CHECK(s.id == subId);
    CHECK(s.matchPolicy == MatchPolicy::exact);
    CHECK(s.subscribers == subscribers);
}

//------------------------------------------------------------------------------
void checkRealmSessions(const std::string& info, Realm& realm,
                        std::vector<Welcome> expected)
{
    INFO(info);

    std::vector<SessionId> sidList;
    for (const auto& w: expected)
        sidList.push_back(w.sessionId());
    auto sessionCount = sidList.size();

    // Realm::sessionCount
    CHECK(realm.sessionCount() == sessionCount);

    // Realm::forEachSession
    std::map<SessionId, SessionInfo> sessionInfos;
    auto n = realm.forEachSession(
        [&](const SessionInfo& s) -> bool
        {
            auto sid = s.sessionId();
            sessionInfos.emplace(sid, s);
            return true;
        });
    CHECK(n == sessionCount);
    REQUIRE(sessionInfos.size() == expected.size());
    for (const auto& w: expected)
    {
        auto sid = w.sessionId();
        REQUIRE(sessionInfos.count(sid) != 0);
        checkSessionDetails(sessionInfos[sid], w, realm.uri());
    }

    // Realm::lookupSession
    for (const auto& w: expected)
    {
        auto sid = w.sessionId();
        auto errorOrDetails = realm.lookupSession(sid);
        REQUIRE(errorOrDetails.has_value());
        checkSessionDetails(**errorOrDetails, w, realm.uri());
    }
}

//------------------------------------------------------------------------------
void checkSessionKilled(const std::string& info, Session& session,
                        std::vector<Incident>& incidents, WampErrc errc,
                        YieldContext& yield)
{
    INFO(info);
    while (incidents.empty() || session.state() == SessionState::established)
        suspendCoro(yield);
    CHECK(session.state() == SessionState::failed);
    CHECK(incidents.size() == 1);
    CHECK(incidents.front().kind() == IncidentKind::abortedByPeer);
    CHECK(incidents.front().error() == errc);
    incidents.clear();
}

} // anomymous namespace


//------------------------------------------------------------------------------
TEST_CASE( "Router realm management", "[WAMP][Router]" )
{
    if (!test::RouterFixture::enabled())
        return;

    auto& theRouter = test::RouterFixture::instance().router();
    RouterLogLevelGuard logLevelGuard{theRouter.logLevel()};
    theRouter.setLogLevel(LogLevel::off);
    IoContext ioctx;

    spawn(ioctx, [&](YieldContext yield)
    {
        {
            INFO("Opening already open realm");
            auto realmOrError = theRouter.openRealm({"cppwamp.test"});
            CHECK(realmOrError == makeUnexpectedError(MiscErrc::alreadyExists));
        }

        {
            INFO("Closing non-existing realm");
            bool closed = theRouter.closeRealm("bogus");
            CHECK_FALSE(closed);
        }

        {
            INFO("Accessing non-existing realm");
            auto found = theRouter.realmAt("bogus");
            REQUIRE_FALSE(found.has_value());
            CHECK(found == makeUnexpectedError(WampErrc::noSuchRealm));
        }

        {
            INFO("Opening, accessing, and closing a realm");
            Uri uri{"cppwamp.test2"};
            auto realmOrError = theRouter.openRealm({uri});
            REQUIRE(realmOrError.has_value());

            Realm realm;
            CHECK_FALSE(bool(realm));
            CHECK_FALSE(realm.isAttached());
            realm = realmOrError.value();
            CHECK(bool(realm));
            CHECK(realm.isOpen());
            CHECK(realm.uri() == uri);

            auto found = theRouter.realmAt(uri, ioctx.get_executor());
            REQUIRE(found.has_value());
            CHECK(found->isOpen());
            CHECK(found->uri() == uri);
            CHECK(found->fallbackExecutor() == ioctx.get_executor());

            auto observer = TestRealmObserver::create();
            realm.observe(observer);

            bool closed = theRouter.closeRealm(uri);
            CHECK(closed);

            while (realm.isOpen() || observer->realmClosedEvents.empty())
                suspendCoro(yield);
            CHECK_THAT(observer->realmClosedEvents,
                       Matchers::Equals(std::vector<Uri>({uri})));

            realm = {};
            CHECK_FALSE(bool(realm));
            CHECK_FALSE(realm.isAttached());

            ioctx.stop();
        }
    });

    ioctx.run();
}

//------------------------------------------------------------------------------
TEST_CASE( "Router realm session events", "[WAMP][Router]" )
{
    if (!test::RouterFixture::enabled())
        return;

    auto& theRouter = test::RouterFixture::instance().router();
    RouterLogLevelGuard logLevelGuard{theRouter.logLevel()};
    theRouter.setLogLevel(LogLevel::off);

    IoContext ioctx;
    Session s{ioctx};
    Welcome welcome;

    auto observer = TestRealmObserver::create();
    auto realm = theRouter.realmAt(testRealm, ioctx.get_executor()).value();
    REQUIRE(realm.fallbackExecutor() == ioctx.get_executor());
    realm.observe(observer);

    spawn(ioctx, [&](YieldContext yield)
    {
        {
            INFO("Session joining");
            s.connect(withTcp, yield).value();
            welcome = s.join(Petition(testRealm), yield).value();

            while (observer->joinEvents.empty())
                suspendCoro(yield);
            REQUIRE(observer->joinEvents.size() == 1);
            const auto& joined = observer->joinEvents.front();
            checkSessionDetails(joined, welcome, realm.uri());
        }

        {
            INFO("Session leaving");
            s.leave(yield).value();

            while (observer->leaveEvents.empty())
                suspendCoro(yield);
            REQUIRE(observer->leaveEvents.size() == 1);
            const auto& left = observer->leaveEvents.front();
            checkSessionDetails(left, welcome, realm.uri());
        }

        // TODO: Multiple observers
        // TODO: RealmObserver::detach

        s.disconnect();
    });

    ioctx.run();
}

//------------------------------------------------------------------------------
TEST_CASE( "Router realm session queries", "[WAMP][Router]" )
{
    if (!test::RouterFixture::enabled())
        return;

    auto& theRouter = test::RouterFixture::instance().router();
    RouterLogLevelGuard logLevelGuard{theRouter.logLevel()};
    theRouter.setLogLevel(LogLevel::off);

    IoContext ioctx;
    auto guard = make_work_guard(ioctx);

    spawn(ioctx, [&](YieldContext yield)
    {
        auto realm = theRouter.realmAt(testRealm, ioctx.get_executor()).value();
        REQUIRE(realm.fallbackExecutor() == ioctx.get_executor());

        checkRealmSessions("No sessions joined yet", realm, {});

        Session s1{ioctx};
        s1.connect(withTcp, yield).value();
        Welcome w1 = s1.join(Petition(testRealm), yield).value();
        checkRealmSessions("s1 joined", realm, {w1});

        Session s2{ioctx};
        s2.connect(withTcp, yield).value();
        Welcome w2 = s2.join(Petition(testRealm), yield).value();
        checkRealmSessions("s2 joined", realm, {w1, w2});

        auto errorOrDetails = realm.lookupSession(0);
        CHECK(errorOrDetails ==
              makeUnexpectedError(WampErrc::noSuchSession));

        s1.leave(yield).value();
        checkRealmSessions("s1 left", realm, {w2});

        s2.leave(yield).value();
        checkRealmSessions("s2 left", realm, {});

        s2.disconnect();
        s1.disconnect();
        guard.reset();
    });

    ioctx.run();
}

//------------------------------------------------------------------------------
TEST_CASE( "Killing router sessions", "[WAMP][Router]" )
{
    if (!test::RouterFixture::enabled())
        return;

    auto& theRouter = test::RouterFixture::instance().router();
    RouterLogLevelGuard logLevelGuard{theRouter.logLevel()};
    theRouter.setLogLevel(LogLevel::off);

    IoContext ioctx;
    auto guard = make_work_guard(ioctx);

    spawn(ioctx, [&](YieldContext yield)
    {
        auto any = [](const SessionInfo&) {return true;};
        auto none = [](const SessionInfo&) {return false;};

        auto realm = theRouter.realmAt(testRealm, ioctx.get_executor()).value();
        REQUIRE(realm.fallbackExecutor() == ioctx.get_executor());
        auto observer = TestRealmObserver::create();
        realm.observe(observer);

        Session s1{ioctx};
        Welcome w1;
        std::vector<Incident> i1;
        s1.observeIncidents([&i1](Incident i) {i1.push_back(i);});
        s1.connect(withTcp, yield).value();
        w1 = s1.join(Petition(testRealm), yield).value();

        Session s2{ioctx};
        Welcome w2;
        std::vector<Incident> i2;
        s2.observeIncidents([&i2](Incident i) {i2.push_back(i);});
        s2.connect(withTcp, yield).value();
        w2 = s2.join(Petition(testRealm), yield).value();

        {
            INFO("Realm::killSessionById - non-existent");
            auto errorOrDone = realm.killSessionById(0, yield);
            CHECK(errorOrDone == makeUnexpectedError(WampErrc::noSuchSession));
            CHECK(i1.empty());
        }

        {
            INFO("Realm::killSessionById");
            auto errc = WampErrc::invalidArgument;
            auto errorOrDone = realm.killSessionById(w1.sessionId(), {errc}, yield);
            REQUIRE(errorOrDone.has_value());
            CHECK(errorOrDone.value() == true);
            checkSessionKilled("s1", s1, i1, errc, yield);

            s1.disconnect();
            s1.connect(withTcp, yield).value();
            w1 = s1.join(Petition(testRealm), yield).value();
        }

        {
            INFO("Realm::killSessions - no matches");
            auto list = realm.killSessions(none, yield);
            CHECK(list.empty());
            CHECK(i1.empty());
            CHECK(i2.empty());
        }

        {
            INFO("Realm::killSessions - with matches");
            auto list = realm.killSessions(any, yield);
            std::vector<SessionId> expectedIds = {w1.sessionId(),
                                                  w2.sessionId()};
            CHECK_THAT(list, Matchers::UnorderedEquals(expectedIds));
            checkSessionKilled("s1", s1, i1, WampErrc::sessionKilled, yield);
            checkSessionKilled("s2", s2, i2, WampErrc::sessionKilled, yield);

            s1.disconnect();
            s1.connect(withTcp, yield).value();
            w1 = s1.join(Petition(testRealm), yield).value();
            s2.disconnect();
            s2.connect(withTcp, yield).value();
            w2 = s2.join(Petition(testRealm), yield).value();
        }

        s2.disconnect();
        s1.disconnect();
        guard.reset();
    });

    ioctx.run();
}

//------------------------------------------------------------------------------
TEST_CASE( "Router realm registration queries and events", "[WAMP][Router]" )
{
    // TODO: Queries

    if (!test::RouterFixture::enabled())
        return;

    auto& theRouter = test::RouterFixture::instance().router();
    RouterLogLevelGuard logLevelGuard{theRouter.logLevel()};
    theRouter.setLogLevel(LogLevel::off);

    IoContext ioctx;
    Session s{ioctx};

    auto observer = TestRealmObserver::create();
    auto realm = theRouter.realmAt(testRealm, ioctx.get_executor()).value();
    REQUIRE(realm.fallbackExecutor() == ioctx.get_executor());
    realm.observe(observer);

    spawn(ioctx, [&](YieldContext yield)
    {
        s.connect(withTcp, yield).value();
        auto welcome = s.join(Petition(testRealm), yield).value();
        std::chrono::system_clock::time_point when;
        Registration reg;

        {
            INFO("Registration");
            reg = s.enroll(Procedure{"foo"},
                                [](Invocation) -> Outcome {return {};},
                                yield).value();
            when = std::chrono::system_clock::now();

            while (observer->registerEvents.empty())
                suspendCoro(yield);
            REQUIRE(observer->registerEvents.size() == 1);
            const auto& ev = observer->registerEvents.front();
            checkSessionDetails(ev.first, welcome, testRealm);
            checkRegistrationDetails(ev.second, "foo", when, reg.id(),
                                     {welcome.sessionId()});
        }

        {
            INFO("Unregistration");
            s.unregister(reg, yield).value();

            while (observer->unregisterEvents.empty())
                suspendCoro(yield);
            REQUIRE(observer->unregisterEvents.size() == 1);
            const auto& ev = observer->unregisterEvents.front();
            checkSessionDetails(ev.first, welcome, testRealm);
            checkRegistrationDetails(ev.second, "foo", when, reg.id(), {});
        }

        {
            INFO("Unregistration via leaving");
            reg = s.enroll(Procedure{"foo"},
                       [](Invocation) -> Outcome {return {};},
                       yield).value();
            observer->unregisterEvents.clear();
            s.leave(yield).value();

            while (observer->unregisterEvents.empty())
                suspendCoro(yield);
            REQUIRE(observer->unregisterEvents.size() == 1);
            const auto& ev = observer->unregisterEvents.front();
            checkSessionDetails(ev.first, welcome, testRealm);
            checkRegistrationDetails(ev.second, "foo", when, reg.id(), {});
        }

        s.disconnect();
    });

    ioctx.run();
}

//------------------------------------------------------------------------------
TEST_CASE( "Router realm subscription queries and events", "[WAMP][Router]" )
{
    // TODO: Queries

    if (!test::RouterFixture::enabled())
        return;

    auto& theRouter = test::RouterFixture::instance().router();
    RouterLogLevelGuard logLevelGuard{theRouter.logLevel()};
    theRouter.setLogLevel(LogLevel::off);

    IoContext ioctx;
    Session s1{ioctx};
    Session s2{ioctx};

    auto observer = TestRealmObserver::create();
    auto realm = theRouter.realmAt(testRealm, ioctx.get_executor()).value();
    REQUIRE(realm.fallbackExecutor() == ioctx.get_executor());
    realm.observe(observer);

    spawn(ioctx, [&](YieldContext yield)
    {
        s1.connect(withTcp, yield).value();
        auto w1 = s1.join(Petition(testRealm), yield).value();
        s2.connect(withTcp, yield).value();
        auto w2 = s2.join(Petition(testRealm), yield).value();
        std::chrono::system_clock::time_point when;
        Subscription sub1;
        Subscription sub2;

        {
            INFO("Subscription");
            sub1 = s1.subscribe(Topic{"foo"}, [](Event) {}, yield).value();
            when = std::chrono::system_clock::now();

            while (observer->subscribeEvents.empty())
                suspendCoro(yield);
            REQUIRE(observer->subscribeEvents.size() == 1);
            const auto& ev = observer->subscribeEvents.front();
            checkSessionDetails(ev.first, w1, testRealm);
            checkSubscriptionInfo(ev.second, "foo", when, sub1.id(),
                                  {w1.sessionId()});
            observer->subscribeEvents.clear();
        }

        {
            INFO("Another subscription to same topic");
            sub2 = s2.subscribe(Topic{"foo"}, [](Event) {}, yield).value();
            when = std::chrono::system_clock::now();

            while (observer->subscribeEvents.empty())
                suspendCoro(yield);
            REQUIRE(observer->subscribeEvents.size() == 1);
            const auto& ev = observer->subscribeEvents.front();
            checkSessionDetails(ev.first, w2, testRealm);
            checkSubscriptionInfo(ev.second, "foo", when, sub2.id(),
                                  {w1.sessionId(), w2.sessionId()});
            observer->subscribeEvents.clear();
        }

        {
            INFO("Unsubscription leaving leaving");
            s1.leave(yield).value();

            while (observer->unsubscribeEvents.empty())
                suspendCoro(yield);
            REQUIRE(observer->unsubscribeEvents.size() == 1);
            const auto& ev = observer->unsubscribeEvents.front();
            checkSessionDetails(ev.first, w1, testRealm);
            checkSubscriptionInfo(ev.second, "foo", when, sub1.id(),
                                  {w2.sessionId()});
            observer->unsubscribeEvents.clear();
        }

        {
            INFO("Final unsubscription");
            s2.unsubscribe(sub2, yield).value();

            while (observer->unsubscribeEvents.empty())
                suspendCoro(yield);
            REQUIRE(observer->unsubscribeEvents.size() == 1);
            const auto& ev = observer->unsubscribeEvents.front();
            checkSessionDetails(ev.first, w2, testRealm);
            checkSubscriptionInfo(ev.second, "foo", when, sub2.id(), {});
        }

        s2.disconnect();
        s1.disconnect();
    });

    ioctx.run();
}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
