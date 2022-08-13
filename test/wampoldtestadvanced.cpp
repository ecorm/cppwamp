/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

// TODO: Remove this source file once CoroSession is removed

#if defined(CPPWAMP_TEST_HAS_CORO)

#include <algorithm>
#include <catch2/catch.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/msgpack.hpp>
#include <cppwamp/tcp.hpp>
#include <cppwamp/coro/corosession.hpp>

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning( disable : 4996 )
#endif

using namespace wamp;
using namespace Catch::Matchers;

namespace
{

const std::string testRealm = "cppwamp.test";
const short testPort = 12345;
const std::string authTestRealm = "cppwamp.authtest";
const short authTestPort = 23456;

Connector::Ptr tcp(AsioContext& ioctx)
{
    return connector<Json>(ioctx, TcpHost("localhost", testPort));
}

Connector::Ptr authTcp(AsioContext& ioctx)
{
    return connector<Json>(ioctx, TcpHost("localhost", authTestPort));
}

//------------------------------------------------------------------------------
struct RpcFixture
{
    template <typename TConnector>
    RpcFixture(AsioContext& ioctx, TConnector cnct)
        : caller(CoroSession<>::create(ioctx, cnct)),
          callee(CoroSession<>::create(ioctx, cnct))
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
    PubSubFixture(AsioContext& ioctx, TConnector cnct)
        : publisher(CoroSession<>::create(ioctx, cnct)),
          subscriber(CoroSession<>::create(ioctx, cnct))
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
    TicketAuthFixture(AsioContext& ioctx, TConnector cnct)
        : session(CoroSession<>::create(ioctx, cnct))
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
        authChallenge.authenticate(Authentication(signature));
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
SCENARIO( "Old WAMP RPC advanced features", "[OldWAMP][Advanced]" )
{
GIVEN( "a caller and a callee" )
{
    AsioContext ioctx;
    RpcFixture f(ioctx, tcp(ioctx));

    WHEN( "using caller identification" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
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
        ioctx.run();
    }

    WHEN( "using pattern-based registrations" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
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
        ioctx.run();
    }
}}

//------------------------------------------------------------------------------
SCENARIO( "Old WAMP progressive call results", "[OldWAMP][Advanced]" )
{
GIVEN( "a caller and a callee" )
{
    AsioContext ioctx;
    RpcFixture f(ioctx, tcp(ioctx));

    WHEN( "using progressive call results" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            std::vector<int> input{9, 3, 7, 5};
            std::vector<int> output;

            f.join(yield);

            f.callee->enroll(
                Procedure("com.myapp.foo"),
                [&ioctx, &input](Invocation inv) -> Outcome
                {
                    CHECK( inv.isProgressive() );

                    boost::asio::spawn(
                        inv.executor(),
                        [&ioctx, &input, inv](boost::asio::yield_context yield)
                    {
                        boost::asio::steady_timer timer(ioctx);

                        for (unsigned i=0; i<input.size(); ++i)
                        {
                            // Simulate a streaming app that throttles
                            // the intermediary results at a fixed rate.
                            timer.expires_from_now(
                                std::chrono::milliseconds(25));
                            timer.async_wait(yield);

                            Result result({input.at(i)});
                            if (i < (input.size() - 1))
                                result.withProgress();
                            inv.yield(result);
                        }
                    });
                    return Outcome::deferred();
                },
                yield);

            for (unsigned i=0; i<2; ++i)
            {
                f.caller->call(
                    Rpc("com.myapp.foo").withProgressiveResults(),
                    [&output, &input](AsyncResult<Result> r)
                    {
                        const auto& result = r.get();
                        auto n = result.args().at(0).to<int>();
                        output.push_back(n);
                        bool progressiveExpected = output.size() < input.size();
                        CHECK( result.isProgressive() == progressiveExpected );
                    });

                while (output.size() < input.size())
                    f.caller->suspend(yield);
                CHECK( input == output );
                output.clear();
            }

            f.disconnect();
        });
        ioctx.run();
    }

