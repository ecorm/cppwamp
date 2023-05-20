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
struct RpcFixture
{
    RpcFixture(IoContext& ioctx, ConnectionWish wish)
        : ioctx(ioctx),
          where(std::move(wish)),
          caller(ioctx),
          callee(ioctx)
    {}

    void join(YieldContext yield)
    {
        caller.connect(where, yield).value();
        caller.join(Petition(testRealm), yield).value();
        callee.connect(where, yield).value();
        callee.join(Petition(testRealm), yield).value();
    }

    void enroll(YieldContext yield)
    {
        using namespace std::placeholders;
        dynamicReg = callee.enroll(
                Procedure("dynamic"),
                std::bind(&RpcFixture::dynamicRpc, this, _1),
                yield).value();

        staticReg = callee.enroll(
                Procedure("static"),
                unpackedRpc<std::string, int>(std::bind(&RpcFixture::staticRpc,
                                                        this, _1, _2, _3)),
                yield).value();
    }

    Outcome dynamicRpc(Invocation inv)
    {
        INFO( "in RPC handler" );
        CHECK( inv.requestId() <= 9007199254740992ll );
        CHECK( inv.executor() == ioctx.get_executor() );
        ++dynamicCount;
        // Echo back the call arguments as the result.
        return Result().withArgList(inv.args());
    }

    Outcome staticRpc(Invocation inv, std::string str, int num)
    {
        INFO( "in RPC handler" );
        CHECK( inv.requestId() <= 9007199254740992ll );
        CHECK( inv.executor() == ioctx.get_executor() );
        ++staticCount;
        // Echo back the call arguments as the yield result.
        return {str, num};
    }

    IoContext& ioctx;
    ConnectionWish where;

    Session caller;
    Session callee;

    ScopedRegistration dynamicReg;
    ScopedRegistration staticReg;

    int dynamicCount = 0;
    int staticCount = 0;
};

} // anonymous namespace

