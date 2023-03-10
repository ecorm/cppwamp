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
#include <cppwamp/tcp.hpp>

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

void suspendCoro(YieldContext& yield)
{
    auto exec = boost::asio::get_associated_executor(yield);
    boost::asio::post(exec, yield);
}

//------------------------------------------------------------------------------
struct RpcFixture
{
    RpcFixture(IoContext& ioctx, ConnectionWish wish)
        : where(std::move(wish)),
          caller(ioctx),
          callee(ioctx)
    {}

    void join(YieldContext yield)
    {
        caller.connect(where, yield).value();
        callerId = caller.join(Realm(testRealm), yield).value().id();
        callee.connect(where, yield).value();
        callee.join(Realm(testRealm), yield).value();
    }

    void disconnect()
    {
        caller.disconnect();
        callee.disconnect();
    }

    ConnectionWish where;

    Session caller;
    Session callee;

    SessionId callerId = -1;
};

//------------------------------------------------------------------------------
struct PubSubFixture
{
    PubSubFixture(IoContext& ioctx, ConnectionWish wish)
        : where(std::move(wish)),
          publisher(ioctx),
          subscriber(ioctx)
    {}

    void join(YieldContext yield)
    {
        publisher.connect(where, yield).value();
        publisherId = publisher.join(Realm(testRealm), yield).value().id();
        subscriber.connect(where, yield).value();
        subscriber.join(Realm(testRealm), yield).value();
    }

    void disconnect()
    {
        publisher.disconnect();
        subscriber.disconnect();
    }

    ConnectionWish where;

    Session publisher;
    Session subscriber;

    SessionId publisherId = -1;
    SessionId subscriberId = -1;
};

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
        info = session.join(Realm(authTestRealm).withAuthMethods({"ticket"})
                                                .withAuthId(std::move(authId))
                                                .captureAbort(abortReason),
                            [this](Challenge c) {onChallenge(std::move(c));},
                            yield);
    }

    void onChallenge(Challenge authChallenge)
    {
        ++challengeCount;
        challenge = authChallenge;
        challengeState = session.state();
        authChallenge.authenticate(Authentication(signature));
    }

    ConnectionWish where;
    Session session;
    String signature;
    SessionState challengeState = SessionState::closed;
    unsigned challengeCount = 0;
    Challenge challenge;
    ErrorOr<Welcome> info;
    Reason abortReason;
};

} // anonymous namespace


//------------------------------------------------------------------------------
SCENARIO( "WAMP RPC advanced features", "[WAMP][Advanced]" )
{
GIVEN( "a caller and a callee" )
{
    IoContext ioctx;
    RpcFixture f(ioctx, withTcp);

    WHEN( "using caller identification" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            SessionId disclosedId = -1;

            f.join(yield);

            f.callee.enroll(
                Procedure("rpc"),
                [&disclosedId](Invocation inv) -> Outcome
                {
                    disclosedId = inv.caller().value_or(0);
                    return {};
                },
                yield).value();

            f.caller.call(Rpc("rpc").withDiscloseMe(), yield).value();
            CHECK( disclosedId == f.callerId );
            f.disconnect();
        });
        ioctx.run();
    }

    WHEN( "using pattern-based registrations" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            int prefixMatchCount = 0;
            int wildcardMatchCount = 0;

            f.join(yield);

            f.callee.enroll(
                Procedure("com.myapp").withMatchPolicy(MatchPolicy::prefix),
                [&prefixMatchCount](Invocation inv) -> Outcome
                {
                    ++prefixMatchCount;
                    CHECK_THAT( inv.procedure().value_or(""),
                                Equals("com.myapp.foo") );
                    return {};
                },
                yield).value();

            f.callee.enroll(
                Procedure("com.other..rpc")
                            .withMatchPolicy(MatchPolicy::wildcard),
                [&wildcardMatchCount](Invocation inv) -> Outcome
                {
                    ++wildcardMatchCount;
                    CHECK_THAT( inv.procedure().value_or(""),
                                Equals("com.other.foo.rpc") );
                    return {};
                },
                yield).value();

            f.caller.call(Rpc("com.myapp.foo"), yield).value();
            CHECK( prefixMatchCount == 1 );
            CHECK( wildcardMatchCount == 0 );

            f.caller.call(Rpc("com.other.foo.rpc"), yield).value();
            CHECK( prefixMatchCount == 1 );
            CHECK( wildcardMatchCount == 1 );

            f.disconnect();
        });
        ioctx.run();
    }
}}