    WHEN( "returning an error instead of a progressive call result" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            std::vector<int> input{9, 3, 7, 5};
            std::vector<int> output;

            f.join(yield);

            f.callee->enroll(
                Procedure("com.myapp.foo"),
                [&ioctx, &input](Invocation inv) -> Outcome
                {
                    CHECK( inv.isProgressive() );

                    boost::asio::spawn(
                        inv.executor(),
                        [&ioctx, &input, inv](boost::asio::yield_context yield)
                    {
                        boost::asio::steady_timer timer(ioctx);

                        for (unsigned i=0; i<input.size(); ++i)
                        {
                            // Simulate a streaming app that throttles
                            // the intermediary results at a fixed rate.
                            timer.expires_from_now(
                                std::chrono::milliseconds(25));
                            timer.async_wait(yield);

                            Result result({input.at(i)});
                            result.withProgress();
                            inv.yield(result);
                        }

                        timer.expires_from_now(
                            std::chrono::milliseconds(25));
                        inv.yield(Error("some_reason"));
                    });
                    return Outcome::deferred();
                },
                yield);

            for (unsigned i=0; i<2; ++i)
            {
                Error error;
                bool receivedError = false;
                f.caller->call(
                    Rpc("com.myapp.foo")
                        .withProgressiveResults()
                        .captureError(error),
                    [&output, &input, &receivedError](AsyncResult<Result> r)
                    {
                        if (output.size() == input.size())
                        {
                            CHECK(r.errorCode() == SessionErrc::callError);
                            receivedError = true;
                            return;
                        }
                        const auto& result = r.get();
                        auto n = result.args().at(0).to<int>();
                        output.push_back(n);
                        CHECK( result.isProgressive() );
                    });

                while (!receivedError)
                    f.caller->suspend(yield);
                CHECK( input == output );
                CHECK( error.reason() == "some_reason" );
                output.clear();
            }

            f.disconnect();
        });
        ioctx.run();
    }

    WHEN( "caller leaves during progressive call results" )
    {
        bool interrupted = false;

        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            std::vector<int> output;
            int tickCount = 0;

            f.join(yield);

            f.callee->enroll(
                Procedure("com.myapp.foo"),
                [&](Invocation inv) -> Outcome
                {
                    CHECK( inv.isProgressive() );
                    boost::asio::spawn(
                        inv.executor(),
                        [&, inv](boost::asio::yield_context yield)
                        {
                            boost::asio::steady_timer timer(ioctx);

                            while (!interrupted)
                            {
                                timer.expires_from_now(
                                    std::chrono::milliseconds(50));
                                timer.async_wait(yield);

                                Result result({tickCount});
                                result.withProgress();
                                ++tickCount;
                                inv.yield(result);
                            }
                        });
                    return Outcome::deferred();
                },
                [&interrupted](Interruption intr) -> Outcome
                {
                    interrupted = true;
                    return wamp::Error("wamp.error.canceled");
                },
                yield);

            f.caller->call(
                Rpc("com.myapp.foo").withProgressiveResults(),
                [&output](AsyncResult<Result> r)
                {
                    if (r.errorCode() == SessionErrc::sessionEnded)
                        return;
                    const auto& result = r.get();
                    auto n = result.args().at(0).to<int>();
                    output.push_back(n);
                    CHECK( result.isProgressive() );
                });

            while (output.size() < 2)
                f.caller->suspend(yield);
            f.caller->leave(yield);

            while (!interrupted)
                f.caller->suspend(yield);
            CHECK(output.size() == 2);
            CHECK(tickCount == 2);

            f.disconnect();
        });
        ioctx.run();
    }
}}

//------------------------------------------------------------------------------
SCENARIO( "Old RPC Cancellation", "[OldWAMP][Advanced]" )
{
GIVEN( "a caller and a callee" )
{
    AsioContext ioctx;
    RpcFixture f(ioctx, tcp(ioctx));

    WHEN( "cancelling an RPC in kill mode before it returns" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
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

            f.caller->cancel(CallCancellation(callRequestId,
                                              CallCancelMode::kill));

            while (!responseReceived)
                f.caller->suspend(yield);

            CHECK( interruptionRequestId == invocationRequestId );
            CHECK( response.errorCode() == SessionErrc::cancelled );

            f.disconnect();
        });
        ioctx.run();
    }

    WHEN( "cancelling an RPC in killnowait mode before it returns" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
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

            f.caller->cancel(CallCancellation(callRequestId,
                                              CallCancelMode::killNoWait));

            while (!responseReceived || interruptionRequestId == 0)
                f.caller->suspend(yield);

            CHECK( interruptionRequestId == invocationRequestId );
            CHECK( response.errorCode() == SessionErrc::cancelled );

            f.disconnect();
        });
        ioctx.run();
    }

// Skip mode cancellation currently does not work properly with Crossbar.
// https://github.com/crossbario/crossbar/issues/1377#issuecomment-1123050045
#if 0
    WHEN( "cancelling an RPC in skip mode before it returns" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            RequestId callRequestId = 0;
            RequestId invocationRequestId = 0;
            bool responseReceived = false;
            bool interruptionReceived = false;
            AsyncResult<Result> response;
            Invocation invocation;

            f.join(yield);

            f.callee->enroll(
                Procedure("rpc"),
                [&invocationRequestId, &invocation](Invocation inv) -> Outcome
                {
                    invocationRequestId = inv.requestId();
                    invocation = std::move(inv);
                    return Outcome::deferred();
                },
                [&interruptionReceived](Interruption intr) -> Outcome
                {
                    interruptionReceived = true;
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

            f.caller->cancel(Cancellation(callRequestId, CallCancelMode::skip));

            while (!responseReceived)
                f.caller->suspend(yield);
            invocation.yield(); // Will be discarded by router

            CHECK_FALSE( interruptionReceived );
            CHECK( response.errorCode() == SessionErrc::cancelled );

            f.disconnect();
        });
        ioctx.run();
    }
