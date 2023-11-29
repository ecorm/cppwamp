/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#if defined(CPPWAMP_TEST_HAS_CORO)

#include "clienttesting.hpp"

using namespace test;
using namespace Catch::Matchers;

namespace
{

//------------------------------------------------------------------------------
void checkInvalidConnect(Session& session, YieldContext yield)
{
    INFO("connect");
    auto index = session.connect(withTcp, yield);
    REQUIRE_FALSE( index.has_value() );
    CHECK( index.error() == MiscErrc::invalidState );
    CHECK_THROWS_AS( index.value(), error::Failure );
}

//------------------------------------------------------------------------------
void checkInvalidJoin(Session& session, YieldContext yield)
{
    INFO("join");
    auto welcome = session.join(testRealm, yield);
    REQUIRE_FALSE( welcome.has_value() );
    CHECK( welcome.error() == MiscErrc::invalidState );
    CHECK_THROWS_AS( session.join(testRealm, yield).value(),
                     error::Failure );
}

//------------------------------------------------------------------------------
void checkInvalidLeave(Session& session, YieldContext yield)
{
    INFO("leave");
    auto reason = session.leave(yield);
    REQUIRE_FALSE( reason.has_value() );
    CHECK( reason.error() == MiscErrc::invalidState );
    CHECK_THROWS_AS( reason.value(), error::Failure );
}

//------------------------------------------------------------------------------
void checkInvalidPublish(Session& session, YieldContext yield)
{
    INFO("publish");
    CHECK_NOTHROW( session.publish(Pub("topic")) );
    CHECK_NOTHROW( session.publish(Pub("topic").withArgs(42)) );
    auto pub = session.publish(Pub("topic"), yield);
    REQUIRE_FALSE( pub );
    CHECK( pub.error() == MiscErrc::invalidState );
    CHECK_THROWS_AS( pub.value(), error::Failure );
}

//------------------------------------------------------------------------------
void checkInvalidSubscribe(Session& session, YieldContext yield)
{
    INFO("subscribe");
    auto sub = session.subscribe("topic", [](Event){}, yield);
    REQUIRE_FALSE( sub );
    CHECK( sub.error() == MiscErrc::invalidState );
    CHECK_THROWS_AS( sub.value(), error::Failure );
}

//------------------------------------------------------------------------------
void checkInvalidEnroll(Session& session, YieldContext yield)
{
    INFO("enroll");
    auto reg = session.enroll("rpc", [](Invocation)->Outcome {return {};},
                              yield);
    REQUIRE_FALSE( reg );
    CHECK( reg.error() == MiscErrc::invalidState );
    CHECK_THROWS_AS( reg.value(), error::Failure );
}

//------------------------------------------------------------------------------
void checkInvalidCall(Session& session, YieldContext yield)
{
    INFO("call");
    auto result = session.call(Rpc("rpc"), yield);
    REQUIRE_FALSE( result );
    CHECK( result.error() == MiscErrc::invalidState );
    CHECK_THROWS_AS( result.value(), error::Failure );
}

//------------------------------------------------------------------------------
inline void checkInvalidOps(Session& session, YieldContext yield)
{
    checkInvalidLeave(session, yield);
    checkInvalidPublish(session, yield);
    checkInvalidSubscribe(session, yield);
    checkInvalidEnroll(session, yield);
    checkInvalidCall(session, yield);
}

} // anonymous namespace