//------------------------------------------------------------------------------
SCENARIO( "RPC Cancellation", "[WAMP][Advanced]" )
{
GIVEN( "a caller and a callee" )
{
    IoContext ioctx;
    RpcFixture f(ioctx, withTcp);

    WHEN( "cancelling an RPC in kill mode before it returns" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            CallCancellationSignal sig;
            RequestId invocationRequestId = 0;
            RequestId interruptionRequestId = 0;
            bool responseReceived = false;
            ErrorOr<Result> response;

            f.join(yield);

            f.callee.enroll(
                Procedure("rpc"),
                [&invocationRequestId](Invocation inv) -> Outcome
                {
                    invocationRequestId = inv.requestId();
                    return deferment;
                },
                [&interruptionRequestId](Interruption intr) -> Outcome
                {
                    interruptionRequestId = intr.requestId();
                    return Error{WampErrc::cancelled};
                },
                yield).value();

            auto slot = sig.slot();
            CHECK(slot.is_connected());

            f.caller.call(
                Rpc("rpc").withCancellationSlot(slot),
                [&response, &responseReceived](ErrorOr<Result> callResponse)
                {
                    responseReceived = true;
                    response = std::move(callResponse);
                });

            CHECK( slot.has_handler() );

            while (invocationRequestId == 0)
                suspendCoro(yield);

            REQUIRE( invocationRequestId != 0 );

            sig.emit(CallCancelMode::kill);

            while (!responseReceived)
                suspendCoro(yield);

            CHECK( interruptionRequestId == invocationRequestId );
            CHECK( response == makeUnexpected(WampErrc::cancelled) );

            f.disconnect();
        });
        ioctx.run();
    }

    WHEN( "cancelling an RPC in kill mode via a handler-bound slot "
          "before it returns" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            boost::asio::cancellation_signal cancelSignal;
            RequestId invocationRequestId = 0;
            RequestId interruptionRequestId = 0;
            bool responseReceived = false;
            ErrorOr<Result> response;

            f.join(yield);

            f.callee.enroll(
                Procedure("rpc"),
                [&invocationRequestId](Invocation inv) -> Outcome
                {
                    invocationRequestId = inv.requestId();
                    return deferment;
                },
                [&interruptionRequestId](Interruption intr) -> Outcome
                {
                    interruptionRequestId = intr.requestId();
                    return Error{WampErrc::cancelled};
                },
                yield).value();

            f.caller.call(
                Rpc("rpc").withCancelMode(CallCancelMode::kill),
                boost::asio::bind_cancellation_slot(
                    cancelSignal.slot(),
                    [&response, &responseReceived](ErrorOr<Result> callResponse)
                    {
                        responseReceived = true;
                        response = std::move(callResponse);
                    }));

            while (invocationRequestId == 0)
                suspendCoro(yield);

            REQUIRE( invocationRequestId != 0 );

            cancelSignal.emit(boost::asio::cancellation_type::total);

            while (!responseReceived)
                suspendCoro(yield);

            CHECK( interruptionRequestId == invocationRequestId );
            CHECK( response == makeUnexpected(WampErrc::cancelled) );

            f.disconnect();
        });
        ioctx.run();
    }

    WHEN( "cancelling via an handler-bound slot when calling with a "
          "stackful coroutine completion token")
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            boost::asio::cancellation_signal cancelSignal;
            RequestId invocationRequestId = 0;
            RequestId interruptionRequestId = 0;
            ErrorOr<Result> response;

            f.join(yield);

            f.callee.enroll(
                Procedure("rpc"),
                [&invocationRequestId](Invocation inv) -> Outcome
                {
                    invocationRequestId = inv.requestId();
                    return deferment;
                },
                [&interruptionRequestId](Interruption intr) -> Outcome
                {
                    interruptionRequestId = intr.requestId();
                    return Error{WampErrc::cancelled};
                },
                yield).value();

            boost::asio::steady_timer timer(ioctx);
            timer.expires_from_now(std::chrono::milliseconds(50));
            timer.async_wait(
                [&cancelSignal](boost::system::error_code)
                {
                    cancelSignal.emit(boost::asio::cancellation_type::total);
                });

            auto result = f.caller.call(
                Rpc("rpc").withCancelMode(CallCancelMode::kill),
                boost::asio::bind_cancellation_slot(cancelSignal.slot(),
                                                    yield));

            CHECK( result == makeUnexpectedError(WampErrc::cancelled) );
            CHECK( interruptionRequestId == invocationRequestId );

            f.disconnect();
        });
        ioctx.run();
    }

    // TODO: Cancelling an RPC in kill mode with no callee interruption handler.

    WHEN( "cancelling an RPC in killnowait mode before it returns" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            CallCancellationSignal sig;
            RequestId invocationRequestId = 0;
            RequestId interruptionRequestId = 0;
            bool responseReceived = false;
            ErrorOr<Result> response;

            f.join(yield);

            f.callee.enroll(
                Procedure("rpc"),
                [&invocationRequestId](Invocation inv) -> Outcome
                {
                    invocationRequestId = inv.requestId();
                    return deferment;
                },
                [&interruptionRequestId](Interruption intr) -> Outcome
                {
                    interruptionRequestId = intr.requestId();
                    return Error{WampErrc::cancelled};
                },
                yield).value();

            f.caller.call(
                Rpc("rpc").withCancellationSlot(sig.slot()),
                [&response, &responseReceived](ErrorOr<Result> callResponse)
                {
                    responseReceived = true;
                    response = std::move(callResponse);
                });

            while (invocationRequestId == 0)
                suspendCoro(yield);

            REQUIRE( invocationRequestId != 0 );

            sig.emit(CallCancelMode::killNoWait);

            while (!responseReceived || interruptionRequestId == 0)
                suspendCoro(yield);

            CHECK( interruptionRequestId == invocationRequestId );
            CHECK( response == makeUnexpected(WampErrc::cancelled) );

            f.disconnect();
        });
        ioctx.run();
    }