//------------------------------------------------------------------------------
SCENARIO( "WAMP RPCs", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a ConnectionWish" )
{
    IoContext ioctx;
    const auto where = withTcp;

    WHEN( "calling remote procedures taking dynamically-typed args" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            RpcFixture f(ioctx, where);
            f.join(yield);
            f.enroll(yield);

            // Check normal RPC
            Error error;
            auto result = f.caller.call(Rpc("dynamic").withArgs("one", 1)
                                         .captureError(error), yield);
            REQUIRE_FALSE(!result);
            CHECK( !error );
            CHECK( error.uri().empty() );
            CHECK( f.dynamicCount == 1 );
            CHECK(( result.value().args() == Array{"one", 1} ));
            result = f.caller.call(Rpc("dynamic").withArgs("two", 2),
                                    yield);
            REQUIRE_FALSE(!result);
            CHECK( f.dynamicCount == 2 );
            CHECK(( result.value().args() == Array{"two", 2} ));

            // Manually unregister the slot.
            f.callee.unregister(f.dynamicReg, yield).value();

            // The router should now report an error when attempting
            // to call the unregistered RPC.
            result = f.caller.call(Rpc("dynamic").withArgs("three", 3), yield);
            CHECK( result == makeUnexpected(WampErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure);

            // Calling should work after re-registering the slot.
            using namespace std::placeholders;
            f.dynamicReg = f.callee.enroll(
                Procedure("dynamic"),
                std::bind(&RpcFixture::dynamicRpc, &f, _1),
                yield).value();
            result = f.caller.call(Rpc("dynamic").withArgs("four", 4),
                                    yield);
            REQUIRE_FALSE(!result);
            CHECK( f.dynamicCount == 3 );
            CHECK(( result.value().args() == Array{"four", 4} ));
        });
        ioctx.run();
    }

    WHEN( "calling remote procedures taking statically-typed args" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            RpcFixture f(ioctx, where);
            f.join(yield);
            f.enroll(yield);

            // Check normal RPC
            auto result = f.caller.call(Rpc("static").withArgs("one", 1),
                                         yield);
            REQUIRE_FALSE(!result);
            CHECK( f.staticCount == 1 );
            CHECK(( result.value().args() == Array{"one", 1} ));

            // Extra arguments should be ignored.
            result = f.caller.call(Rpc("static").withArgs("two", 2, true),
                                    yield);
            REQUIRE_FALSE(!result);
            CHECK( f.staticCount == 2 );
            CHECK(( result.value().args() == Array{"two", 2} ));

            // Unregister the slot via RAII.
            f.staticReg = ScopedRegistration();

            // The router should now report an error when attempting
            // to call the unregistered RPC.
            result = f.caller.call(Rpc("static").withArgs("three", 3), yield);
            CHECK( result == makeUnexpected(WampErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure );

            // Calling should work after re-registering the slot.
            using namespace std::placeholders;
            f.staticReg = f.callee.enroll(
                Procedure("static"),
                unpackedRpc<std::string, int>(std::bind(&RpcFixture::staticRpc,
                                                        &f, _1, _2, _3)),
                yield).value();
            result = f.caller.call(Rpc("static").withArgs("four", 4), yield);
            REQUIRE_FALSE(!result);
            CHECK( f.staticCount == 3 );
            CHECK(( result.value().args() == Array{"four", 4} ));
        });
        ioctx.run();
    }

    WHEN( "calling simple remote procedures" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            RpcFixture f(ioctx, where);
            f.join(yield);

            f.staticReg = f.callee.enroll(
                    Procedure("static"),
                    simpleRpc<int, std::string, int>([&](std::string, int n)
                    {
                        ++f.staticCount;
                        return n; // Echo back the integer argument
                    }),
                    yield).value();

            // Check normal RPC
            auto result = f.caller.call(Rpc("static").withArgs("one", 1),
                                         yield);
            REQUIRE_FALSE(!result);
            CHECK( f.staticCount == 1 );
            CHECK(( result.value().args() == Array{1} ));

            // Extra arguments should be ignored.
            result = f.caller.call(Rpc("static").withArgs("two", 2, true),
                                    yield);
            REQUIRE_FALSE(!result);
            CHECK( f.staticCount == 2 );
            CHECK(( result.value().args() == Array{2} ));

            // Unregister the slot via RAII.
            f.staticReg = ScopedRegistration();

            // The router should now report an error when attempting
            // to call the unregistered RPC.
            result = f.caller.call(Rpc("static").withArgs("three", 3), yield);
            CHECK( result == makeUnexpected(WampErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure );

            // Calling should work after re-registering the slot.
            using namespace std::placeholders;
            f.staticReg = f.callee.enroll(
                    Procedure("static"),
                    simpleRpc<int, std::string, int>([&](std::string, int n)
                    {
                        ++f.staticCount;
                        return n; // Echo back the integer argument
                    }),
                    yield).value();
            result = f.caller.call(Rpc("static").withArgs("four", 4), yield);
            REQUIRE_FALSE(!result);
            CHECK( f.staticCount == 3 );
            CHECK(( result.value().args() == Array{4} ));
        });
        ioctx.run();
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "WAMP Registation Lifetimes", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a ConnectionWish" )
{
    IoContext ioctx;
    const auto where = withTcp;

    WHEN( "unregistering after a session is destroyed" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            RpcFixture f(ioctx, where);
            f.join(yield);
            f.enroll(yield);

            // Move and destroy f.callee.impl_
            {
                Session temp{std::move(f.callee)};
            }

            // Manually unregister a RPC.
            REQUIRE_NOTHROW( f.dynamicReg.unregister() );

            // Unregister an RPC via RAII.
            REQUIRE_NOTHROW( f.staticReg = ScopedRegistration() );

            // The router should report an error when attempting
            // to call the unregistered RPCs.
            auto result = f.caller.call(Rpc("dynamic").withArgs("one", 1),
                                         yield);
            CHECK( result == makeUnexpected(WampErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure );

            result = f.caller.call(Rpc("dynamic").withArgs("two", 2), yield);
            CHECK( result == makeUnexpected(WampErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure );
        });
        ioctx.run();
    }

    WHEN( "unregistering after leaving" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            RpcFixture f(ioctx, where);
            f.join(yield);
            f.enroll(yield);

            // Make the callee leave the session.
            f.callee.leave(yield).value();

            // Manually unregister a RPC.
            auto unregistered = f.callee.unregister(f.dynamicReg, yield);
            REQUIRE( unregistered.has_value() );
            CHECK( unregistered.value() == false );
            REQUIRE_NOTHROW( f.dynamicReg.unregister() );

            // Unregister an RPC via RAII.
            REQUIRE_NOTHROW( f.staticReg = ScopedRegistration() );

            // The router should report an error when attempting
            // to call the unregistered RPCs.
            auto result = f.caller.call(Rpc("dynamic").withArgs("one", 1),
                                         yield);
            CHECK( result == makeUnexpected(WampErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure );

            result = f.caller.call(Rpc("dynamic").withArgs("two", 2), yield);
            CHECK( result == makeUnexpected(WampErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure );
        });
        ioctx.run();
    }

    WHEN( "unregistering after disconnecting" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            RpcFixture f(ioctx, where);
            f.join(yield);
            f.enroll(yield);

            // Make the callee disconnect.
            f.callee.disconnect();

            // Manually unregister a RPC.
            auto unregistered = f.callee.unregister(f.dynamicReg, yield);
            REQUIRE( unregistered.has_value() );
            CHECK( unregistered.value() == false );
            REQUIRE_NOTHROW( f.dynamicReg.unregister() );

            // Unregister an RPC via RAII.
            REQUIRE_NOTHROW( f.staticReg = ScopedRegistration() );

            // The router should report an error when attempting
            // to call the unregistered RPCs.
            auto result = f.caller.call(Rpc("dynamic").withArgs("one", 1),
                                         yield);
            CHECK( result == makeUnexpected(WampErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure );

            result = f.caller.call(Rpc("dynamic").withArgs("two", 2), yield);
            CHECK( result == makeUnexpected(WampErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure );
        });
        ioctx.run();
    }

    WHEN( "unregistering after reset" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            RpcFixture f(ioctx, where);
            f.join(yield);
            f.enroll(yield);

            // Move and destroy f.callee.impl_
            {
                Session temp{std::move(f.callee)};
            }

            // Unregister an RPC via RAII.
            REQUIRE_NOTHROW( f.staticReg = ScopedRegistration() );

            // The router should report an error when attempting
            // to call the unregistered RPCs.
            auto result = f.caller.call(Rpc("dynamic").withArgs("one", 1),
                                         yield);
            CHECK( result == makeUnexpected(WampErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure );

            result = f.caller.call(Rpc("dynamic").withArgs("two", 2), yield);
            CHECK( result == makeUnexpected(WampErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure );
        });
        ioctx.run();
    }
    WHEN( "moving a ScopedRegistration" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            RpcFixture f(ioctx, where);
            f.join(yield);
            f.enroll(yield);

            // Check move construction.
            {
                ScopedRegistration reg(std::move(f.dynamicReg));
                CHECK_FALSE( !reg );
                CHECK( reg.id() >= 0 );
                CHECK( !f.dynamicReg );

                f.caller.call(Rpc("dynamic"), yield).value();
                CHECK( f.dynamicCount == 1 );
            }
            // 'reg' goes out of scope here.
            auto result = f.caller.call(Rpc("dynamic"), yield);
            CHECK( result == makeUnexpected(WampErrc::noSuchProcedure) );
            CHECK( f.dynamicCount == 1 );

            // Check move assignment.
            {
                ScopedRegistration reg;
                reg = std::move(f.staticReg);
                CHECK_FALSE( !reg );
                CHECK( reg.id() >= 0 );
                CHECK( !f.staticReg );

                f.caller.call(Rpc("static").withArgs("", 0), yield).value();
                CHECK( f.staticCount == 1 );
            }
            // 'reg' goes out of scope here.
            result = f.caller.call(Rpc("static").withArgs("", 0), yield);
            CHECK( result == makeUnexpected(WampErrc::noSuchProcedure) );
            CHECK( f.staticCount == 1 );
        });
        ioctx.run();
    }
}
}

