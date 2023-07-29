/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#if defined(CPPWAMP_TEST_HAS_CORO)

#include <algorithm>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <catch2/catch.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/msgpack.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/spawn.hpp>
#include <cppwamp/transports/tcp.hpp>

using namespace wamp;
using namespace Catch::Matchers;

namespace
{

const std::string testRealm = "cppwamp.test";
const short testPort = 12345;
const std::string authTestRealm = "cppwamp.authtest";
const short authTestPort = 23456;
const auto withTcp = TcpHost("localhost", testPort).withFormat(json);
const auto authTcp = TcpHost("localhost", authTestPort).withFormat(json);

//------------------------------------------------------------------------------
struct TicketAuthFixture
{
    TicketAuthFixture(IoContext& ioctx, ConnectionWish wish)
        : where(std::move(wish)),
          session(ioctx)
    {}

    void join(String authId, String signature, YieldContext yield)
    {
        this->signature = std::move(signature);
        session.connect(where, yield).value();
        if (noChallengeHandlerArmed)
        {
            welcome =
                session.join(
                    Petition(authTestRealm).withAuthMethods({"ticket"})
                                           .withAuthId(std::move(authId))
                                           .captureAbort(abortReason),
                    yield);
        }
        else
        {
            welcome =
                session.join(
                    Petition(authTestRealm).withAuthMethods({"ticket"})
                                           .withAuthId(std::move(authId))
                                           .captureAbort(abortReason),
                    [this](Challenge c) {onChallenge(std::move(c));},
                    yield);
        }
    }

    void onChallenge(Challenge authChallenge)
    {
        ++challengeCount;
        challenge = authChallenge;
        challengeState = session.state();
        if (failAuthenticationArmed)
            authChallenge.fail(Reason{WampErrc::authenticationFailed});
        else if (throwArmed)
            throw Reason{WampErrc::authenticationFailed};
        else
            authChallenge.authenticate(Authentication(signature));
    }

    ConnectionWish where;
    Session session;
    String signature;
    SessionState challengeState = SessionState::closed;
    unsigned challengeCount = 0;
    Challenge challenge;
    ErrorOr<Welcome> welcome;
    Reason abortReason;
    bool failAuthenticationArmed = false;
    bool throwArmed = false;
    bool noChallengeHandlerArmed = false;
};

} // anonymous namespace


//------------------------------------------------------------------------------
SCENARIO( "WAMP ticket authentication", "[WAMP][Advanced]" )
{
GIVEN( "a Session with a registered challenge handler" )
{
    IoContext ioctx;
    TicketAuthFixture f(ioctx, authTcp);
    std::vector<Incident> incidents;
    f.session.observeIncidents(
        [&incidents](Incident i) {incidents.push_back(i);});

    WHEN( "joining with ticket authentication requested" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            f.join("alice", "password123", yield);
            f.session.disconnect();
        });
        ioctx.run();

        THEN( "the challenge was received and the authentication accepted" )
        {
            REQUIRE( f.challengeCount == 1 );
            CHECK( f.challengeState == SessionState::authenticating );
            CHECK( f.challenge.method() == "ticket" );
            REQUIRE( f.welcome.has_value() );
            CHECK( f.welcome->optionByKey("authmethod") == "ticket" );
            CHECK( f.welcome->optionByKey("authrole") == "ticketrole" );
            CHECK( incidents.empty() );
        }
    }

    WHEN( "joining with an invalid ticket" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            f.join("alice", "badpassword", yield);
            f.session.disconnect();
        });
        ioctx.run();

        THEN( "the challenge was received and the authentication rejected" )
        {
            REQUIRE( f.challengeCount == 1 );
            CHECK( f.challengeState == SessionState::authenticating );
            CHECK( f.challenge.method() == "ticket" );
            CHECK_FALSE( f.welcome.has_value() );
            auto ec = f.abortReason.errorCode();
            CHECK((ec == WampErrc::authenticationDenied ||
                   ec == WampErrc::authorizationDenied)); // <- Crossbar
            CHECK( incidents.empty() );
        }
    }

    WHEN( "failing the authentication" )
    {
        f.failAuthenticationArmed = true;

        spawn(ioctx, [&](YieldContext yield)
        {
            f.join("alice", "password123", yield);
            CHECK(f.session.state() == SessionState::failed);
            f.session.disconnect();
        });
        ioctx.run();

        THEN( "the session was aborted by the client" )
        {
            REQUIRE( f.challengeCount == 1 );
            CHECK( f.challengeState == SessionState::authenticating );
            CHECK( f.challenge.method() == "ticket" );
            REQUIRE_FALSE( f.welcome.has_value() );
            CHECK( f.welcome.error() == WampErrc::authenticationFailed );
            CHECK( f.abortReason.uri().empty() );
            REQUIRE( incidents.size() == 1 );
            CHECK( incidents.at(0).kind() == IncidentKind::challengeFailure );
            CHECK( incidents.at(0).error() == WampErrc::authenticationFailed );
        }
    }

    WHEN( "throwing within the challenge handler" )
    {
        f.throwArmed = true;

        spawn(ioctx, [&](YieldContext yield)
        {
            f.join("alice", "password123", yield);
            CHECK(f.session.state() == SessionState::failed);
            f.session.disconnect();
        });
        ioctx.run();

        THEN( "the session was aborted by the client" )
        {
            REQUIRE( f.challengeCount == 1 );
            CHECK( f.challengeState == SessionState::authenticating );
            CHECK( f.challenge.method() == "ticket" );
            REQUIRE_FALSE( f.welcome.has_value() );
            CHECK( f.welcome.error() == WampErrc::authenticationFailed );
            CHECK( f.abortReason.uri().empty() );
            REQUIRE( incidents.size() == 1 );
            CHECK( incidents.at(0).kind() == IncidentKind::challengeFailure );
            CHECK( incidents.at(0).error() == WampErrc::authenticationFailed );
        }
    }

    WHEN( "missing challenge handler" )
    {
        f.noChallengeHandlerArmed = true;

        spawn(ioctx, [&](YieldContext yield)
        {
            f.join("alice", "password123", yield);
            CHECK(f.session.state() == SessionState::failed);
            f.session.disconnect();
        });
        ioctx.run();

        THEN( "the session was aborted by the client" )
        {
            REQUIRE_FALSE( f.welcome.has_value() );
            CHECK( f.welcome.error() == WampErrc::authenticationFailed );
            CHECK( f.abortReason.uri().empty() );
            REQUIRE( incidents.size() == 1 );
            CHECK( incidents.at(0).kind() == IncidentKind::challengeFailure );
            CHECK( incidents.at(0).error() == WampErrc::authenticationFailed );
        }
    }

}}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