// Skip mode cancellation currently does not work properly with Crossbar.
// https://github.com/crossbario/crossbar/issues/1377#issuecomment-1123050045
#if 0
    WHEN( "cancelling an RPC in skip mode before it returns" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            CallCancellationSignal sig;
            RequestId invocationRequestId = 0;
            bool responseReceived = false;
            bool interruptionReceived = false;
            ErrorOr<Result> response;
            Invocation invocation;

            f.join(yield);

            f.callee.enroll(
                Procedure("rpc"),
                [&invocationRequestId, &invocation](Invocation inv) -> Outcome
                {
                    invocationRequestId = inv.requestId();
                    invocation = std::move(inv);
                    return deferment;
                },
                [&interruptionReceived](Interruption intr) -> Outcome
                {
                    interruptionReceived = true;
                    return Error("wamp.error.canceled");
                },
                yield).value();

            f.caller.call(
                Rpc("rpc").withCancellationSlot(sig.slot()),
                [&response, &responseReceived](ErrorOr<Result> callResponse)
                {
                    responseReceived = true;
                    response = std::move(callResponse);
                });

            while (invocationRequestId == 0)
                suspendCoro(yield);
            REQUIRE( invocationRequestId != 0 );

            sig.emit(CallCancelMode::skip);

            while (!responseReceived)
                suspendCoro(yield);
            invocation.yield(); // Will be discarded by router

            CHECK_FALSE( interruptionReceived );
            CHECK( response == makeUnexpected(WampErrc::cancelled) );

            f.disconnect();
        });
        ioctx.run();
    }
#endif

    WHEN( "cancelling an RPC after it returns" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            CallCancellationSignal sig;
            RequestId invocationRequestId = 0;
            RequestId interruptionRequestId = 0;
            bool responseReceived = false;
            ErrorOr<Result> response;

            f.join(yield);

            f.callee.enroll(
                Procedure("rpc"),
                [&invocationRequestId](Invocation inv) -> Outcome
                {
                    invocationRequestId = inv.requestId();
                    return Result{Variant{"completed"}};
                },
                [&interruptionRequestId](Interruption intr) -> Outcome
                {
                    interruptionRequestId = intr.requestId();
                    return Error{WampErrc::cancelled};
                },
                yield).value();

            f.caller.call(
                Rpc("rpc").withCancellationSlot(sig.slot()),
                [&response, &responseReceived](ErrorOr<Result> callResponse)
                {
                    responseReceived = true;
                    response = std::move(callResponse);
                });

            while (!responseReceived)
                suspendCoro(yield);

            REQUIRE( response.value().args() == Array{Variant{"completed"}} );

            sig.emit(CallCancelMode::kill);

            /* Router should not treat late CANCEL as a protocol error, and
               should allow clients to continue calling RPCs. */
            f.caller.call(Rpc("rpc"), yield).value();

            /* Router should discard INTERRUPT messages for non-pending RPCs. */
            CHECK( interruptionRequestId == 0 );

            f.disconnect();
        });
        ioctx.run();
    }
}}

