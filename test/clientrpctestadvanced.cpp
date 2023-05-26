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
const auto withTcp = TcpHost("localhost", testPort).withFormat(json);

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
        welcome = caller.join(Petition(testRealm), yield).value();
        callerId = welcome.sessionId();
        callee.connect(where, yield).value();
        callee.join(Petition(testRealm), yield).value();
    }

    void disconnect()
    {
        caller.disconnect();
        callee.disconnect();
    }

    ConnectionWish where;

    Session caller;
    Session callee;

    Welcome welcome;
    SessionId callerId = -1;
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
            REQUIRE(f.welcome.features().dealer().all_of(
                DealerFeatures::callerIdentification));

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
            if (!f.welcome.features().dealer().all_of(
                    DealerFeatures::patternBasedRegistration))
            {
                f.disconnect();
                return;
            }

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
            REQUIRE(f.welcome.features().dealer().all_of(
                DealerFeatures::callCanceling));

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
            REQUIRE(f.welcome.features().dealer().all_of(
                DealerFeatures::callCanceling));

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

    WHEN( "cancelling via a handler-bound slot when calling with a "
          "stackful coroutine completion token")
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            boost::asio::cancellation_signal cancelSignal;
            RequestId invocationRequestId = 0;
            RequestId interruptionRequestId = 0;
            ErrorOr<Result> response;

            f.join(yield);
            REQUIRE(f.welcome.features().dealer().all_of(
                DealerFeatures::callCanceling));

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

    WHEN( "cancelling an RPC in kill mode with no interruption handler" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            CallCancellationSignal sig;
            RequestId invocationRequestId = 0;
            bool responseReceived = false;
            ErrorOr<Result> response;

            f.join(yield);
            REQUIRE(f.welcome.features().dealer().all_of(
                DealerFeatures::callCanceling));

            f.callee.enroll(
                Procedure("rpc"),
                [&invocationRequestId](Invocation inv) -> Outcome
                {
                    invocationRequestId = inv.requestId();
                    return deferment;
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

            sig.emit(CallCancelMode::kill);

            while (!responseReceived)
                suspendCoro(yield);

            CHECK( response == makeUnexpected(WampErrc::cancelled) );

            f.disconnect();
        });
        ioctx.run();
    }

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
            REQUIRE(f.welcome.features().dealer().all_of(
                DealerFeatures::callCanceling));

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
            REQUIRE(f.welcome.features().dealer().test(
                DealerFeatures::callCanceling));

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
            REQUIRE(f.welcome.features().dealer().all_of(
                DealerFeatures::callCanceling));

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
SCENARIO( "Call timeouts", "[WAMP][Advanced]" )
{
GIVEN( "a caller and a callee" )
{
    IoContext ioctx;
    RpcFixture f(ioctx, withTcp);

    auto runTest = [&](bool callerInitiated)
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            std::vector<ErrorOr<Result>> results;
            std::vector<RequestId> interruptions;
            std::map<RequestId, int> valuesByRequestId;

            f.join(yield);
            if (callerInitiated)
            {
                REQUIRE(f.welcome.features().dealer().all_of(
                    DealerFeatures::callCanceling));
            }
            else if (!f.welcome.features().dealer().all_of(
                        DealerFeatures::callTimeout))
            {
                f.disconnect();
                return;
            }

            f.callee.enroll(
                Procedure("com.myapp.foo"),
                [&](Invocation inv) -> Outcome
                {
                    spawn(ioctx, [&, inv](YieldContext yield) mutable
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
                auto rpc1 = Rpc("com.myapp.foo").withArgs(1);
                auto rpc2 = Rpc("com.myapp.foo").withArgs(2);

                if (callerInitiated)
                {
                    rpc1.withCallerTimeout(std::chrono::milliseconds(100));
                    rpc2.withCallerTimeout(std::chrono::milliseconds(50));
                }
                else
                {
                    rpc1.withDealerTimeout(std::chrono::milliseconds(100));
                    rpc2.withDealerTimeout(std::chrono::milliseconds(50));
                }

                f.caller.call(rpc1, callHandler);
                f.caller.call(rpc2, callHandler);
                f.caller.call(Rpc("com.myapp.foo").withArgs(3), callHandler);

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
    };

    WHEN( "the caller initiates timeouts" )
    {
        runTest(true);
    }

    WHEN( "the dealer initiates timeouts" )
    {
        runTest(false);
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
    bool throwErrorArmed = false;
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
            channel.fail(WampErrc::invalidArgument);
            CHECK(channel.state() == ChannelState::closed);
            return;
        }
        else if (throwErrorArmed)
        {
            throw Error{WampErrc::invalidArgument};
        }

        auto rsvp = CalleeOutputChunk().withArgs("rsvp");
        bool sent = channel.respond(rsvp).value();
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
                        CHECK(chan.state() == ChannelState::closed);
                    }
                    else if (isFinal && leaveEarlyArmed)
                    {
                        f.callee.leave(yield).value();
                        CHECK(chan.state() == ChannelState::abandoned);
                    }
                    else if (isFinal && destroyEarlyArmed)
                    {
                        chan.detach();
                        CHECK(chan.state() == ChannelState::detached);
                    }
                    else
                    {
                        chan.send(CalleeOutputChunk(isFinal)
                                       .withArgs(input.at(i))).value();
                        auto expectedState = isFinal ? ChannelState::closed
                                                     : ChannelState::open;
                        CHECK(chan.state() == expectedState);
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

        CHECK(channel.state() == ChannelState::open);
    };

    auto runTest = [&]()
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            f.join(yield);
            REQUIRE(f.welcome.features().dealer().all_of(
                DealerFeatures::progressiveCallResults));
            f.callee.enroll(Stream("com.myapp.foo").withInvitationExpected(),
                            onStream, yield).value();

            for (unsigned i=0; i<2; ++i)
            {
                Error error;
                StreamRequest req{"com.myapp.foo", StreamMode::calleeToCaller};
                req.withArgs("invitation").captureError(error);
                auto channelOrError = f.caller.requestStream(req, onChunk,
                                                             yield);

                if (rejectArmed || throwErrorArmed)
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

                if (leaveEarlyArmed)
                {
                    f.callee.join(Petition(testRealm), yield).value();
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

    WHEN( "rejecting an invitation with an exception" )
    {
        throwErrorArmed = true;
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
    unsigned calleeChunkCount = 0;

    auto onCalleeChunk = [&](CalleeChannel channel,
                             ErrorOr<CalleeInputChunk> chunk)
    {
        REQUIRE(chunk.has_value());
        CHECK( channel.mode() == StreamMode::calleeToCaller );
        CHECK_FALSE( channel.invitationExpected() );
        auto s = chunk->args().at(0).as<String>();
        CHECK(s == "hello");
        ++calleeChunkCount;
    };

    auto onStream = [&](CalleeChannel channel)
    {
        CHECK( channel.mode() == StreamMode::calleeToCaller );
        CHECK_FALSE( channel.invitationExpected() );
        CHECK_FALSE( channel.invitation().hasArgs() );
        channel.accept(onCalleeChunk).value();

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

    auto onCallerChunk = [&](CallerChannel channel,
                             ErrorOr<CallerInputChunk> chunk)
    {
        INFO("for output.size()=" << output.size());
        CHECK( channel.mode() == StreamMode::calleeToCaller );
        CHECK_FALSE( channel.hasRsvp() );
        REQUIRE( chunk.has_value() );

        bool isFinal = output.size() == input.size() - 1;
        auto n = chunk->args().at(0).to<int>();
        output.push_back(n);
        CHECK( chunk->isFinal() == isFinal );
    };

    WHEN( "streaming result chunks" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            f.join(yield);
            REQUIRE(f.welcome.features().dealer().all_of(
                DealerFeatures::progressiveCallResults));
            f.callee.enroll(Stream("com.myapp.foo"), onStream, yield).value();

            for (unsigned i=0; i<2; ++i)
            {
                StreamRequest req{"com.myapp.foo", StreamMode::calleeToCaller};
                req.withArgs("hello");
                auto channelOrError = f.caller.openStream(req, onCallerChunk,
                                                          yield);
                REQUIRE(channelOrError.has_value());
                auto channel = channelOrError.value();
                CHECK(channel.mode() == StreamMode::calleeToCaller);
                CHECK_FALSE(channel.hasRsvp());

                while (output.size() < input.size())
                    suspendCoro(yield);
                CHECK( input == output );
                CHECK( calleeChunkCount == 1 );
                output.clear();
                calleeChunkCount = 0;
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
    bool cancelArmed = false;
    bool dropChannelArmed = false;
    bool callerThrowArmed = false;
    bool calleeThrowArmed = false;

    auto onInterrupt = [&](CalleeChannel channel, Interruption intr)
    {
        interruptReceived = true;
        CHECK(intr.cancelMode() == CallCancelMode::killNoWait);
        if (calleeThrowArmed)
        {
            timer.cancel();
            throw Error{WampErrc::invalidArgument};
        }
        channel.fail(WampErrc::cancelled);
        timer.cancel();
    };

    auto onStream = [&](CalleeChannel channel)
    {
        CHECK( channel.mode() == StreamMode::calleeToCaller );
        channel.respond(CalleeOutputChunk().withArgs("rsvp"),
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
                if (!callerThrowArmed)
                    CHECK( interruptReceived );
                output.push_back(input.back());
            });
    };

    auto onChunk = [&](CallerChannel channel, ErrorOr<CallerInputChunk> chunk)
    {
        INFO("for output.size()=" << output.size());
        bool isFinal = output.size() == input.size() - 1;
        if (isFinal && callerThrowArmed)
            throw Reason{WampErrc::invalidArgument};
        if (isFinal)
        {
            REQUIRE_FALSE(chunk.has_value());
            CHECK(chunk.error() == WampErrc::cancelled);
        }
        else
        {
            REQUIRE(chunk.has_value());
            auto n = chunk->args().at(0).to<int>();
            output.push_back(n);
        }
    };

    auto runTest = [&]()
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            f.join(yield);
            REQUIRE(f.welcome.features().dealer().all_of(
                DealerFeatures::progressiveCallResults |
                DealerFeatures::callCanceling));
            f.callee.enroll(Stream("com.myapp.foo").withInvitationExpected(),
                            onStream, yield).value();

            for (unsigned i=0; i<2; ++i)
            {
                StreamRequest req{"com.myapp.foo", StreamMode::calleeToCaller};
                req.withArgs("invitation");
                auto channelOrError = f.caller.requestStream(req, onChunk,
                                                             yield);
                REQUIRE(channelOrError.has_value());
                auto channel = std::move(channelOrError).value();

                while (output.size() < input.size() - 1)
                    suspendCoro(yield);
                REQUIRE_FALSE(interruptReceived);

                if (cancelArmed)
                    channel.cancel(CallCancelMode::killNoWait);
                else if (dropChannelArmed)
                    channel.detach();

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
        cancelArmed = true;
        runTest();
    }

    WHEN( "Cancelling by dropping the channel" )
    {
        dropChannelArmed = true;
        runTest();
    }

    WHEN( "Cancelling by throwing within the chunk handler" )
    {
        callerThrowArmed = true;
        runTest();
    }

    WHEN( "Throwing within the interrupt handler" )
    {
        cancelArmed = true;
        calleeThrowArmed = true;
        runTest();
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
    bool errorReceived = false;

    auto onInterrupt = [&](CalleeChannel, Interruption intr)
    {
        CHECK(intr.cancelMode() == CallCancelMode::killNoWait);
        interruptReceived = true;
        timer.cancel();
    };

    auto onStream = [&](CalleeChannel channel)
    {
        CHECK( channel.mode() == StreamMode::calleeToCaller );
        channel.respond(CalleeOutputChunk().withArgs("rsvp"), nullptr,
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
            REQUIRE(chunk.has_value());
            auto n = chunk->args().at(0).to<int>();
            output.push_back(n);
        }
        else if (chunk.has_value())
        {
            f.caller.leave([](ErrorOr<Reason>) {});
        }
        else
        {
            CHECK(chunk.error() == MiscErrc::abandoned);
            CHECK(channel.error().errorCode() == WampErrc::unknown);
            errorReceived = true;
        }
    };

    WHEN( "streaming result chunks" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            f.join(yield);
            REQUIRE(f.welcome.features().dealer().all_of(
                DealerFeatures::progressiveCallResults));
            f.callee.enroll(Stream("com.myapp.foo").withInvitationExpected(),
                            onStream, yield).value();

            for (unsigned i=0; i<2; ++i)
            {
                StreamRequest req{"com.myapp.foo", StreamMode::calleeToCaller};
                req.withArgs("invitation");
                auto channelOrError = f.caller.requestStream(req, onChunk,
                                                             yield);
                REQUIRE(channelOrError.has_value());
                auto channel = channelOrError.value();

                while ((output.size() < input.size()) || !errorReceived)
                    suspendCoro(yield);
                CHECK( input == output );
                CHECK( interruptReceived );
                CHECK( errorReceived );

                output.clear();
                interruptReceived = false;
                errorReceived = false;

                f.caller.join(Petition(testRealm), yield).value();
            }

            f.disconnect();
        });
        ioctx.run();
    }
}}

//------------------------------------------------------------------------------
SCENARIO( "WAMP callee-to-caller streaming timeouts", "[WAMP][Advanced]" )
{
GIVEN( "a caller and a callee" )
{
    IoContext ioctx;
    RpcFixture f(ioctx, withTcp);
    boost::asio::steady_timer timer(ioctx);

    std::vector<int> input{9, 3, 7, 5};
    std::vector<int> output;
    bool interruptReceived = false;
    bool errorReceived = false;

    auto onInterrupt = [&](CalleeChannel, Interruption intr)
    {
        CHECK(intr.cancelMode() == CallCancelMode::killNoWait);
        interruptReceived = true;
        timer.cancel();
    };

    auto onStream = [&](CalleeChannel channel)
    {
        CHECK( channel.mode() == StreamMode::calleeToCaller );
        channel.respond(CalleeOutputChunk().withArgs("rsvp"), nullptr,
                        onInterrupt).value();

        spawn(
            ioctx,
            [&, channel](YieldContext yield) mutable
            {
                // Never send the last chunk
                for (unsigned i=0; i<input.size()-1; ++i)
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
            REQUIRE(chunk.has_value());
            auto n = chunk->args().at(0).to<int>();
            output.push_back(n);
        }
        else
        {
            REQUIRE_FALSE(chunk.has_value());
            CHECK(chunk.error() == WampErrc::cancelled);
            CHECK(channel.error().errorCode() == WampErrc::timeout);
            errorReceived = true;
        }
    };

    WHEN( "streaming result chunks" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            f.join(yield);
            REQUIRE(f.welcome.features().dealer().all_of(
                DealerFeatures::progressiveCallResults |
                DealerFeatures::callCanceling));
            f.callee.enroll(Stream("com.myapp.foo").withInvitationExpected(),
                            onStream, yield).value();

            for (unsigned i=0; i<2; ++i)
            {
                StreamRequest req{"com.myapp.foo", StreamMode::calleeToCaller};
                req.withArgs("invitation")
                   .withCallerTimeout(std::chrono::milliseconds(75));
                auto channelOrError = f.caller.requestStream(req, onChunk,
                                                             yield);
                REQUIRE(channelOrError.has_value());
                auto channel = channelOrError.value();

                while ((output.size() < input.size()) || !errorReceived)
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
SCENARIO( "WAMP caller-to-callee streaming with invitations",
          "[WAMP][Advanced]" )
{
GIVEN( "a caller and a callee" )
{
    IoContext ioctx;
    RpcFixture f(ioctx, withTcp);

    std::vector<int> input{9, 3, 7, 5};
    std::vector<int> output;
    CalleeChannel calleeChannel;
    bool callerFinalChunkReceived = false;
    bool calleeLeaveArmed = false;
    bool destroyEarlyArmed = false;
    bool calleeThrowArmed = false;

    auto onChunkReceivedByCallee =
        [&](CalleeChannel channel, ErrorOr<CalleeInputChunk> chunk)
        {
            if (!chunk.has_value())
            {
                CHECK(chunk.error() == MiscErrc::abandoned);
                CHECK(output.size() == input.size());
                if (calleeLeaveArmed)
                    CHECK(channel.state() == ChannelState::abandoned);
                calleeChannel.detach();
                return;
            }

            output.push_back(chunk->args().at(0).to<int>());
            if (output.size() == input.size())
            {
                if (calleeLeaveArmed)
                {
                    f.callee.leave(detached);
                }
                else if (destroyEarlyArmed)
                {
                    calleeChannel.detach();
                }
                else if (calleeThrowArmed)
                {
                    throw error::BadType("bad");
                }
                else
                {
                    CHECK( chunk->isFinal() );
                    auto sent = calleeChannel.send(
                        CalleeOutputChunk(true).withArgs(output.size()));
                    CHECK(sent);
                    CHECK(channel.state() == ChannelState::closed);
                }
            }
        };

    auto onStream = [&](CalleeChannel channel)
    {
        CHECK( channel.mode() == StreamMode::callerToCallee );
        CHECK( channel.invitationExpected() );
        CHECK( channel.invitation().args().front().as<String>() ==
              "invitation" );

        bool done = channel.accept(onChunkReceivedByCallee).value();
        CHECK(done);
        calleeChannel = std::move(channel);
    };

    auto onChunkReceivedByCaller =
        [&](CallerChannel channel, ErrorOr<CallerInputChunk> chunk)
        {
            if (calleeLeaveArmed || destroyEarlyArmed)
            {
                REQUIRE_FALSE(chunk.has_value());
                CHECK(channel.state() == ChannelState::closed);
                CHECK(chunk.error() == WampErrc::cancelled);
            }
            else if (calleeThrowArmed)
            {
                REQUIRE_FALSE(chunk.has_value());
                CHECK(channel.state() == ChannelState::closed);
                CHECK(chunk.error() == WampErrc::invalidArgument);
            }
            else
            {
                REQUIRE(chunk.has_value());
                CHECK(chunk->isFinal());
                CHECK(chunk->args().at(0).to<unsigned>() == input.size());
                CHECK(output.size() == input.size());
                auto expectedState = chunk->isFinal() ? ChannelState::closed
                                                      : ChannelState::open;
                CHECK(channel.state() == expectedState);
            }
            callerFinalChunkReceived = true;
        };

    auto runTest = [&]()
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            f.join(yield);
            if (!f.welcome.features().dealer().all_of(
                DealerFeatures::progressiveCallInvocations))
            {
                f.disconnect();
                return;
            }

            f.callee.enroll(Stream("com.myapp.foo").withInvitationExpected(),
                            onStream, yield).value();

            for (unsigned i=0; i<2; ++i)
            {
                StreamRequest req{"com.myapp.foo", StreamMode::callerToCallee};
                req.withArgs("invitation");
                auto channelOrError =
                    f.caller.openStream(req, onChunkReceivedByCaller, yield);
                REQUIRE(channelOrError.has_value());
                auto channel = channelOrError.value();
                CHECK(channel.mode() == StreamMode::callerToCallee);
                CHECK_FALSE(channel.hasRsvp());
                CHECK(channel.rsvp().args().empty());

                boost::asio::steady_timer timer(ioctx);
                for (unsigned i=0; i<input.size(); ++i)
                {
                    // Simulate a streaming app that throttles
                    // the intermediary results at a fixed rate.
                    timer.expires_from_now(std::chrono::milliseconds(25));
                    timer.async_wait(yield);

                    bool isFinal = (i == input.size() - 1);
                    channel.send(CallerOutputChunk(isFinal)
                                  .withArgs(input.at(i))).value();
                }

                while (!callerFinalChunkReceived)
                    suspendCoro(yield);
                CHECK( input == output );
                output.clear();
                callerFinalChunkReceived = false;

                if (calleeLeaveArmed)
                {
                    while (f.callee.state() != SessionState::closed)
                        suspendCoro(yield);
                    f.callee.join(Petition(testRealm), yield).value();
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

    WHEN( "streaming call chunks" )
    {
        runTest();
    }

    WHEN( "callee leaves before receiving final chunk" )
    {
        calleeLeaveArmed = true;
        runTest();
    }

    WHEN( "callee destroys channel before receiving final chunk" )
    {
        destroyEarlyArmed = true;
        runTest();
    }

    WHEN( "callee throws before receiving final chunk" )
    {
        calleeThrowArmed = true;
        runTest();
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "WAMP caller-to-callee streaming with no negotiation",
          "[WAMP][Advanced]" )
{
GIVEN( "a caller and a callee" )
{
    IoContext ioctx;
    RpcFixture f(ioctx, withTcp);

    std::vector<int> input{9, 3, 7, 5};
    std::vector<int> output;
    CalleeChannel calleeChannel;
    bool callerFinalChunkReceived = false;

    auto onChunkReceivedByCallee =
        [&](CalleeChannel channel, ErrorOr<CalleeInputChunk> chunk)
        {
            REQUIRE(chunk.has_value());
            output.push_back(chunk->args().at(0).to<int>());
            if (output.size() == input.size())
            {
                CHECK( chunk->isFinal() );
                auto sent = calleeChannel.send(
                    CalleeOutputChunk(true).withArgs(output.size()));
                CHECK(sent);
                CHECK(channel.state() == ChannelState::closed);
            }
        };

    auto onStream = [&](CalleeChannel channel)
    {
        CHECK( channel.mode() == StreamMode::callerToCallee );
        CHECK_FALSE( channel.invitationExpected() );
        CHECK( channel.invitation().args().empty() );

        bool done = channel.accept(onChunkReceivedByCallee).value();
        CHECK(done);
        calleeChannel = std::move(channel);
    };

    auto onChunkReceivedByCaller =
        [&](CallerChannel channel, ErrorOr<CallerInputChunk> chunk)
        {
            REQUIRE(chunk.has_value());
            CHECK(chunk->isFinal());
            CHECK(chunk->args().at(0).to<unsigned>() == input.size());
            CHECK(output.size() == input.size());
            callerFinalChunkReceived = true;
        };

    WHEN( "streaming call chunks" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            f.join(yield);
            if (!f.welcome.features().dealer().all_of(
                DealerFeatures::progressiveCallInvocations))
            {
                f.disconnect();
                return;
            }

            f.callee.enroll(
                Stream("com.myapp.foo").withInvitationExpected(false),
                onStream, yield).value();

            for (unsigned i=0; i<2; ++i)
            {
                StreamRequest req{"com.myapp.foo", StreamMode::callerToCallee};
                req.withArgs(input.front());
                auto channelOrError =
                    f.caller.openStream(req, onChunkReceivedByCaller, yield);
                REQUIRE(channelOrError.has_value());
                auto channel = channelOrError.value();
                CHECK(channel.mode() == StreamMode::callerToCallee);
                CHECK_FALSE(channel.hasRsvp());
                CHECK(channel.rsvp().args().empty());

                boost::asio::steady_timer timer(ioctx);
                for (unsigned i=1; i<input.size(); ++i)
                {
                    // Simulate a streaming app that throttles
                    // the intermediary results at a fixed rate.
                    timer.expires_from_now(std::chrono::milliseconds(25));
                    timer.async_wait(yield);

                    bool isFinal = (i == input.size() - 1);
                    channel.send(CallerOutputChunk(isFinal)
                                  .withArgs(input.at(i))).value();
                }

                while (!callerFinalChunkReceived)
                    suspendCoro(yield);
                CHECK( input == output );
                output.clear();
                callerFinalChunkReceived = false;
            }

            f.disconnect();
        });
        ioctx.run();
    };
}}


//------------------------------------------------------------------------------
SCENARIO( "WAMP caller-to-callee streaming cancellation", "[WAMP][Advanced]" )
{
GIVEN( "a caller and a callee" )
{
    IoContext ioctx;
    RpcFixture f(ioctx, withTcp);
    boost::asio::steady_timer timer(ioctx);

    std::vector<int> input{9, 3, 7, 5};
    std::vector<int> output;
    CalleeChannel calleeChannel;
    bool interruptReceived = false;
    bool callerErrorReceived = false;
    bool cancelArmed = false;
    bool dropChannelArmed = false;
    bool callerLeaveArmed = false;
    bool calleeThrowArmed = false;

    auto onChunkReceivedByCallee =
        [&](CalleeChannel channel, ErrorOr<CalleeInputChunk> chunk)
    {
        if (chunk.has_value())
        {
            output.push_back(chunk->args().at(0).to<int>());
        }
        else
        {
            CHECK(chunk.error() == WampErrc::cancelled);
            output.push_back(input.back());
            calleeChannel.detach();
            return;
        }
    };

    auto onInterrupt = [&](CalleeChannel channel, Interruption intr)
    {
        interruptReceived = true;
        if (dropChannelArmed || callerLeaveArmed)
            CHECK(intr.cancelMode() == CallCancelMode::killNoWait);
        else
            CHECK(intr.cancelMode() == CallCancelMode::kill);
        calleeChannel.detach();
        output.push_back(input.back());
        if (calleeThrowArmed)
            throw Error{WampErrc::invalidArgument};
        channel.fail(WampErrc::cancelled);
    };

    auto onStream = [&](CalleeChannel channel)
    {
        CHECK( channel.mode() == StreamMode::callerToCallee );
        CHECK( channel.invitationExpected() );
        CHECK( channel.invitation().args().front().as<String>() ==
              "invitation" );

        bool done = channel.accept(onChunkReceivedByCallee, onInterrupt).value();
        CHECK(done);
        calleeChannel = std::move(channel);
    };

    auto onChunkReceivedByCaller =
        [&](CallerChannel channel, ErrorOr<CallerInputChunk> chunk)
        {
            REQUIRE_FALSE(chunk.has_value());
            if (calleeThrowArmed)
                CHECK(chunk.error() == WampErrc::invalidArgument);
            else if (callerLeaveArmed)
                CHECK(chunk.error() == MiscErrc::abandoned);
            else
                CHECK(chunk.error() == WampErrc::cancelled);
            callerErrorReceived = true;
        };

    auto runTest = [&]()
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            f.join(yield);
            if (!f.welcome.features().dealer().all_of(
                DealerFeatures::progressiveCallInvocations))
            {
                f.disconnect();
                return;
            }

            f.callee.enroll(Stream("com.myapp.foo").withInvitationExpected(),
                            onStream, yield).value();

            for (unsigned i=0; i<2; ++i)
            {
                StreamRequest req{"com.myapp.foo", StreamMode::callerToCallee};
                req.withArgs("invitation");
                auto channelOrError =
                    f.caller.openStream(req, onChunkReceivedByCaller, yield);
                REQUIRE(channelOrError.has_value());
                auto channel = std::move(channelOrError).value();
                CHECK(channel.mode() == StreamMode::callerToCallee);
                CHECK_FALSE(channel.hasRsvp());
                CHECK(channel.rsvp().args().empty());

                boost::asio::steady_timer timer(ioctx);
                for (unsigned i=0; i<input.size()-1; ++i)
                {
                    // Simulate a streaming app that throttles
                    // the intermediary results at a fixed rate.
                    timer.expires_from_now(std::chrono::milliseconds(25));
                    timer.async_wait(yield);

                    bool isFinal = (i == input.size() - 1);
                    channel.send(CallerOutputChunk(isFinal)
                                  .withArgs(input.at(i))).value();
                }

                if (cancelArmed)
                    channel.cancel(CallCancelMode::kill);
                else if (dropChannelArmed)
                    channel.detach();
                else if (callerLeaveArmed)
                    f.caller.leave(yield).value();

                while ((output.size() != input.size()) ||
                       (!dropChannelArmed && !callerErrorReceived))
                {
                    suspendCoro(yield);
                }
                CHECK( input == output );
                CHECK( interruptReceived );
                output.clear();
                interruptReceived = false;
                callerErrorReceived = false;

                if (callerLeaveArmed)
                    f.caller.join(Petition(testRealm), yield).value();
            }
            f.disconnect();
        });
        ioctx.run();
    };

    WHEN( "Cancelling via explicit cancel" )
    {
        cancelArmed = true;
        runTest();
    }

    WHEN( "Cancelling by dropping the channel" )
    {
        dropChannelArmed = true;
        runTest();
    }

    WHEN( "Cancelling by caller leaving" )
    {
        callerLeaveArmed = true;
        runTest();
    }

    WHEN( "Throwing within the interrupt handler" )
    {
        cancelArmed = true;
        calleeThrowArmed = true;
        runTest();
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "WAMP bidirectional streaming", "[WAMP][Advanced]" )
{
GIVEN( "a caller and a callee" )
{
    IoContext ioctx;
    RpcFixture f(ioctx, withTcp);

    std::vector<int> input{9, 3, 7, 5};
    std::vector<int> output;
    CalleeChannel calleeChannel;

    auto onChunkReceivedByCallee =
        [&](CalleeChannel channel, ErrorOr<CalleeInputChunk> chunk)
    {
        // Echo the paylaod back in the other direction
        REQUIRE(chunk.has_value());
        auto n = chunk->args().at(0).to<int>();
        bool isFinal = chunk->isFinal();
        channel.send(CalleeOutputChunk{isFinal}.withArgs(n)).value();
        auto expectedState = isFinal ? ChannelState::closed
                                     : ChannelState::open;
        CHECK(channel.state() == expectedState);
    };

    auto onStream = [&](CalleeChannel channel)
    {
        CHECK( channel.mode() == StreamMode::bidirectional );
        CHECK( channel.invitationExpected() );
        CHECK( channel.invitation().args().front().as<String>() ==
              "invitation" );

        bool done = channel.accept(onChunkReceivedByCallee).value();
        CHECK(done);
        calleeChannel = std::move(channel);
    };

    auto onChunkReceivedByCaller =
        [&](CallerChannel channel, ErrorOr<CallerInputChunk> chunk)
    {
        REQUIRE(chunk.has_value());
        output.push_back(chunk->args().at(0).to<int>());
        CHECK(chunk->isFinal() == (output.size() == input.size()));
    };

    WHEN( "streaming" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            f.join(yield);
            if (!f.welcome.features().dealer().all_of(
                    DealerFeatures::progressiveCallInvocations))
            {
                f.disconnect();
                return;
            }
            f.callee.enroll(Stream("com.myapp.foo").withInvitationExpected(),
                            onStream, yield).value();
            for (unsigned i=0; i<2; ++i)
            {
                StreamRequest req{"com.myapp.foo", StreamMode::bidirectional};
                req.withArgs("invitation");
                auto channelOrError =
                    f.caller.openStream(req, onChunkReceivedByCaller, yield);
                REQUIRE(channelOrError.has_value());
                auto channel = channelOrError.value();
                CHECK(channel.mode() == StreamMode::bidirectional);
                CHECK_FALSE(channel.hasRsvp());
                CHECK(channel.rsvp().args().empty());
                boost::asio::steady_timer timer(ioctx);
                for (unsigned i=0; i<input.size(); ++i)
                {
                    // Simulate a streaming app that throttles
                    // the intermediary results at a fixed rate.
                    timer.expires_from_now(std::chrono::milliseconds(25));
                    timer.async_wait(yield);
                   bool isFinal = (i == input.size() - 1);
                    channel.send(CallerOutputChunk(isFinal)
                                     .withArgs(input.at(i))).value();
                }
                while (output.size() < input.size())
                    suspendCoro(yield);
                CHECK( input == output );
                output.clear();
            }
            f.disconnect();
        });
        ioctx.run();
    };
}}
#endif // defined(CPPWAMP_TEST_HAS_CORO)