#endif

    WHEN( "cancelling an RPC after it returns" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
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

            f.caller->cancel(CallCancellation(callRequestId,
                                              CallCancelMode::kill));

            /* Router should not treat late CANCEL as a protocol error, and
               should allow clients to continue calling RPCs. */
            f.caller->call(Rpc("rpc"), yield);

            /* Router should discard INTERRUPT messages for non-pending RPCs. */
            CHECK( interruptionRequestId == 0 );

            f.disconnect();
        });
        ioctx.run();
    }
}}

//------------------------------------------------------------------------------
SCENARIO( "Old Caller-initiated timeouts", "[OldWAMP][Advanced]" )
{
GIVEN( "a caller and a callee" )
{
    AsioContext ioctx;
    RpcFixture f(ioctx, tcp(ioctx));

    WHEN( "the caller initiates timeouts" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            std::vector<AsyncResult<Result>> results;
            std::vector<RequestId> interruptions;
            std::map<RequestId, int> valuesByRequestId;

            f.join(yield);

            f.callee->enroll(
                Procedure("com.myapp.foo"),
                [&](Invocation inv) -> Outcome
                {
                    boost::asio::spawn(
                        inv.executor(),
                        [&, inv](boost::asio::yield_context yield)
                        {
                            int arg = 0;
                            inv.convertTo(arg);
                            valuesByRequestId[inv.requestId()] = arg;
                            boost::asio::steady_timer timer(ioctx);
                            timer.expires_from_now(std::chrono::milliseconds(150));
                            timer.async_wait(yield);

                            bool interrupted =
                                std::count(interruptions.begin(),
                                           interruptions.end(),
                                           inv.requestId()) != 0;
                            if (interrupted)
                                inv.yield(Error("wamp.error.canceled"));
                            else
                                inv.yield({arg});
                        });

                    return Outcome::deferred();
                },
                [&](Interruption intr) -> Outcome
                {
                    interruptions.push_back(intr.requestId());
                    return Outcome::deferred();
                },
                yield);

            auto callHandler = [&results](AsyncResult<Result> r)
            {
                results.emplace_back(std::move(r));
            };

            for (int i=0; i<2; ++i)
            {
                f.caller->call(
                    Rpc("com.myapp.foo")
                        .withArgs(1)
                        .withCallerTimeout(std::chrono::milliseconds(100)),
                    callHandler);

                f.caller->call(
                    Rpc("com.myapp.foo").withArgs(2).withCallerTimeout(50),
                    callHandler);

                f.caller->call(
                    Rpc("com.myapp.foo").withArgs(3),
                    callHandler);

                while (results.size() < 3)
                    f.caller->suspend(yield);

                REQUIRE( results.size() == 3 );
                CHECK( results[0].errorCode() == SessionErrc::cancelled );
                CHECK( results[1].errorCode() == SessionErrc::cancelled );
                CHECK( results[2].get().args().at(0).to<int>() == 3 );
                REQUIRE( interruptions.size() == 2 );
                CHECK( valuesByRequestId[interruptions[0]] == 2 );
                CHECK( valuesByRequestId[interruptions[1]] == 1 );

                results.clear();
                interruptions.clear();
                valuesByRequestId.clear();
            }

            f.disconnect();
        });
        ioctx.run();
    }
}}

//------------------------------------------------------------------------------
SCENARIO( "Old WAMP pub/sub advanced features", "[OldWAMP][Advanced]" )
{
GIVEN( "a publisher and a subscriber" )
{
    AsioContext ioctx;
    PubSubFixture f(ioctx, tcp(ioctx));

    WHEN( "using publisher identification" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
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
        ioctx.run();
    }

    WHEN( "using pattern-based subscriptions" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
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
        ioctx.run();
    }

    WHEN( "using publisher exclusion" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
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
        ioctx.run();
    }

    WHEN( "using subscriber black/white listing" )
    {
        auto subscriber2 = CoroSession<>::create(ioctx, tcp(ioctx));

        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
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

            // Block subscriber2
            f.publisher->publish(Pub("onEvent")
                                     .withExcludedSessions({subscriber2Id}),
                                 yield);
            while (eventCount1 < 1)
                f.publisher->suspend(yield);
            CHECK( eventCount1 == 1 );
            CHECK( eventCount2 == 0 );

            // Allow subscriber2
            f.publisher->publish(Pub("onEvent")
                                     .withEligibleSessions({subscriber2Id}),
                                 yield);
            while (eventCount2 < 1)
                f.publisher->suspend(yield);
            CHECK( eventCount1 == 1 );
            CHECK( eventCount2 == 1 );

            f.disconnect();
            subscriber2->disconnect();
        });
        ioctx.run();
    }
}}

//------------------------------------------------------------------------------
SCENARIO( "Old WAMP ticket authentication", "[OldWAMP][Advanced]" )
{
GIVEN( "a Session with a registered challenge handler" )
{
    AsioContext ioctx;
    TicketAuthFixture f(ioctx, authTcp(ioctx));

    WHEN( "joining with ticket authentication requested" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            f.join("alice", "password123", yield);
            f.session->disconnect();
        });
        ioctx.run();

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

#endif // defined(CPPWAMP_TEST_HAS_CORO)