//------------------------------------------------------------------------------
SCENARIO( "Caller-initiated timeouts", "[WAMP][Advanced]" )
{
GIVEN( "a caller and a callee" )
{
    IoContext ioctx;
    RpcFixture f(ioctx, withTcp);

    WHEN( "the caller initiates timeouts" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            std::vector<ErrorOr<Result>> results;
            std::vector<RequestId> interruptions;
            std::map<RequestId, int> valuesByRequestId;

            f.join(yield);

            f.callee.enroll(
                Procedure("com.myapp.foo"),
                [&](Invocation inv) -> Outcome
                {
                    spawn(ioctx, [&, inv](YieldContext yield)
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
                            inv.yield(Error{WampErrc::cancelled});
                        else
                            inv.yield({arg});
                    });

                    return deferment;
                },
                [&](Interruption intr) -> Outcome
                {
                    interruptions.push_back(intr.requestId());
                    return deferment;
                },
                yield).value();

            auto callHandler = [&results](ErrorOr<Result> r)
            {
                results.emplace_back(std::move(r));
            };

            for (int i=0; i<2; ++i)
            {
                f.caller.call(
                    Rpc("com.myapp.foo")
                        .withArgs(1)
                        .withCallerTimeout(std::chrono::milliseconds(100)),
                    callHandler);

                f.caller.call(
                    Rpc("com.myapp.foo")
                        .withArgs(2)
                        .withCallerTimeout(std::chrono::milliseconds(50)),
                    callHandler);

                f.caller.call(
                    Rpc("com.myapp.foo").withArgs(3),
                    callHandler);

                while (results.size() < 3)
                    suspendCoro(yield);

                REQUIRE( results.size() == 3 );
                CHECK( results[0] == makeUnexpected(WampErrc::cancelled) );
                CHECK( results[1] == makeUnexpected(WampErrc::cancelled) );
                CHECK( results[2].value().args().at(0).to<int>() == 3 );
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
SCENARIO( "WAMP callee-to-caller streaming with invitations",
          "[WAMP][Advanced]" )
{
GIVEN( "a caller and a callee" )
{
    IoContext ioctx;
    RpcFixture f(ioctx, withTcp);

    std::vector<int> input{9, 3, 7, 5};
    std::vector<int> output;
    bool errorArmed = false;
    bool rejectArmed = false;
    bool leaveEarlyArmed = false;
    bool destroyEarlyArmed = false;

    auto onStream = [&](CalleeChannel channel)
    {
        CHECK( channel.mode() == StreamMode::calleeToCaller );
        CHECK( channel.invitationExpected() );
        CHECK( channel.invitation().args().front().as<String>() ==
              "invitation" );

        if (rejectArmed)
        {
            bool sent = channel.fail(WampErrc::invalidArgument).value();
            CHECK(sent);
            return;
        }

        auto rsvp = CalleeOutputChunk().withArgs("rsvp");
        bool sent = channel.accept(rsvp).value();
        CHECK(sent);

        spawn(
            ioctx,
            [&](YieldContext yield) mutable
            {
                CalleeChannel chan = std::move(channel);
                boost::asio::steady_timer timer(ioctx);

                for (unsigned i=0; i<input.size(); ++i)
                {
                    // Simulate a streaming app that throttles
                    // the intermediary results at a fixed rate.
                    timer.expires_from_now(std::chrono::milliseconds(25));
                    timer.async_wait(yield);

                    bool isFinal = (i == input.size() - 1);
                    if (isFinal && errorArmed)
                    {
                        chan.fail(Error{WampErrc::invalidArgument});
                    }
                    else if (isFinal && leaveEarlyArmed)
                    {
                        f.callee.leave(yield).value();
                    }
                    else if (isFinal && destroyEarlyArmed)
                    {
                        chan.detach();
                    }
                    else
                    {
                        chan.send(CalleeOutputChunk(isFinal)
                                       .withArgs(input.at(i))).value();
                    }
                }
            });
    };

    auto onChunk = [&](CallerChannel channel, ErrorOr<CallerInputChunk> chunk)
    {
        INFO("for output.size()=" << output.size());
        CHECK( channel.mode() == StreamMode::calleeToCaller );

        bool isFinal = output.size() == input.size() - 1;
        if (isFinal && errorArmed)
        {
            REQUIRE_FALSE( chunk.has_value() );
            CHECK(chunk.error() == WampErrc::invalidArgument);
            CHECK(channel.error().errorCode() == WampErrc::invalidArgument);
            output.push_back(input.back());
        }
        else if (isFinal && (leaveEarlyArmed || destroyEarlyArmed))
        {
            REQUIRE_FALSE( chunk.has_value() );
            CHECK(chunk.error() == WampErrc::cancelled);
            CHECK(channel.error().errorCode() == WampErrc::cancelled);
            output.push_back(input.back());
        }
        else
        {
            REQUIRE( chunk.has_value() );
            auto n = chunk->args().at(0).to<int>();
            output.push_back(n);
            CHECK( chunk->isFinal() == isFinal );
        }
    };

    auto runTest = [&]()
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            f.join(yield);
            f.callee.enroll(Stream("com.myapp.foo").withInvitationExpected(),
                            onStream, yield).value();

            for (unsigned i=0; i<2; ++i)
            {
                Error error;
                Invitation inv{"com.myapp.foo", StreamMode::calleeToCaller};
                inv.withArgs("invitation").captureError(error);
                auto channelOrError = f.caller.invite(inv, onChunk, yield);

                if (rejectArmed)
                {
                    CHECK(error.errorCode() == WampErrc::invalidArgument);
                    REQUIRE_FALSE(channelOrError.has_value());
                    CHECK(channelOrError.error() == WampErrc::invalidArgument);
                    break;
                }

                REQUIRE(channelOrError.has_value());
                auto channel = channelOrError.value();
                CHECK(channel.mode() == StreamMode::calleeToCaller);
                CHECK(channel.hasRsvp());
                CHECK(channel.rsvp().args().at(0).as<String>() == "rsvp");

                while (output.size() < input.size())
                    suspendCoro(yield);
                CHECK( input == output );
                output.clear();

                if (i == 0 && leaveEarlyArmed)
                {
                    f.callee.join(Realm(testRealm), yield).value();
                    f.callee.enroll(
                        Stream("com.myapp.foo").withInvitationExpected(),
                        onStream,
                        yield).value();
                }
            }

            f.disconnect();
        });
        ioctx.run();
    };

    WHEN( "streaming result chunks" )
    {
        runTest();
    }

    WHEN( "returning an error instead of a chunk" )
    {
        errorArmed = true;
        runTest();
    }

    WHEN( "rejecting an invitation with an error" )
    {
        rejectArmed = true;
        runTest();
    }

    WHEN( "callee leaves without sending final chunk" )
    {
        leaveEarlyArmed = true;
        runTest();
    }

    WHEN( "callee destroys channel without sending final chunk" )
    {
        destroyEarlyArmed = true;
        runTest();
    }
}}

