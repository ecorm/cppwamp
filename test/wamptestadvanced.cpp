/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#if CPPWAMP_TESTING_WAMP

#include <catch.hpp>
#include <cppwamp/corosession.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/tcp.hpp>
#include <cppwamp/internal/config.hpp>

#include <iostream>

using namespace wamp;
using namespace Catch::Matchers;

namespace
{

const std::string testRealm = "cppwamp.test";
const short testPort = 12345;
const std::string authTestRealm = "cppwamp.authtest";
const short authTestPort = 23456;

Connector::Ptr tcp(AsioService& iosvc)
{
    return connector<Json>(iosvc, TcpHost("localhost", testPort));
}

Connector::Ptr authTcp(AsioService& iosvc)
{
    return connector<Json>(iosvc, TcpHost("localhost", authTestPort));
}

//------------------------------------------------------------------------------
struct RpcFixture
{
    template <typename TConnector>
    RpcFixture(AsioService& iosvc, TConnector cnct)
        : caller(CoroSession<>::create(iosvc, cnct)),
          callee(CoroSession<>::create(iosvc, cnct))
    {}

    void join(boost::asio::yield_context yield)
    {
        caller->connect(yield);
        callerId = caller->join(Realm(testRealm), yield).id();
        callee->connect(yield);
        callee->join(Realm(testRealm), yield);
    }

    void disconnect()
    {
        caller->disconnect();
        callee->disconnect();
    }

    CoroSession<>::Ptr caller;
    CoroSession<>::Ptr callee;

    SessionId callerId = -1;
};

//------------------------------------------------------------------------------
struct PubSubFixture
{
    template <typename TConnector>
    PubSubFixture(AsioService& iosvc, TConnector cnct)
        : publisher(CoroSession<>::create(iosvc, cnct)),
          subscriber(CoroSession<>::create(iosvc, cnct))
    {}

    void join(boost::asio::yield_context yield)
    {
        publisher->connect(yield);
        publisherId = publisher->join(Realm(testRealm), yield).id();
        subscriber->connect(yield);
        subscriber->join(Realm(testRealm), yield);
    }

    void disconnect()
    {
        publisher->disconnect();
        subscriber->disconnect();
    }

    CoroSession<>::Ptr publisher;
    CoroSession<>::Ptr subscriber;

    SessionId publisherId = -1;
    SessionId subscriberId = -1;
};

//------------------------------------------------------------------------------
struct TicketAuthFixture
{
    template <typename TConnector>
    TicketAuthFixture(AsioService& iosvc, TConnector cnct)
        : session(CoroSession<>::create(iosvc, cnct))
    {
        session->setChallengeHandler( [this](Challenge c){onChallenge(c);} );
    }

    void join(String authId, String signature, boost::asio::yield_context yield)
    {
        this->signature = std::move(signature);
        session->connect(yield);
        info = session->join(Realm(authTestRealm).withAuthMethods({"ticket"})
                                                 .withAuthId(std::move(authId)),
                             yield);
    }

    void onChallenge(Challenge authChallenge)
    {
        ++challengeCount;
        challenge = authChallenge;
        challengeState = session->state();
        session->authenticate(Authentication(signature));
    }

    CoroSession<>::Ptr session;
    String signature;
    SessionState challengeState = SessionState::closed;
    unsigned challengeCount = 0;
    Challenge challenge;
    SessionInfo info;
};

} // anonymous namespace


//------------------------------------------------------------------------------
SCENARIO( "WAMP RPC advanced features", "[WAMP]" )
{
GIVEN( "a caller and a callee" )
{
    AsioService iosvc;
    RpcFixture f(iosvc, tcp(iosvc));

    WHEN( "using caller identification" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            SessionId disclosedId = -1;

            f.join(yield);

            f.callee->enroll(
                Procedure("rpc"),
                [&disclosedId](Invocation inv) -> Outcome
                {
                    disclosedId = inv.caller().valueOr<SessionId>(-1);
                    return {};
                },
                yield);

            f.caller->call(Rpc("rpc").withDiscloseMe(), yield);
            CHECK( disclosedId == f.callerId );
            f.disconnect();
        });
        iosvc.run();
    }

    WHEN( "using pattern-based registrations" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            int prefixMatchCount = 0;
            int wildcardMatchCount = 0;

            f.join(yield);

            f.callee->enroll(
                Procedure("com.myapp").usingPrefixMatch(),
                [&prefixMatchCount](Invocation inv) -> Outcome
                {
                    ++prefixMatchCount;
                    CHECK_THAT( inv.procedure().valueOr<std::string>(""),
                                Equals("com.myapp.foo") );
                    return {};
                },
                yield);

            f.callee->enroll(
                Procedure("com.other..rpc").usingWildcardMatch(),
                [&wildcardMatchCount](Invocation inv) -> Outcome
                {
                    ++wildcardMatchCount;
                    CHECK_THAT( inv.procedure().valueOr<std::string>(""),
                                Equals("com.other.foo.rpc") );
                    return {};
                },
                yield);

            f.caller->call(Rpc("com.myapp.foo"), yield);
            CHECK( prefixMatchCount == 1 );
            CHECK( wildcardMatchCount == 0 );

            f.caller->call(Rpc("com.other.foo.rpc"), yield);
            CHECK( prefixMatchCount == 1 );
            CHECK( wildcardMatchCount == 1 );

            f.disconnect();
        });
        iosvc.run();
    }
}}