//------------------------------------------------------------------------------
SCENARIO( "Nested WAMP RPCs and Events", "[WAMP][Basic]" )
{
GIVEN( "these test fixture objects" )
{
    IoContext ioctx;
    const auto where = withTcp;
    Session session1(ioctx);
    Session session2(ioctx);

    // Regular RPC handler
    auto upperify = [](Invocation, std::string str) -> Outcome
    {
        std::transform(str.begin(), str.end(), str.begin(),
                       ::toupper);
        return {str};
    };

    WHEN( "calling remote procedures within an invocation" )
    {
        auto uppercat = [&session2](std::string str1, std::string str2,
                                    YieldContext yield) -> String
        {
            auto upper1 = session2.call(
                    Rpc("upperify").withArgs(str1), yield).value();
            auto upper2 = session2.call(
                    Rpc("upperify").withArgs(str2), yield).value();
            return upper1[0].to<std::string>() + upper2[0].to<std::string>();
        };

        spawn(ioctx, [&](YieldContext yield)
        {
            session1.connect(where, yield).value();
            session1.join(Petition(testRealm), yield).value();
            session1.enroll(Procedure("upperify"),
                             unpackedRpc<std::string>(upperify), yield).value();


            session2.connect(where, yield).value();
            session2.join(Petition(testRealm), yield).value();
            session2.enroll(
                Procedure("uppercat"),
                simpleCoroRpc<std::string, std::string, std::string>(uppercat),
                yield).value();

            std::string s1 = "hello ";
            std::string s2 = "world";
            auto result = session1.call(Rpc("uppercat").withArgs(s1, s2),
                                         yield).value();
            CHECK( result[0] == "HELLO WORLD" );
            session1.disconnect();
            session2.disconnect();
        });

        ioctx.run();
    }

    WHEN( "calling remote procedures within an event handler" )
    {
        auto& callee = session1;
        auto& subscriber = session2;

        std::string upperized;
        auto onEvent =
            [&upperized, &subscriber](std::string str, YieldContext yield)
            {
                auto result = subscriber.call(Rpc("upperify").withArgs(str),
                                               yield).value();
                upperized = result[0].to<std::string>();
            };

        spawn(ioctx, [&](YieldContext yield)
        {
            callee.connect(where, yield).value();
            callee.join(Petition(testRealm), yield).value();
            callee.enroll(Procedure("upperify"),
                           unpackedRpc<std::string>(upperify), yield).value();

            subscriber.connect(where, yield).value();
            subscriber.join(Petition(testRealm), yield).value();
            subscriber.subscribe(Topic("onEvent"),
                                  simpleCoroEvent<std::string>(onEvent),
                                  yield).value();

            callee.publish(Pub("onEvent").withArgs("Hello"), yield).value();
            while (upperized.empty())
                suspendCoro(yield);
            CHECK_THAT( upperized, Equals("HELLO") );
            callee.disconnect();
            subscriber.disconnect();
        });

        ioctx.run();
    }

    WHEN( "publishing within an invocation" )
    {
        auto& callee = session1;
        auto& subscriber = session2;

        std::string upperized;
        auto onEvent = [&upperized](Event, std::string str)
        {
            upperized = str;
        };

        auto shout =
            [&callee](Invocation, std::string str, YieldContext yield) -> Outcome
            {
                std::string upper = str;
                std::transform(upper.begin(), upper.end(),
                               upper.begin(), ::toupper);
                callee.publish(Pub("grapevine").withArgs(upper), yield).value();
                return Result({upper});
            };

        spawn(ioctx, [&](YieldContext yield)
        {
            callee.connect(where, yield).value();
            callee.join(Petition(testRealm), yield).value();
            callee.enroll(Procedure("shout"),
                           unpackedCoroRpc<std::string>(shout), yield).value();

            subscriber.connect(where, yield).value();
            subscriber.join(Petition(testRealm), yield).value();
            subscriber.subscribe(Topic("grapevine"),
                                  unpackedEvent<std::string>(onEvent),
                                  yield).value();

            subscriber.call(Rpc("shout").withArgs("hello"), yield).value();
            while (upperized.empty())
                suspendCoro(yield);
            CHECK_THAT( upperized, Equals("HELLO") );
            callee.disconnect();
            subscriber.disconnect();
        });

        ioctx.run();
    }

    WHEN( "unregistering within an invocation" )
    {
        auto& callee = session1;
        auto& caller = session2;

        int callCount = 0;
        Registration reg;

        auto oneShot = [&callCount, &reg, &callee](YieldContext yield)
        {
            // We need a yield context here for a blocking unregister.
            ++callCount;
            callee.unregister(reg, yield).value();
        };

        spawn(ioctx, [&](YieldContext yield)
        {
            callee.connect(where, yield).value();
            callee.join(Petition(testRealm), yield).value();
            reg = callee.enroll(Procedure("oneShot"),
                                 simpleCoroRpc<void>(oneShot), yield).value();

            caller.connect(where, yield).value();
            caller.join(Petition(testRealm), yield).value();

            caller.call(Rpc("oneShot"), yield).value();
            while (callCount == 0)
                suspendCoro(yield);
            CHECK( callCount == 1 );

            auto result = caller.call(Rpc("oneShot"), yield);
            CHECK( result == makeUnexpected(WampErrc::noSuchProcedure) );

            callee.disconnect();
            caller.disconnect();
        });

        ioctx.run();
    }

    WHEN( "publishing within an event" )
    {
        std::string upperized;
        bool published = false;

        auto onTalk =
            [&session1, &published](std::string str, YieldContext yield)
            {
                // We need a separate yield context here for a blocking
                // publish.
                std::string upper = str;
                std::transform(upper.begin(), upper.end(),
                               upper.begin(), ::toupper);
                session1.publish(Pub("onShout").withArgs(upper), yield).value();
                published = true;
            };

        auto onShout = [&upperized](Event, std::string str)
        {
            upperized = str;
        };

        spawn(ioctx, [&](YieldContext yield)
        {
            session1.connect(where, yield).value();
            session1.join(Petition(testRealm), yield).value();
            session1.subscribe(
                        Topic("onTalk"),
                        simpleCoroEvent<std::string>(onTalk), yield).value();

            session2.connect(where, yield).value();
            session2.join(Petition(testRealm), yield).value();
            session2.subscribe(
                        Topic("onShout"),
                        unpackedEvent<std::string>(onShout), yield).value();

            session2.publish(Pub("onTalk").withArgs("hello"), yield).value();
            while (upperized.empty() || !published)
                suspendCoro(yield);
            CHECK_THAT( upperized, Equals("HELLO") );
            session1.disconnect();
            session2.disconnect();
        });

        ioctx.run();
    }

    WHEN( "unsubscribing within an event" )
    {
        auto& publisher = session1;
        auto& subscriber = session2;

        int eventCount = 0;
        Subscription sub;

        auto onEvent = [&eventCount, &sub, &subscriber](Event, YieldContext yield)
        {
            // We need a yield context here for a blocking unsubscribe.
            ++eventCount;
            subscriber.unsubscribe(sub, yield).value();
        };

        spawn(ioctx, [&](YieldContext yield)
        {
            publisher.connect(where, yield).value();
            publisher.join(Petition(testRealm), yield).value();

            subscriber.connect(where, yield).value();
            subscriber.join(Petition(testRealm), yield).value();
            sub = subscriber.subscribe(Topic("onEvent"),
                                        unpackedCoroEvent(onEvent),
                                        yield).value();

            // Dummy RPC used to end polling
            int rpcCount = 0;
            subscriber.enroll(Procedure("dummy"),
                [&rpcCount](Invocation) -> Outcome
                {
                   ++rpcCount;
                   return {};
                },
                yield).value();

            publisher.publish(Pub("onEvent"), yield).value();
            while (eventCount == 0)
                suspendCoro(yield);

            // This publish should not have any subscribers
            publisher.publish(Pub("onEvent"), yield).value();

            // Invoke dummy RPC so that we know when to stop
            publisher.call(Rpc("dummy"), yield).value();

            // The event count should still be one
            CHECK( eventCount == 1 );

            publisher.disconnect();
            subscriber.disconnect();
        });

        ioctx.run();
    }
}}