//------------------------------------------------------------------------------
SCENARIO( "WAMP callee-to-caller streaming with no negotiation",
         "[WAMP][Advanced]" )
{
GIVEN( "a caller and a callee" )
{
    IoContext ioctx;
    RpcFixture f(ioctx, withTcp);

    std::vector<int> input{9, 3, 7, 5};
    std::vector<int> output;

    auto onStream = [&](CalleeChannel channel)
    {
        CHECK( channel.mode() == StreamMode::calleeToCaller );
        CHECK_FALSE( channel.invitationExpected() );
        CHECK_FALSE( channel.invitation().hasArgs() );
        channel.accept().value();

        spawn(
            ioctx,
            [&, channel](YieldContext yield) mutable
            {
                boost::asio::steady_timer timer(ioctx);

                for (unsigned i=0; i<input.size(); ++i)
                {
                    timer.expires_from_now(std::chrono::milliseconds(25));
                    timer.async_wait(yield);

                    bool isFinal = (i == input.size() - 1);
                    channel.send(CalleeOutputChunk(isFinal)
                                      .withArgs(input.at(i))).value();
                }
            });
    };

    auto onChunk = [&](CallerChannel channel, ErrorOr<CallerInputChunk> chunk)
    {
        INFO("for output.size()=" << output.size());
        CHECK( channel.mode() == StreamMode::calleeToCaller );

        bool isFinal = output.size() == input.size() - 1;
        REQUIRE( chunk.has_value() );
        auto n = chunk->args().at(0).to<int>();
        output.push_back(n);
        CHECK( chunk->isFinal() == isFinal );
    };

    WHEN( "streaming result chunks" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            f.join(yield);
            f.callee.enroll(Stream("com.myapp.foo"), onStream, yield).value();

            for (unsigned i=0; i<2; ++i)
            {
                Summons summons{"com.myapp.foo", StreamMode::calleeToCaller};
                summons.withArgs("I summon you");
                auto channelOrError = f.caller.summon(summons, onChunk);
                REQUIRE(channelOrError.has_value());
                auto channel = channelOrError.value();
                CHECK(channel.mode() == StreamMode::calleeToCaller);
                CHECK_FALSE(channel.hasRsvp());

                while (output.size() < input.size())
                    suspendCoro(yield);
                CHECK( input == output );
                output.clear();
            }

            f.disconnect();
        });
        ioctx.run();
    }
}}