//------------------------------------------------------------------------------
SCENARIO( "WAMP pub/sub advanced features", "[WAMP]" )
{
GIVEN( "a publisher and a subscriber" )
{
    AsioService iosvc;
    PubSubFixture f(iosvc, tcp(iosvc));

    WHEN( "using publisher identification" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            SessionId disclosedId = -1;
            int eventCount = 0;

            f.join(yield);

            f.subscriber->subscribe(
                Topic("onEvent"),
                [&disclosedId, &eventCount](Event event)
                {
                    disclosedId = event.publisher().valueOr<SessionId>(-1);
                    ++eventCount;
                },
                yield);

            f.publisher->publish(Pub("onEvent").withDiscloseMe(), yield);
            while (eventCount == 0)
                f.publisher->suspend(yield);
            CHECK( disclosedId == f.publisherId );
            f.disconnect();
        });
        iosvc.run();
    }

    WHEN( "using pattern-based subscriptions" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            int prefixMatchCount = 0;
            int wildcardMatchCount = 0;
            std::string prefixTopic;
            std::string wildcardTopic;

            f.join(yield);

            f.subscriber->subscribe(
                Topic("com.myapp").usingPrefixMatch(),
                [&prefixMatchCount, &prefixTopic](Event event)
                {
                    prefixTopic = event.topic().valueOr<std::string>("");
                    ++prefixMatchCount;
                },
                yield);

            f.subscriber->subscribe(
                Topic("com..onEvent").usingWildcardMatch(),
                [&wildcardMatchCount, &wildcardTopic](Event event)
                {
                    wildcardTopic = event.topic().valueOr<std::string>("");
                    ++wildcardMatchCount;
                },
                yield);

            f.publisher->publish(Pub("com.myapp.foo"), yield);
            while (prefixMatchCount < 1)
                f.publisher->suspend(yield);
            CHECK( prefixMatchCount == 1 );
            CHECK_THAT( prefixTopic, Equals("com.myapp.foo") );
            CHECK( wildcardMatchCount == 0 );

            f.publisher->publish(Pub("com.foo.onEvent"), yield);
            while (wildcardMatchCount < 1)
                f.publisher->suspend(yield);
            CHECK( prefixMatchCount == 1 );
            CHECK( wildcardMatchCount == 1 );
            CHECK_THAT( wildcardTopic, Equals("com.foo.onEvent") );

            f.publisher->publish(Pub("com.myapp.onEvent"), yield);
            while ((prefixMatchCount < 2) || (wildcardMatchCount < 2))
                f.publisher->suspend(yield);
            CHECK( prefixMatchCount == 2 );
            CHECK( wildcardMatchCount == 2 );
            CHECK_THAT( prefixTopic, Equals("com.myapp.onEvent") );
            CHECK_THAT( wildcardTopic, Equals("com.myapp.onEvent") );

            f.disconnect();
        });
        iosvc.run();
    }

    WHEN( "using publisher exclusion" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            int subscriberEventCount = 0;
            int publisherEventCount = 0;

            f.join(yield);

            f.subscriber->subscribe(
                Topic("onEvent"),
                [&subscriberEventCount](Event) {++subscriberEventCount;},
                yield);

            f.publisher->subscribe(
                Topic("onEvent"),
                [&publisherEventCount](Event) {++publisherEventCount;},
                yield);

            f.publisher->publish(Pub("onEvent").withExcludeMe(false), yield);
            while ((subscriberEventCount < 1) || (publisherEventCount < 1))
                f.publisher->suspend(yield);
            CHECK( subscriberEventCount == 1 );
            CHECK( publisherEventCount == 1 );
            f.disconnect();
        });
        iosvc.run();
    }

    WHEN( "using subscriber black/white listing" )
    {
        auto subscriber2 = CoroSession<>::create(iosvc, tcp(iosvc));

        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            int eventCount1 = 0;
            int eventCount2 = 0;

            f.join(yield);
            subscriber2->connect(yield);
            auto subscriber2Id =
                    subscriber2->join(Realm(testRealm), yield).id();

            f.subscriber->subscribe(
                Topic("onEvent"),
                [&eventCount1](Event) {++eventCount1;},
                yield);

            subscriber2->subscribe(
                Topic("onEvent"),
                [&eventCount2](Event) {++eventCount2;},
                yield);

            // Blacklist subscriber2
            f.publisher->publish(Pub("onEvent").withBlacklist({subscriber2Id}),
                                 yield);
            while (eventCount1 < 1)
                f.publisher->suspend(yield);
            CHECK( eventCount1 == 1 );
            CHECK( eventCount2 == 0 );

            // Whitelist subscriber2
            f.publisher->publish(Pub("onEvent").withWhitelist({subscriber2Id}),
                                 yield);
            while (eventCount2 < 1)
                f.publisher->suspend(yield);
            CHECK( eventCount1 == 1 );
            CHECK( eventCount2 == 1 );

            f.disconnect();
            subscriber2->disconnect();
        });
        iosvc.run();
    }
}}

