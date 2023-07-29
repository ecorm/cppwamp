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
#include <cppwamp/internal/timeformatting.hpp>
#include "routerfixture.hpp"

using namespace wamp;

namespace Matchers = Catch::Matchers;

namespace
{

const std::string testRealm = "cppwamp.test";
const unsigned short testPort = 12345;
const auto withTcp = TcpHost("localhost", testPort).withFormat(json);

//------------------------------------------------------------------------------
void checkJoinInfo(const SessionJoinInfo& info, const Welcome& w)
{
    CHECK(info.authId       == w.authId());
    CHECK(info.authMethod   == w.authMethod());
    CHECK(info.authProvider == w.authProvider());
    CHECK(info.authRole     == w.authRole());
    CHECK(info.sessionId    == w.sessionId());

    // The transport property is optional and implementation-defined
    if (test::RouterFixture::enabled())
    {
        auto t = info.transport;
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
}

//------------------------------------------------------------------------------
void doCheckRegisterMetaProcedure(const Uri& realmUri,
                                  WampErrc expectedForKnown,
                                  WampErrc expectedForUnknown)
{
    IoContext ioctx;

    spawn(ioctx, [&](YieldContext yield)
    {
        Session s{ioctx};
        s.connect(withTcp, yield).value();
        s.join(Petition{realmUri}, yield).value();

        {
            INFO( "Known meta procedure" );
            auto reg = s.enroll(
                Procedure{"wamp.session.count"},
                [](Invocation) -> Outcome {return Result{42};},
                yield);
            if (expectedForKnown == WampErrc::success)
            {
                auto count = s.call(Rpc{"wamp.session.count"}, yield);
                REQUIRE(count.has_value());
                REQUIRE(!count.value().args().empty());
                CHECK(count.value().args().at(0) == 42);
            }
            else
            {
                REQUIRE_FALSE(reg.has_value());
                CHECK(reg.error() == expectedForKnown);
            }
        }

        {
            INFO( "Unknown meta procedure" );
            auto reg = s.enroll(
                Procedure{"wamp.bogus"},
                [](Invocation) -> Outcome {return Result{123};},
                yield);
            if (expectedForUnknown == WampErrc::success)
            {
                auto count = s.call(Rpc{"wamp.bogus"}, yield);
                REQUIRE(count.has_value());
                REQUIRE(!count.value().args().empty());
                CHECK(count.value().args().at(0) == 123);
            }
            else
            {
                REQUIRE_FALSE(reg.has_value());
                CHECK(reg.error() == expectedForUnknown);
            }
        }

        s.disconnect();
    });

    ioctx.run();
}

//------------------------------------------------------------------------------
void checkRegisterMetaProcedure(bool metaApiEnabled,
                                bool metaProcedureRegistrationAllowed,
                                WampErrc expectedForKnown,
                                WampErrc expectedForUnknown)
{
    auto& router = test::RouterFixture::instance().router();
    test::RouterLogLevelGuard guard{router.logLevel()};
    router.setLogLevel(LogLevel::error);

    const String realmUri{"cppwamp.test-meta-procedure-registration"};
    auto options =
        RealmOptions{realmUri}
            .withMetaApiEnabled(metaApiEnabled)
            .withMetaProcedureRegistrationAllowed(
                metaProcedureRegistrationAllowed);
    test::ScopedRealm realm{router.openRealm(options).value()};

    doCheckRegisterMetaProcedure(realmUri, expectedForKnown,
                                 expectedForUnknown);
    realm->close();
}

//------------------------------------------------------------------------------
void doCheckPublishMetaTopic(const Uri& realmUri, WampErrc expected)
{
    IoContext ioctx;
    Event knownEvent;
    Event unknownEvent;
    auto onKnownEvent = [&](Event e) {knownEvent = std::move(e);};
    auto onUnknownEvent = [&](Event e) {unknownEvent = std::move(e);};

    spawn(ioctx, [&](YieldContext yield)
    {
        Session s1{ioctx};
        s1.connect(withTcp, yield).value();
        s1.join(Petition{realmUri}, yield).value();
        s1.subscribe(Topic{"wamp.session.on_join"}, onKnownEvent,
                     yield).value();
        s1.subscribe(Topic{"wamp.bogus"}, onUnknownEvent, yield).value();

        Session s2{ioctx};
        s2.connect(withTcp, yield).value();
        s2.join(Petition{realmUri}, yield).value();

        {
            INFO( "Known meta topic" );
            auto pub = s2.publish(
                Pub{"wamp.session.on_join"}.withArgs(42),
                yield);
            if (expected == WampErrc::success)
            {
                while (knownEvent.args().empty())
                    test::suspendCoro(yield);
                CHECK(knownEvent.args().at(0) == 42);
            }
            else
            {
                REQUIRE_FALSE(pub.has_value());
                CHECK(pub.error() == expected);
            }
        }

        {
            INFO( "Unknown meta topic" );
            auto pub = s2.publish(Pub{"wamp.bogus"}.withArgs(123), yield);
            if (expected == WampErrc::success)
            {
                while (unknownEvent.args().empty())
                    test::suspendCoro(yield);
                CHECK(unknownEvent.args().at(0) == 123);
            }
            else
            {
                REQUIRE_FALSE(pub.has_value());
                CHECK(pub.error() == expected);
            }
        }

        s2.disconnect();
        s1.disconnect();
    });

    ioctx.run();
}

//------------------------------------------------------------------------------
void checkPublishMetaTopic(bool allowed, WampErrc expected)
{
    auto& router = test::RouterFixture::instance().router();
    test::RouterLogLevelGuard guard{router.logLevel()};
    router.setLogLevel(LogLevel::error);

    const String realmUri{"cppwamp.test-meta-topic-publication"};
    auto options =
        RealmOptions{realmUri}.withMetaTopicPublicationAllowed(allowed);
    test::ScopedRealm realm{router.openRealm(options).value()};

    doCheckPublishMetaTopic(realmUri, expected);
    realm->close();
}

} // anomymous namespace


//------------------------------------------------------------------------------
TEST_CASE( "Time formatting and parsing", "[Router][Time]" )
{
    using TimePoint = std::chrono::system_clock::time_point;

    struct TestVector
    {
        int64_t msTicks;
        std::string text;
    };

    const std::vector<TestVector> testVectors
    {
        {-9223371960000000, "1677-09-21T00:14:00.000000Z"},
        {-1000001,          "1969-12-31T23:59:58.999999Z"},
        {-1000000,          "1969-12-31T23:59:59.000000Z"},
        {-999999,           "1969-12-31T23:59:59.000001Z"},
        {-500000,           "1969-12-31T23:59:59.500000Z"},
        {-1,                "1969-12-31T23:59:59.999999Z"},
        {0,                 "1970-01-01T00:00:00.000000Z"},
        {1,                 "1970-01-01T00:00:00.000001Z"},
        {500000,            "1970-01-01T00:00:00.500000Z"},
        {999999,            "1970-01-01T00:00:00.999999Z"},
        {1000000,           "1970-01-01T00:00:01.000000Z"},
        {946684799999999,   "1999-12-31T23:59:59.999999Z"},
        {946684800000000,   "2000-01-01T00:00:00.000000Z"},
        {951782400000000,   "2000-02-29T00:00:00.000000Z"},
        {951868799999999,   "2000-02-29T23:59:59.999999Z"},
        {951868800000000,   "2000-03-01T00:00:00.000000Z"},
        {983404799999999,   "2001-02-28T23:59:59.999999Z"},
        {983404800000000,   "2001-03-01T00:00:00.000000Z"},
        {9223372036854775,  "2262-04-11T23:47:16.854775Z"}
    };

    for (const auto& vec: testVectors)
    {
        INFO("For timestamp " << vec.text);
        TimePoint time;
        TimePoint expected{std::chrono::microseconds{vec.msTicks}};
        bool ok = internal::parseRfc3339Timestamp(vec.text, time);
        CHECK(ok);
        CHECK(time.time_since_epoch().count() ==
              expected.time_since_epoch().count());

        auto formatted = internal::toRfc3339Timestamp<6>(expected);
        CHECK(formatted == vec.text);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "Invalid timestamp parsing", "[Router][Time]" )
{
    using TimePoint = std::chrono::system_clock::time_point;

    const std::vector<std::string> timestamps
    {
        "0000-00-00T00:00:00Z",   // Zero day and month
        "1970-00-01T00:00:00Z",   // Zero month
        "1970-01-00T00:00:00Z",   // Zero day
        "1970-01-01T-1:00:00Z",   // Negative hour
        "1970-01-01T00:00:61Z",   // Invalid seconds
        "1970-01-01T00:60:00Z",   // Invalid minutes
        "1970-01-01T24:00:00Z",   // Invalid hour
        "1970-01-32T00:00:00Z",   // Invalid day
        "1970-13-01T00:00:00Z",   // Invalid month
        " 1970-01-01T00:00:00Z",  // leading space
        "1970-01-01 T00:00:00Z",  // middle space
        "1970-01-01T 00:00:00Z",  // middle space
        "1970-01-01T00: 00:00Z",  // middle space
        "1970-01-01T00:00 :00Z",  // middle space
        "1970-01-01T00:00:00Z ",  // trailing space
        "01970-01-01T00:00:00Z",  // too many digits
        "1970-001-01T00:00:00Z",  // too many digits
        "1970-01-001T00:00:00Z",  // too many digits
        "1970-01-00T000:00:00Z",  // too many digits
        "1970-01-00T00:000:00Z",  // too many digits
        "1970-01-00T00:00:000Z",  // too many digits
        "1970-01-01T00:00:00GMT", // invalid time zone
        "1970-01-01T00:00:00UTC", // invalid time zone
        "1970-01-01T00:00:00",    // no time zone
        "19700101000000Z",        // no separator
        "1970-01-01/00:00:00Z",   // bad separator
        "1970/01/01T00:00:00Z",   // bad separator
        "1970.01.01T00:00:00Z",   // bad separator
        "1970-01-01T00-00-00Z",   // bad separator
        "1970-01-01T00.00.00Z",   // bad separator
        "01-01-1970T00:00:00Z",   // wrong order
        "01/01/1970T00:00:00Z",   // MM/DD/YYYY format
        "1970-01-01.0T00:00:00Z", // decimal days
    };

    for (const auto& timestamp: timestamps)
    {
        INFO("For timestamp '" << timestamp << "'");
        TimePoint time;
        bool ok = internal::parseRfc3339Timestamp(timestamp, time);
        CHECK_FALSE(ok);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "WAMP session meta events", "[WAMP][Router][MetaAPI]" )
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
        auto w1 = s1.join(Petition(testRealm), yield).value();
        REQUIRE(w1.features().broker().test(Feature::sessionMetaApi));
        s1.subscribe(Topic{"wamp.session.on_join"}, onJoin, yield).value();
        s1.subscribe(Topic{"wamp.session.on_leave"}, onLeave, yield).value();

        s2.connect(withTcp, yield).value();
        auto w2 = s2.join(Petition(testRealm), yield).value();

        while (joinedInfo.sessionId == 0)
            test::suspendCoro(yield);
        checkJoinInfo(joinedInfo, w2);

        s2.leave(yield).value();

        while (leftInfo.sessionId == 0)
            test::suspendCoro(yield);
        CHECK(leftInfo.sessionId == w2.sessionId());

        // Crossbar only provides session ID
        if (test::RouterFixture::enabled())
        {
            CHECK(leftInfo.authid == w2.authId());
            CHECK(leftInfo.authrole == w2.authRole());
        }

        s2.disconnect();
        s1.disconnect();
    });