//------------------------------------------------------------------------------
SCENARIO( "WAMP callee-to-caller streaming cancellation", "[WAMP][Advanced]" )
{
GIVEN( "a caller and a callee" )
{
    IoContext ioctx;
    RpcFixture f(ioctx, withTcp);
    boost::asio::steady_timer timer(ioctx);

    std::vector<int> input{9, 3, 7, 5};
    std::vector<int> output;
    bool interruptReceived = false;

    auto onInterrupt = [&](CalleeChannel channel, Interruption intr)
    {
        CHECK(intr.cancelMode() == CallCancelMode::killNoWait);
        channel.fail(WampErrc::cancelled);
        interruptReceived = true;
        timer.cancel();
    };

    auto onStream = [&](CalleeChannel channel)
    {
        CHECK( channel.mode() == StreamMode::calleeToCaller );
        channel.accept(CalleeOutputChunk().withArgs("rsvp"),
                        nullptr, onInterrupt).value();

        spawn(
            ioctx,
            [&, channel](YieldContext yield) mutable
            {
                // Never send the final chunk
                for (unsigned i=0; i<input.size()-1; ++i)
                {
                    timer.expires_from_now(std::chrono::milliseconds(25));
                    timer.async_wait(yield);
                    channel.send(CalleeOutputChunk(false)
                                      .withArgs(input.at(i))).value();
                }

                boost::system::error_code ec;
                timer.expires_from_now(std::chrono::seconds(3));
                timer.async_wait(yield[ec]);
                CHECK( interruptReceived );
                output.push_back(input.back());
            });
    };

    auto onChunk = [&](CallerChannel channel, ErrorOr<CallerInputChunk> chunk)
    {
        INFO("for output.size()=" << output.size());
        bool isFinal = output.size() == input.size() - 1;
        REQUIRE(!isFinal);
        auto n = chunk->args().at(0).to<int>();
        output.push_back(n);
    };

    auto runTest = [&](bool dropChannel)
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            f.join(yield);
            f.callee.enroll(Stream("com.myapp.foo").withInvitationExpected(),
                            onStream, yield).value();

            for (unsigned i=0; i<1; ++i)
            {
                Invitation inv{"com.myapp.foo", StreamMode::calleeToCaller};
                inv.withArgs("invitation");
                auto channelOrError = f.caller.invite(inv, onChunk, yield);
                REQUIRE(channelOrError.has_value());
                auto channel = std::move(channelOrError).value();

                while (output.size() < input.size() - 1)
                    suspendCoro(yield);
                REQUIRE_FALSE(interruptReceived);

                if (dropChannel)
                    channel.detach();
                else
                    channel.cancel(CallCancelMode::killNoWait);

                while (output.size() < input.size())
                    suspendCoro(yield);
                CHECK( input == output );
                output.clear();
                interruptReceived = false;
            }

            f.disconnect();
        });
        ioctx.run();
    };

    WHEN( "Cancelling via explicit cancel" )
    {
        runTest(false);
    }

    WHEN( "Cancelling by dropping the channel" )
    {
        runTest(true);
    }
}}