//------------------------------------------------------------------------------
SCENARIO( "WAMP Invalid State Failures", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a TCP connector" )
{
    IoContext ioctx;
    const auto where = withTcp;

    WHEN( "using invalid operations while disconnected" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            Session session(ioctx);
            REQUIRE( session.state() == SessionState::disconnected );
            checkInvalidJoin(session, yield);
            checkInvalidLeave(session, yield);
            checkInvalidOps(session, yield);
        });

        ioctx.run();
    }

    WHEN( "using invalid operations while connecting" )
    {
        Session session(ioctx);
        session.connect(where, [](ErrorOr<size_t>){} );

        spawn(ioctx, [&](YieldContext yield)
        {
            ioctx.stop();
            ioctx.restart();
            REQUIRE( session.state() == SessionState::connecting );
            checkInvalidConnect(session, yield);
            checkInvalidJoin(session, yield);
            checkInvalidLeave(session, yield);
            checkInvalidOps(session, yield);
            session.disconnect();
        });

        ioctx.run();
    }

    WHEN( "using invalid operations while failed" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            Session session(ioctx);
            CHECK_THROWS( session.connect(invalidTcp, yield).value() );
            REQUIRE( session.state() == SessionState::failed );
            checkInvalidJoin(session, yield);
            checkInvalidLeave(session, yield);
            checkInvalidOps(session, yield);
        });

        ioctx.run();
    }

    WHEN( "using invalid operations while closed" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            Session session(ioctx);
            session.connect(where, yield).value();
            REQUIRE( session.state() == SessionState::closed );
            checkInvalidConnect(session, yield);
            checkInvalidLeave(session, yield);
            checkInvalidOps(session, yield);
            session.disconnect();
        });

        ioctx.run();
    }

    WHEN( "using invalid operations while establishing" )
    {
        Session session(ioctx);

        auto reestablish = [&](YieldContext yield)
        {
            session.disconnect();
            session.connect(where, yield).value();
            session.join(testRealm, [](ErrorOr<Welcome>){});
            while (session.state() != SessionState::establishing)
                suspendCoro(yield);
        };

        spawn(ioctx, [&](YieldContext yield)
        {
            reestablish(yield);
            checkInvalidConnect(session, yield);

            reestablish(yield);
            checkInvalidJoin(session, yield);

            reestablish(yield);
            checkInvalidLeave(session, yield);

            reestablish(yield);
            checkInvalidPublish(session, yield);

            reestablish(yield);
            checkInvalidSubscribe(session, yield);

            reestablish(yield);
            checkInvalidEnroll(session, yield);

            reestablish(yield);
            checkInvalidCall(session, yield);

            session.disconnect();
        });
        ioctx.run();
    }

    WHEN( "using invalid operations while established" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            Session session(ioctx);
            session.connect(where, yield).value();
            session.join(testRealm, yield).value();
            REQUIRE( session.state() == SessionState::established );
            checkInvalidConnect(session, yield);
            checkInvalidJoin(session, yield);
            session.disconnect();
        });

        ioctx.run();
    }

    WHEN( "using invalid operations while shutting down" )
    {
        Session session(ioctx);
        spawn(ioctx, [&](YieldContext yield)
        {
            session.connect(where, yield).value();
            session.join(testRealm, yield).value();
            session.leave([](ErrorOr<Goodbye>){});
            while (session.state() != SessionState::shuttingDown)
                suspendCoro(yield);
            checkInvalidConnect(session, yield);
            checkInvalidJoin(session, yield);
            checkInvalidLeave(session, yield);
            checkInvalidOps(session, yield);
            session.disconnect();
        });
        ioctx.run();
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "Large WAMP Payloads", "[WAMP][Basic]" )
{
GIVEN( "these test fixture objects" )
{
    IoContext ioctx;
    const auto where = withTcp;
    Session caller(ioctx);
    Session callee(ioctx);

    // RPC that returns the string argument back to the caller.
    std::string echoedString;
    auto echo =
        [&echoedString](Invocation, std::string str) -> Outcome
    {
        echoedString = str;
        return Result({str});
    };

    // RPC that returns a result payload larger than transport limits.
    auto excessive = [](Invocation) -> Outcome
    {
        return Result({std::string(17*1024*1024, ' ')});
    };

    // RPC that returns an error payload larger than transport limits.
    auto excessiveError = [](Invocation) -> Outcome
    {
        return Error("bad").withArgs(std::string(17*1024*1024, ' '));
    };

    WHEN( "an RPC is invoked while a large event payload is being transmitted" )
    {
        // Fill large string with repeating character sequence
        std::string largeString(1024*1024, ' ');
        for (size_t i=0; i<largeString.length(); ++i)
            largeString[i] = '0' + (i % 64);

        std::string eventString;

        auto onEvent = [&eventString](Event, std::string str)
        {
            eventString = std::move(str);
        };

        // RPC that triggers the publishing of a large event payload
        auto trigger =
            [&callee, &largeString] (Invocation) -> Outcome
            {
                callee.publish(Pub("grapevine").withArgs(largeString));
                return Result();
            };

        spawn(ioctx, [&](YieldContext yield)
        {
            caller.connect(where, yield).value();
            caller.join(testRealm, yield).value();
            caller.subscribe("grapevine",
                             unpackedEvent<std::string>(onEvent),
                             yield).value();

            callee.connect(where, yield).value();
            callee.join(testRealm, yield).value();
            callee.enroll("echo", unpackedRpc<std::string>(echo),
                          yield).value();
            callee.enroll("trigger", trigger, yield).value();

            for (int i=0; i<10; ++i)
            {
                // Use async call so that it doesn't block until completion.
                caller.call(Rpc("trigger").withArgs("hello"),
                                 [](ErrorOr<Result>) {});

                /*  Try to get callee to send an RPC response while it's still
                    transmitting the large event payload. RawsockTransport
                    should properly enqueue the RPC response while the large
                    event payload is being transmitted. */
                while (eventString.empty())
                    caller.call(Rpc("echo").withArgs("hello"), yield).value();

                CHECK_THAT( eventString, Equals(largeString) );
                eventString.clear();
            }
            callee.disconnect();
            caller.disconnect();
        });

        ioctx.run();
    }

    WHEN( "sending a payload exceeds the router's transport limit" )
    {
        // Fill large string with repeating character sequence
        std::string largeString(17*1024*1024, ' ');
        for (size_t i=0; i<largeString.length(); ++i)
            largeString[i] = '0' + (i % 64);

        spawn(ioctx, [&](YieldContext yield)
        {
            caller.connect(where, yield).value();
            caller.join(testRealm, yield).value();

            callee.connect(where, yield).value();
            callee.join(testRealm, yield).value();
            callee.enroll("echo", unpackedRpc<std::string>(echo),
                          yield).value();

            auto result = caller.call(Rpc("echo").withArgs(largeString), yield);
            CHECK( result == makeUnexpectedError(WampErrc::payloadSizeExceeded) );
            CHECK( echoedString.empty() );

            callee.disconnect();
            caller.disconnect();
        });

        ioctx.run();
    }

    WHEN( "returning a result exceeding the router's transport limit" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            caller.connect(where, yield).value();
            caller.join(testRealm, yield).value();

            std::vector<Incident> incidents;
            callee.observeIncidents(
                [&incidents](Incident i) {incidents.push_back(i);});
            callee.connect(where, yield).value();
            callee.join(testRealm, yield).value();
            callee.enroll("excessive", excessive, yield).value();

            auto result = caller.call(Rpc("excessive"), yield);
            CHECK( result == makeUnexpectedError(WampErrc::payloadSizeExceeded) );
            REQUIRE(incidents.size() == 1);
            auto incident = incidents.front();
            CHECK(incident.kind() == IncidentKind::trouble);
            CHECK(incident.error() == WampErrc::payloadSizeExceeded );

            callee.disconnect();
            caller.disconnect();
        });

        ioctx.run();
    }

    WHEN( "returning an error exceeding the router's transport limit" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            caller.connect(where, yield).value();
            caller.join(testRealm, yield).value();

            std::vector<Incident> incidents;
            callee.observeIncidents(
                [&incidents](Incident i) {incidents.push_back(i);});
            callee.connect(where, yield).value();
            callee.join(testRealm, yield).value();
            callee.enroll("excessive", excessiveError, yield).value();

            Error error;
            auto result = caller.call(Rpc("excessive").captureError(error),
                                      yield);
            CHECK( result == makeUnexpectedError(WampErrc::unknown) );
            CHECK( error.uri() == "bad" );
            REQUIRE(incidents.size() == 1);
            auto incident = incidents.front();
            CHECK(incident.kind() == IncidentKind::trouble);
            CHECK(incident.error() == WampErrc::payloadSizeExceeded );

            callee.disconnect();
            caller.disconnect();
        });

        ioctx.run();
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Using Thread Pools", "[WAMP][Basic]" )
{
GIVEN( "a thread pool execution context" )
{
    boost::asio::thread_pool pool(4);
    const auto where = withTcp;
    Session session(pool);
    unsigned callParallelism = 0;
    unsigned callWatermark = 0;
    std::vector<int> callNumbers;
    std::vector<int> resultNumbers;
    std::mutex callMutex;
    unsigned eventParallelism = 0;
    unsigned eventWatermark = 0;
    std::atomic<unsigned> eventCount(0);
    std::vector<int> eventNumbers;
    std::mutex eventMutex;
    std::vector<int> numbers;
    for (int i=0; i<20; ++i)
        numbers.push_back(i);

    auto rpc = [&](Invocation inv) -> Deferment
    {
        unsigned c = 0;
        {
            std::lock_guard<std::mutex> lock(callMutex);
            c = ++callParallelism;
            if (c > callWatermark)
                callWatermark = c;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto n = inv.args().at(0).to<int>();

        // Alternate between publish with and without a completion token
        if (n % 2 == 0)
        {
            session.publish(
                Pub("topic").withExcludeMe(false).withArgs(n),
                [](ErrorOr<PublicationId> pubId)
                {
                    pubId.value();
                });
        }
        else
        {
            session.publish(Pub("topic").withExcludeMe(false).withArgs(n));
        }

        {
            std::lock_guard<std::mutex> lock(callMutex);
            callNumbers.push_back(n);
            --callParallelism;
        }

        inv.yield(Result{n});
        return deferment;
    };

    auto onEvent = [&](Event ev)
    {

        unsigned c = 0;
        {
            std::lock_guard<std::mutex> lock(eventMutex);
            c = ++eventParallelism;
            if (c > eventWatermark)
                eventWatermark = c;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto n = ev.args().at(0).to<int>();

        {
            std::lock_guard<std::mutex> lock(eventMutex);
            eventNumbers.push_back(n);
            --eventParallelism;
        }

        ++eventCount;
    };

    spawn(session.strand(), [&](YieldContext yield)
    {
        session.connect(where, yield).value();
        session.join(testRealm, yield).value();
        session.enroll("rpc", rpc, yield).value();
        session.subscribe("topic", onEvent, yield).value();
        for (unsigned i=0; i<numbers.size(); ++i)
        {
            session.call(
                Rpc("rpc").withArgs(numbers[i]),
                [&resultNumbers](ErrorOr<Result> n)
                {
                    resultNumbers.push_back(n.value().args().at(0).to<int>());
                });
        }
        while ((eventCount.load() < numbers.size()) ||
               (resultNumbers.size() < numbers.size()))
        {
            suspendCoro(yield);
        }
        session.leave(yield).value();
        session.disconnect();

        CHECK( callWatermark > 1 );
        CHECK( eventWatermark > 1 );
        CHECK_THAT( callNumbers, Catch::Matchers::UnorderedEquals(numbers) );
        CHECK_THAT( resultNumbers, Catch::Matchers::UnorderedEquals(numbers) );
        CHECK_THAT( eventNumbers, Catch::Matchers::UnorderedEquals(numbers) );
    });

    pool.join();
}
}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