    ioctx.run();
}

//------------------------------------------------------------------------------
TEST_CASE( "WAMP session meta procedures", "[WAMP][Router][MetaAPI]" )
{
    using SessionIdList = std::vector<SessionId>;

    IoContext ioctx;
    Session s1{ioctx};
    Session s2{ioctx};

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
        REQUIRE(w1.features().dealer().test(Feature::sessionMetaApi));

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

            auto result = s1.call(rpc, yield).value();
            REQUIRE(result.args().size() == 1);
            result.convertTo(list);
            CHECK_THAT(list, Matchers::UnorderedEquals(allSessionIds));

            result = s1.call(rpc.withArgs(inclusiveAuthRoleList), yield).value();
            REQUIRE(result.args().size() == 1);
            result.convertTo(list);
            CHECK_THAT(list, Matchers::UnorderedEquals(allSessionIds));

            result = s1.call(rpc.withArgs(exclusiveAuthRoleList), yield).value();
            REQUIRE(result.args().size() == 1);
            result.convertTo(list);
            CHECK(list.empty());
        }

        {
            INFO("wamp.session.get");
            Rpc rpc{"wamp.session.get"};
            SessionJoinInfo info;

            auto result = s1.call(rpc.withArgs(w2.sessionId()), yield).value();
            REQUIRE(result.args().size() == 1);
            result.convertTo(info);
            checkJoinInfo(info, w2);
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
                test::suspendCoro(yield);

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
            CHECK_THAT(list, Matchers::Equals(SessionIdList{w2.sessionId()}));

            while (incidents.empty() || s2.state() == SessionState::established)
                test::suspendCoro(yield);

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
                test::suspendCoro(yield);

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
                test::suspendCoro(yield);

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
TEST_CASE( "Attempting to register meta procedures", "[WAMP][Router]" )
{
    SECTION("Meta API disabled and registrations not allowed")
    {
        if (!test::RouterFixture::enabled())
            return;
        checkRegisterMetaProcedure(
            false, false, WampErrc::invalidUri, WampErrc::invalidUri);
    }

    SECTION("Meta API disabled and registrations allowed")
    {
        if (!test::RouterFixture::enabled())
            return;
        checkRegisterMetaProcedure(
            false, true, WampErrc::success, WampErrc::success);
    }

    // This is the behavior for Crossbar
    SECTION("Meta API enabled and registrations not allowed")
    {
        doCheckRegisterMetaProcedure(testRealm, WampErrc::invalidUri,
                                     WampErrc::invalidUri);
    }

    SECTION("Meta API enabled and registrations allowed")
    {
        if (!test::RouterFixture::enabled())
            return;
        checkRegisterMetaProcedure(
            true, true, WampErrc::procedureAlreadyExists, WampErrc::success);
    }
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
        // Crossbar nulls the session ID in the meta event when
        // the callee leaves.
        // https://github.com/crossbario/crossbar/issues/2084
        Variant maybeSessionId;
        event.convertTo(maybeSessionId, unregisteredRegId);
        unregisteredSessionId = maybeSessionId.valueOr<SessionId>(0);
    };

    auto onRegistrationDeleted = [&](Event event)
    {
        // Crossbar nulls the session ID in the meta event when
        // the callee leaves.
        // https://github.com/crossbario/crossbar/issues/2084
        Variant maybeSessionId;
        event.convertTo(maybeSessionId, deletedRegistrationId);
        regDeletedSessionId = maybeSessionId.valueOr<SessionId>(0);
    };

    auto rpc = [](Invocation) -> Outcome {return {};};

    spawn(ioctx, [&](YieldContext yield)
    {
        namespace chrono = std::chrono;
        auto now = chrono::system_clock::now();
        auto before = now - chrono::seconds(60);
        auto after = now + chrono::seconds(60);

        s1.connect(withTcp, yield).value();
        auto w1 = s1.join(Petition(testRealm), yield).value();
        REQUIRE(w1.features().dealer().test(Feature::registrationMetaApi));
        s1.subscribe(Topic{"wamp.registration.on_create"},
                     onRegistrationCreated, yield).value();
        s1.subscribe(Topic{"wamp.registration.on_register"}, onRegister,
                     yield).value();
        s1.subscribe(Topic{"wamp.registration.on_unregister"}, onUnregister,
                     yield).value();
        s1.subscribe(Topic{"wamp.registration.on_delete"},
                     onRegistrationDeleted, yield).value();

        s2.connect(withTcp, yield).value();
        auto w2 = s2.join(Petition(testRealm), yield).value();
        auto reg = s2.enroll(Procedure{"rpc"}, rpc, yield).value();
        while (regInfo.id == 0 || registrationId == 0)
            test::suspendCoro(yield);
        CHECK(regCreatedSessionId == w2.sessionId());
        CHECK(regInfo.uri == "rpc");
        CHECK(regInfo.created > before);
        CHECK(regInfo.created < after);
        CHECK(regInfo.id == reg.id());
        CHECK(regInfo.matchPolicy == MatchPolicy::exact);
        CHECK(regInfo.invocationPolicy == InvocationPolicy::single);
        CHECK(registeredSessionId == w2.sessionId());
        CHECK(registrationId == reg.id());

        reg.unregister();
        while (unregisteredRegId == 0 || deletedRegistrationId == 0)
            test::suspendCoro(yield);
        CHECK(unregisteredSessionId == w2.sessionId());
        CHECK(unregisteredRegId == reg.id());
        CHECK(regDeletedSessionId == w2.sessionId());
        CHECK(deletedRegistrationId == reg.id());

        unregisteredRegId = 0;
        deletedRegistrationId = 0;
        reg = s2.enroll(Procedure{"rpc"}, rpc, yield).value();
        s2.leave(yield).value();
        while (unregisteredRegId == 0 || deletedRegistrationId == 0)
            test::suspendCoro(yield);
        CHECK(unregisteredRegId == reg.id());
        CHECK(deletedRegistrationId == reg.id());

        // Crossbar nulls the session ID in the meta event when
        // the callee leaves.
        // https://github.com/crossbario/crossbar/issues/2084
        if (test::RouterFixture::enabled())
        {
            CHECK(unregisteredSessionId == w2.sessionId());
            CHECK(regDeletedSessionId == w2.sessionId());
        }

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
        // Crossbar nulls the session ID in the meta event when
        // the subscriber leaves.
        // https://github.com/crossbario/crossbar/issues/2084
        Variant maybeSessionId;
        event.convertTo(maybeSessionId, unsubscribedSubId);
        unsubscribedSessionId = maybeSessionId.valueOr<SessionId>(0);
    };

    auto onSubDeleted = [&](Event event)
    {
        // Crossbar nulls the session ID in the meta event when
        // the subscriber leaves.
        // https://github.com/crossbario/crossbar/issues/2084
        Variant maybeSessionId;
        event.convertTo(maybeSessionId, deletedSubId);
        deletedSessionId = maybeSessionId.valueOr<SessionId>(0);
    };

    spawn(ioctx, [&](YieldContext yield)
    {
        namespace chrono = std::chrono;
        auto now = chrono::system_clock::now();
        auto before = now - chrono::seconds(60);
        auto after = now + chrono::seconds(60);
        s1.connect(withTcp, yield).value();
        auto w1 = s1.join(Petition(testRealm), yield).value();
        REQUIRE(w1.features().broker().test(Feature::subscriptionMetaApi));
        s1.subscribe(Topic{"wamp.subscription.on_create"},
                     onSubscriptionCreated, yield).value();
        s1.subscribe(Topic{"wamp.subscription.on_subscribe"}, onSubscribe,
                     yield).value();
        s1.subscribe(Topic{"wamp.subscription.on_unsubscribe"}, onUnsubscribe,
                     yield).value();
        s1.subscribe(Topic{"wamp.subscription.on_delete"},
                     onSubDeleted, yield).value();

        s2.connect(withTcp, yield).value();
        auto w2 = s2.join(Petition(testRealm), yield).value();
        auto sub2 = s2.subscribe(Topic{"exact"}, [](Event) {}, yield).value();

        while (subInfo.id == 0 || subscriptionId == 0)
            test::suspendCoro(yield);
        CHECK(subCreatedSessionId == w2.sessionId());
        CHECK(subInfo.uri == "exact");
        CHECK(subInfo.created > before);
        CHECK(subInfo.created < after);
        CHECK(subInfo.id == sub2.id());
        CHECK(subInfo.matchPolicy == MatchPolicy::exact);
        CHECK(subscribedSessionId == w2.sessionId());
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
            test::suspendCoro(yield);
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

        while (subscriptionId == 0) test::suspendCoro(yield);
        CHECK(subscribedSessionId == welcome4.sessionId());
        CHECK(subscriptionId == sub4.id());

        sub3.unsubscribe();
        while (unsubscribedSubId == 0) {test::suspendCoro(yield);}
        CHECK(unsubscribedSubId == sub3.id());
        CHECK(deletedSubId == 0);
        CHECK(unsubscribedSessionId == welcome3.sessionId());
        CHECK(deletedSessionId == 0);

        unsubscribedSubId = 0;
        sub4.unsubscribe();
        while (unsubscribedSubId == 0 || deletedSubId == 0)
            test::suspendCoro(yield);
        CHECK(unsubscribedSessionId == welcome4.sessionId());
        CHECK(unsubscribedSubId == sub4.id());
        CHECK(deletedSessionId == welcome4.sessionId());
        CHECK(deletedSubId == sub4.id());

        unsubscribedSubId = 0;
        deletedSubId = 0;
        s2.leave(yield).value();
        while (unsubscribedSubId == 0 || deletedSubId == 0)
            test::suspendCoro(yield);
        CHECK(unsubscribedSubId == sub2.id());
        CHECK(deletedSubId == sub2.id());

        // Crossbar nulls the session ID in the meta event when
        // the callee leaves.
        // https://github.com/crossbario/crossbar/issues/2084
        if (test::RouterFixture::enabled())
        {
            CHECK(unsubscribedSessionId == w2.sessionId());
            CHECK(deletedSessionId == w2.sessionId());
        }

        s4.disconnect();
        s3.disconnect();
        s2.disconnect();
        s1.disconnect();
    });

    ioctx.run();
}

//------------------------------------------------------------------------------
TEST_CASE( "Attempting to publish meta topics", "[WAMP][Router][MetaAPI]" )
{
    // This is the behavior for Crossbar
    SECTION("publications not allowed")
    {
        doCheckPublishMetaTopic(testRealm, WampErrc::invalidUri);
    }

    SECTION("publications allowed")
    {
        if (!test::RouterFixture::enabled())
            return;
        checkPublishMetaTopic(true, WampErrc::success);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "Insecure WAMP meta events subscriptions",
           "[WAMP][Router][MetaAPI]" )
{
    IoContext ioctx;
    Session s1{ioctx};
    Session s2{ioctx};
    int neverFiredEventCount = 0;
    int prefixEventCount = 0;
    int wildcardEventCount = 0;
    int registerEventCount = 0;

    auto onNeverFiredEvent =
        [&neverFiredEventCount](Event) {++neverFiredEventCount;};

    auto onPrefixEvent =
        [&prefixEventCount](Event ev)
        {
            auto topic = ev.topic().value_or("");
            if (topic.rfind("wamp.subscription", 0) != 0)
                ++prefixEventCount;
        };

    auto onWildcardEvent =
        [&wildcardEventCount](Event ev)
        {
            auto topic = ev.topic().value_or("");
            if (topic.rfind("wamp.subscription", 0) != 0)
                ++wildcardEventCount;
        };

    auto onRegisterEvent = [&registerEventCount](Event) {++registerEventCount;};

    spawn(ioctx, [&](YieldContext yield)
    {
        s1.connect(withTcp, yield).value();
        auto w1 = s1.join(Petition(testRealm), yield).value();
        REQUIRE(w1.features().broker().test(Feature::sessionMetaApi));

        s1.subscribe(Topic{"wamp.registration.on_register"},
                     onRegisterEvent, yield).value();

        using MP = MatchPolicy;
        s1.subscribe(Topic{"wamp."}.withMatchPolicy(MP::prefix),
                     onPrefixEvent, yield).value();
        s1.subscribe(Topic{"wamp"}.withMatchPolicy(MP::prefix),
                     onNeverFiredEvent, yield).value();
        s1.subscribe(Topic{"w"}.withMatchPolicy(MP::prefix),
                     onNeverFiredEvent, yield).value();

        s1.subscribe(Topic{"wamp.."}.withMatchPolicy(MP::wildcard),
                     onWildcardEvent, yield).value();
        s1.subscribe(Topic{".."}.withMatchPolicy(MP::wildcard),
                     onNeverFiredEvent, yield).value();
        s1.subscribe(Topic{".session."}.withMatchPolicy(MP::wildcard),
                     onNeverFiredEvent, yield).value();
        s1.subscribe(Topic{"..on_join"}.withMatchPolicy(MP::wildcard),
                     onNeverFiredEvent, yield).value();
        s1.subscribe(Topic{".session.on_join"}.withMatchPolicy(MP::wildcard),
                     onNeverFiredEvent, yield).value();

        s2.connect(withTcp, yield).value();
        auto w2 = s2.join(Petition(testRealm), yield).value();

        // Cause a registration meta event to stop the waiting loop below.
        s2.enroll(Procedure{"rpc"},
                  [](Invocation) -> Outcome {return Result{};},
                  yield).value();

        while (registerEventCount == 0)
            test::suspendCoro(yield);

        CHECK(neverFiredEventCount == 0);
        CHECK(prefixEventCount == 2); // registration created + session join
        CHECK(wildcardEventCount == 2);

        s2.disconnect();
        s1.disconnect();
    });

    ioctx.run();
}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