//------------------------------------------------------------------------------
SCENARIO( "WAMP callee-to-caller streaming with caller leaving",
          "[WAMP][Advanced]" )
{
GIVEN( "a caller and a callee" )
{
    IoContext ioctx;
    RpcFixture f(ioctx, withTcp);
    boost::asio::steady_timer timer(ioctx);

    std::vector<int> input{9, 3, 7, 5};
    std::vector<int> output;
    bool interruptReceived = false;
    bool errorReceived = true;

    auto onInterrupt = [&](CalleeChannel, Interruption intr)
    {
        CHECK(intr.cancelMode() == CallCancelMode::killNoWait);
        interruptReceived = true;
        timer.cancel();
    };

    auto onStream = [&](CalleeChannel channel)
    {
        CHECK( channel.mode() == StreamMode::calleeToCaller );
        channel.accept(CalleeOutputChunk().withArgs("rsvp"), nullptr,
                       onInterrupt).value();

        spawn(
            ioctx,
            [&, channel](YieldContext yield) mutable
            {
                // Don't mark the last chunk as final
                for (unsigned i=0; i<input.size(); ++i)
                {
                    timer.expires_from_now(std::chrono::milliseconds(25));
                    timer.async_wait(yield);
                    channel.send(CalleeOutputChunk()
                                     .withArgs(input.at(i))).value();
                }

                timer.expires_from_now(std::chrono::seconds(3));
                boost::system::error_code ec;
                timer.async_wait(yield[ec]);
                CHECK( interruptReceived );
                output.push_back(input.back());
            });
    };

    auto onChunk = [&](CallerChannel channel, ErrorOr<CallerInputChunk> chunk)
    {
        INFO("for output.size()=" << output.size());
        bool isFinal = output.size() == (input.size() - 1);
        if (!isFinal)
        {
            auto n = chunk->args().at(0).to<int>();
            output.push_back(n);
        }
        else if (chunk.has_value())
        {
            f.caller.leave([](ErrorOr<Reason>) {});
        }
        else
        {
            CHECK(chunk.error() == Errc::abandoned);
            CHECK(channel.error().errorCode() == WampErrc::unknown);
            errorReceived = true;
        }
    };

    WHEN( "streaming result chunks" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            f.join(yield);
            f.callee.enroll(Stream("com.myapp.foo").withInvitationExpected(),
                            onStream, yield).value();

            for (unsigned i=0; i<1; ++i)
            {
                Invitation inv{"com.myapp.foo", StreamMode::calleeToCaller};
                inv.withArgs("invitation");
                auto channelOrError = f.caller.invite(inv, onChunk, yield);
                REQUIRE(channelOrError.has_value());
                auto channel = channelOrError.value();

                while (output.size() < input.size())
                    suspendCoro(yield);
                CHECK( input == output );
                CHECK( interruptReceived );
                CHECK( errorReceived );
                output.clear();
                interruptReceived = false;
                errorReceived = false;
            }

            f.disconnect();
        });
        ioctx.run();
    }
}}