//------------------------------------------------------------------------------
SCENARIO( "WAMP ticket authentication", "[WAMP]" )
{
GIVEN( "a Session with a registered challenge handler" )
{
    AsioService iosvc;
    TicketAuthFixture f(iosvc, authTcp(iosvc));

    WHEN( "joining with ticket authentication requested" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            f.join("alice", "password123", yield);
            f.session->disconnect();
        });
        iosvc.run();

        THEN( "the challenge was received and the authentication accepted" )
        {
            REQUIRE( f.challengeCount == 1 );
            CHECK( f.challengeState == SessionState::authenticating );
            CHECK( f.challenge.method() == "ticket" );
            CHECK( f.info.optionByKey("authmethod") == "ticket" );
            CHECK( f.info.optionByKey("authrole") == "ticketrole" );
        }
    }
}}

//------------------------------------------------------------------------------
SCENARIO( "RPC Cancellation", "[WAMP]" )
{
GIVEN( "a caller and a callee" )
{
    AsioService iosvc;
    RpcFixture f(iosvc, tcp(iosvc));

    WHEN( "cancelling an RPC in kill mode before it returns" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            RequestId callRequestId = 0;
            RequestId invocationRequestId = 0;
            RequestId interruptionRequestId = 0;
            bool responseReceived = false;
            AsyncResult<Result> response;

            f.join(yield);

            f.callee->enroll(
                Procedure("rpc"),
                [&invocationRequestId](Invocation inv) -> Outcome
                {
                    invocationRequestId = inv.requestId();
                    return Outcome::deferred();
                },
                [&interruptionRequestId](Interruption intr) -> Outcome
                {
                    interruptionRequestId = intr.requestId();
                    return Error("wamp.error.canceled");
                },
                yield);

            callRequestId = f.caller->call(Rpc("rpc"),
                [&response, &responseReceived](AsyncResult<Result> callResponse)
                {
                    responseReceived = true;
                    response = std::move(callResponse);
                });

            REQUIRE( callRequestId != 0 );

            while (invocationRequestId == 0)
                f.caller->suspend(yield);

            REQUIRE( invocationRequestId != 0 );

            f.caller->cancel(Cancellation(callRequestId, CancelMode::kill));

            while (!responseReceived)
                f.caller->suspend(yield);

            CHECK( interruptionRequestId == invocationRequestId );
            CHECK( response.errorCode() == SessionErrc::cancelled );

            f.disconnect();
        });
        iosvc.run();
    }

    WHEN( "cancelling an RPC after it returns" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            RequestId callRequestId = 0;
            RequestId invocationRequestId = 0;
            RequestId interruptionRequestId = 0;
            bool responseReceived = false;
            AsyncResult<Result> response;

            f.join(yield);

            f.callee->enroll(
                Procedure("rpc"),
                [&invocationRequestId](Invocation inv) -> Outcome
                {
                    invocationRequestId = inv.requestId();
                    return Result{Variant{"completed"}};
                },
                [&interruptionRequestId](Interruption intr) -> Outcome
                {
                    interruptionRequestId = intr.requestId();
                    return Error("wamp.error.canceled");
                },
                yield);

            callRequestId = f.caller->call(Rpc("rpc"),
                [&response, &responseReceived](AsyncResult<Result> callResponse)
                {
                    responseReceived = true;
                    response = std::move(callResponse);
                });

            while (!responseReceived)
                f.caller->suspend(yield);

            REQUIRE( response.get().args() == Array{Variant{"completed"}} );

            f.caller->cancel(Cancellation(callRequestId, CancelMode::kill));

            /* Router should not treat late CANCEL as a protocol error, and
               should allow clients to continue calling RPCs. */
            f.caller->call(Rpc("rpc"), yield);

            /* Router should discard INTERRUPT messages for non-pending RPCs. */
            CHECK( interruptionRequestId == 0 );

            f.disconnect();
        });
        iosvc.run();
    }

    // TODO: Test other cancel modes once they're supported by Crossbar
}}

#endif // #if CPPWAMP_TESTING_WAMP