//------------------------------------------------------------------------------
SCENARIO( "WAMP RPC Failures", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a ConnectionWish" )
{
    IoContext ioctx;
    const auto where = withTcp;

    WHEN( "registering an already existing procedure" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            RpcFixture f(ioctx, where);
            f.join(yield);
            f.enroll(yield);

            auto handler = [](Invocation) -> Outcome {return {};};

            auto reg = f.callee.enroll(Procedure("dynamic"), handler, yield);
            CHECK( reg == makeUnexpected(WampErrc::procedureAlreadyExists) );
            CHECK_THROWS_AS( reg.value(), error::Failure );
        });
        ioctx.run();
    }

    WHEN( "an RPC returns an error URI" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            int callCount = 0;
            RpcFixture f(ioctx, where);
            f.join(yield);
            f.enroll(yield);

            auto reg = f.callee.enroll(
                Procedure("rpc"),
                [&callCount](Invocation) -> Outcome
                {
                    ++callCount;
                    return Error(WampErrc::authorizationDenied)
                           .withArgs(123)
                           .withKwargs(Object{{{"foo"},{"bar"}}});
                },
                yield).value();

            Error error;
            auto result = f.caller.call(Rpc("rpc").captureError(error),
                                         yield);
            CHECK( result == makeUnexpected(WampErrc::authorizationDenied) );
            CHECK_THROWS_AS( result.value(), error::Failure );
            CHECK_FALSE( !error );
            CHECK( error.errorCode() == WampErrc::authorizationDenied );
            CHECK( error.args() == Array{123} );
            CHECK( error.kwargs() == (Object{{{"foo"},{"bar"}}}) );
            CHECK( callCount == 1 );
        });
        ioctx.run();
    }

    WHEN( "an RPC throws an error URI" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            int callCount = 0;
            RpcFixture f(ioctx, where);
            f.join(yield);
            f.enroll(yield);

            auto reg = f.callee.enroll(
                Procedure("rpc"),
                [&callCount](Invocation) -> Outcome
                {
                    ++callCount;
                    throw Error(WampErrc::authorizationDenied)
                          .withArgs(123)
                          .withKwargs(Object{{{"foo"},{"bar"}}});;
                    return {};
                },
                yield).value();

            {
                Error error;
                auto result = f.caller.call(Rpc("rpc").captureError(error),
                                             yield);
                CHECK( result == makeUnexpected(WampErrc::authorizationDenied) );
                CHECK_FALSE( !error );
                CHECK( error.errorCode() == WampErrc::authorizationDenied );
                CHECK( error.args() == Array{123} );
                CHECK( error.kwargs() == (Object{{{"foo"},{"bar"}}}) );
            }

            CHECK( callCount == 1 );
        });
        ioctx.run();
    }

    WHEN( "invoking a statically-typed RPC with invalid argument types" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            RpcFixture f(ioctx, where);
            f.join(yield);
            f.enroll(yield);

            // Check type mismatch
            auto result = f.caller.call(Rpc("static").withArgs(42, 42), yield);
            REQUIRE( !result );
            CHECK( result == makeUnexpected(WampErrc::invalidArgument) );
            CHECK_THROWS_AS( result.value(), error::Failure );
            CHECK( f.staticCount == 0 );

            // Check insufficient arguments
            result = f.caller.call(Rpc("static").withArgs(42), yield);
            CHECK( result == makeUnexpected(WampErrc::invalidArgument) );
            CHECK_THROWS_AS( result.value(), error::Failure );
            CHECK( f.staticCount == 0 );
        });
        ioctx.run();
    }

    WHEN( "invoking an RPC that throws a wamp::error::BadType exceptions" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            RpcFixture f(ioctx, where);
            f.join(yield);

            f.callee.enroll(
                Procedure("bad_conversion"),
                [](Invocation inv)
                {
                    inv.args().front().to<String>();
                    return Result();
                },
                yield).value();

            f.callee.enroll(
                Procedure("bad_conv_coro"),
                simpleCoroRpc<void, Variant>(
                [](Variant v, YieldContext yield) { v.to<String>(); }),
                yield).value();

            f.callee.enroll(
                Procedure("bad_access"),
                simpleRpc<void, Variant>( [](Variant v){v.as<String>();} ),
                yield).value();

            f.callee.enroll(
                Procedure("bad_access_coro"),
                unpackedCoroRpc<Variant>(
                [](Invocation inv, Variant v, YieldContext yield)
                {
                    v.as<String>();
                    return Result();
                }),
                yield).value();


            // Check bad conversion
            auto result = f.caller.call(Rpc("bad_conversion").withArgs(42),
                                         yield);
            CHECK( result == makeUnexpected(WampErrc::invalidArgument) );
            CHECK_THROWS_AS( result.value(), error::Failure );

            // Check bad conversion in coroutine handler
            result = f.caller.call(Rpc("bad_conv_coro").withArgs(42), yield);
            CHECK( result == makeUnexpected(WampErrc::invalidArgument) );
            CHECK_THROWS_AS( result.value(), error::Failure );

            // Check bad access
            result = f.caller.call(Rpc("bad_access").withArgs(42), yield);
            CHECK( result == makeUnexpected(WampErrc::invalidArgument) );
            CHECK_THROWS_AS( result.value(), error::Failure );

            // Check bad access in couroutine handler
            result = f.caller.call(Rpc("bad_access_coro").withArgs(42), yield);
            CHECK( result == makeUnexpected(WampErrc::invalidArgument) );
            CHECK_THROWS_AS( result.value(), error::Failure );
        });
        ioctx.run();
    }

    WHEN( "a callee leaves without returning" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            RpcFixture f(ioctx, where);
            f.join(yield);
            f.enroll(yield);

            auto reg = f.callee.enroll(
                Procedure("rpc"),
                [&](Invocation) -> Outcome
                {
                    f.callee.leave([](ErrorOr<Reason>) {});
                    return deferment;
                },
                yield).value();

            Error error;
            auto result = f.caller.call(Rpc("rpc").captureError(error),
                                         yield);
            CHECK( result == makeUnexpected(WampErrc::cancelled) );
            CHECK_FALSE( !error );
            CHECK( error.errorCode() == WampErrc::cancelled );
        });
        ioctx.run();
    }
}}