//------------------------------------------------------------------------------
SCENARIO( "WAMP pub/sub advanced features", "[WAMP][Advanced]" )
{
GIVEN( "a publisher and a subscriber" )
{
    IoContext ioctx;
    PubSubFixture f(ioctx, withTcp);

    WHEN( "using publisher identification" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            SessionId disclosedId = -1;
            int eventCount = 0;

            f.join(yield);

            f.subscriber.subscribe(
                Topic("onEvent"),
                [&disclosedId, &eventCount](Event event)
                {
                    disclosedId = event.publisher().value_or(0);
                    ++eventCount;
                },
                yield).value();

            f.publisher.publish(Pub("onEvent").withDiscloseMe(), yield).value();
            while (eventCount == 0)
                suspendCoro(yield);
            CHECK( disclosedId == f.publisherId );
            f.disconnect();
        });
        ioctx.run();
    }

    WHEN( "using pattern-based subscriptions" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            int prefixMatchCount = 0;
            int wildcardMatchCount = 0;
            std::string prefixTopic;
            std::string wildcardTopic;

            f.join(yield);

            f.subscriber.subscribe(
                Topic("com.myapp").withMatchPolicy(MatchPolicy::prefix),
                [&prefixMatchCount, &prefixTopic](Event event)
                {
                    prefixTopic = event.topic().value_or("");
                    ++prefixMatchCount;
                },
                yield).value();

            f.subscriber.subscribe(
                Topic("com..onEvent").withMatchPolicy(MatchPolicy::wildcard),
                [&wildcardMatchCount, &wildcardTopic](Event event)
                {
                    wildcardTopic = event.topic().value_or("");
                    ++wildcardMatchCount;
                },
                yield).value();

            f.publisher.publish(Pub("com.myapp.foo"), yield).value();
            while (prefixMatchCount < 1)
                suspendCoro(yield);
            CHECK( prefixMatchCount == 1 );
            CHECK_THAT( prefixTopic, Equals("com.myapp.foo") );
            CHECK( wildcardMatchCount == 0 );

            f.publisher.publish(Pub("com.foo.onEvent"), yield).value();
            while (wildcardMatchCount < 1)
                suspendCoro(yield);
            CHECK( prefixMatchCount == 1 );
            CHECK( wildcardMatchCount == 1 );
            CHECK_THAT( wildcardTopic, Equals("com.foo.onEvent") );

            f.publisher.publish(Pub("com.myapp.onEvent"), yield).value();
            while ((prefixMatchCount < 2) || (wildcardMatchCount < 2))
                suspendCoro(yield);
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
        spawn(ioctx, [&](YieldContext yield)
        {
            int subscriberEventCount = 0;
            int publisherEventCount = 0;

            f.join(yield);

            f.subscriber.subscribe(
                Topic("onEvent"),
                [&subscriberEventCount](Event) {++subscriberEventCount;},
                yield).value();

            f.publisher.subscribe(
                Topic("onEvent"),
                [&publisherEventCount](Event) {++publisherEventCount;},
                yield).value();

            f.publisher.publish(Pub("onEvent").withExcludeMe(false),
                                 yield).value();
            while ((subscriberEventCount < 1) || (publisherEventCount < 1))
                suspendCoro(yield);
            CHECK( subscriberEventCount == 1 );
            CHECK( publisherEventCount == 1 );
            f.disconnect();
        });
        ioctx.run();
    }

    WHEN( "using subscriber black/white listing" )
    {
        Session subscriber2(ioctx);

        spawn(ioctx, [&](YieldContext yield)
        {
            int eventCount1 = 0;
            int eventCount2 = 0;

            f.join(yield);
            subscriber2.connect(withTcp, yield).value();
            auto subscriber2Id =
                subscriber2.join(Realm(testRealm), yield).value().id();

            f.subscriber.subscribe(
                Topic("onEvent"),
                [&eventCount1](Event) {++eventCount1;},
                yield).value();

            subscriber2.subscribe(
                Topic("onEvent"),
                [&eventCount2](Event) {++eventCount2;},
                yield).value();

            // Block subscriber2
            f.publisher.publish(Pub("onEvent")
                                     .withExcludedSessions({subscriber2Id}),
                                 yield).value();
            while (eventCount1 < 1)
                suspendCoro(yield);
            CHECK( eventCount1 == 1 );
            CHECK( eventCount2 == 0 );

            // Allow subscriber2
            f.publisher.publish(Pub("onEvent")
                                     .withEligibleSessions({subscriber2Id}),
                                 yield).value();
            while (eventCount2 < 1)
                suspendCoro(yield);
            CHECK( eventCount1 == 1 );
            CHECK( eventCount2 == 1 );

            f.disconnect();
            subscriber2.disconnect();
        });
        ioctx.run();
    }
}}

//------------------------------------------------------------------------------
SCENARIO( "WAMP ticket authentication", "[WAMP][Advanced]" )
{
GIVEN( "a Session with a registered challenge handler" )
{
    IoContext ioctx;
    TicketAuthFixture f(ioctx, authTcp);

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
            REQUIRE( f.info.has_value() );
            CHECK( f.info->optionByKey("authmethod") == "ticket" );
            CHECK( f.info->optionByKey("authrole") == "ticketrole" );
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
            CHECK_FALSE( f.info.has_value() );
            CHECK( f.abortReason.errorCode() == WampErrc::authorizationDenied );
        }
    }
}}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