//------------------------------------------------------------------------------
SCENARIO( "Invalid WAMP RPC URIs", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a ConnectionWish" )
{
    IoContext ioctx;
    const auto where = withTcp;

    WHEN( "enrolling with an invalid procedure URI" )
    {
        checkInvalidUri(
            [](Session& session, YieldContext yield)
            {
                return session.enroll(Procedure("#bad"),
                                      [](Invocation)->Outcome {return {};},
                                      yield);
            }
        );
    }

    WHEN( "calling with an invalid procedure URI" )
    {
        checkInvalidUri(
            [](Session& session, YieldContext yield)
            {
                return session.call(Rpc("#bad"), yield);
            } );

        AND_WHEN( "calling with args" )
        {
            checkInvalidUri(
                [](Session& session, YieldContext yield)
                {
                    return session.call(Rpc("#bad").withArgs(42), yield);
                } );
        }
    }
}}

//------------------------------------------------------------------------------
SCENARIO( "WAMP Disconnect/Leave During Async RPC Ops", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a ConnectionWish" )
{
    IoContext ioctx;
    const auto where = withTcp;

    WHEN( "disconnecting during async enroll" )
    {
        checkDisconnect<Registration>(
            [](Session& session, YieldContext yield,
               bool& completed, ErrorOr<Registration>& result)
            {
                session.join(Petition(testRealm), yield).value();
                session.enroll(Procedure("rpc"),
                               [](Invocation)->Outcome {return {};},
                               [&](ErrorOr<Registration> reg)
                               {
                                   completed = true;
                                   result = reg;
                               });
            });
    }

    WHEN( "disconnecting during async unregister" )
    {
        checkDisconnect<bool>([](Session& session, YieldContext yield,
                                 bool& completed, ErrorOr<bool>& result)
        {
            session.join(Petition(testRealm), yield).value();
            auto reg = session.enroll(Procedure("rpc"),
                                      [](Invocation)->Outcome{return {};},
                                      yield).value();
            session.unregister(reg, [&](ErrorOr<bool> unregistered)
            {
                completed = true;
                result = unregistered;
            });
        });
    }

    WHEN( "disconnecting during async unregister via session" )
    {
        checkDisconnect<bool>([](Session& session,
                                 YieldContext yield,
                                 bool& completed,
                                 ErrorOr<bool>& result)
        {
            session.join(Petition(testRealm), yield).value();
            auto reg = session.enroll(Procedure("rpc"),
                                      [](Invocation)->Outcome{return {};},
                                      yield).value();
            session.unregister(reg, [&](ErrorOr<bool> unregistered)
            {
                completed = true;
                result = unregistered;
            });
        });
    }

    WHEN( "disconnecting during async call" )
    {
        checkDisconnect<Result>([](Session& session, YieldContext yield,
                                   bool& completed, ErrorOr<Result>& result)
        {
            session.join(Petition(testRealm), yield).value();
            session.call(Rpc("rpc").withArgs("foo"),
                [&](ErrorOr<Result> callResult)
                {
                    completed = true;
                    result = callResult;
                });
        });
    }

    WHEN( "asynchronous enroll just before leaving" )
    {
        ErrorOr<Registration> reg;
        spawn(ioctx, [&](YieldContext yield)
        {
            Session s(ioctx);
            s.connect(where, yield).value();
            s.join(Petition(testRealm), yield).value();
            s.enroll(Procedure("procedure"),
                     [&](Invocation) -> Outcome {return {};},
                     [&](ErrorOr<Registration> r) {reg = r;});
            s.leave(yield).value();
            CHECK( s.state() == SessionState::closed );
        });

        ioctx.run();
        CHECK( reg.has_value() );
    }

    WHEN( "asynchronous unregister just before leaving" )
    {
        ErrorOr<bool> done;
        spawn(ioctx, [&](YieldContext yield)
        {
            Session s(ioctx);
            s.connect(where, yield).value();
            s.join(Petition(testRealm), yield).value();
            auto reg = s.enroll(Procedure("procedure"),
                                [&](Invocation) -> Outcome {return {};},
                                yield).value();
            s.unregister(reg, [&](ErrorOr<bool> ok) {done = ok;});
            s.leave(yield).value();
            CHECK( s.state() == SessionState::closed );
        });

        ioctx.run();
        REQUIRE( done.has_value() );
        CHECK( done.value() == true );
    }

    WHEN( "asynchronous call just before leaving" )
    {
        ErrorOr<Result> result;
        spawn(ioctx, [&](YieldContext yield)
        {
            Session s(ioctx);
            s.connect(where, yield).value();
            s.join(Petition(testRealm), yield).value();
            s.call(Rpc("procedure"),
                   [&](ErrorOr<Result> r) {result = r;});
            s.leave(yield).value();
            CHECK( s.state() == SessionState::closed );
        });

        ioctx.run();
        REQUIRE_FALSE( result.has_value() );
        CHECK( result.error() == WampErrc::noSuchProcedure );
    }
}}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
