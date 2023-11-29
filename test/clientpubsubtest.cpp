/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#if defined(CPPWAMP_TEST_HAS_CORO)

#include "clienttesting.hpp"

using namespace test;
using namespace Catch::Matchers;

//------------------------------------------------------------------------------
SCENARIO( "WAMP Pub-Sub", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a ConnectionWish" )
{
    IoContext ioctx;
    const auto where = withTcp;

    WHEN( "publishing and subscribing" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(ioctx, where);
            f.join(yield);
            f.subscribe(yield);

            // Check dynamic and static subscriptions.
            f.publisher.publish(Pub("str.num").withArgs("one", 1));
            pid = f.publisher.publish(Pub("str.num").withArgs("two", 2),
                                       yield).value();
            while (f.dynamicPubs.size() < 2 || f.staticPubs.size() < 2)
                suspendCoro(yield);

            REQUIRE( f.dynamicPubs.size() == 2 );
            CHECK( f.dynamicPubs.back() == pid );
            CHECK(( f.dynamicArgs == Array{"two", 2} ));
            REQUIRE( f.staticPubs.size() == 2 );
            CHECK( f.staticPubs.back() == pid );
            CHECK(( f.staticArgs == Array{"two", 2} ));
            CHECK( f.otherPubs.empty() );

            // Check subscription from another client.
            f.publisher.publish(Pub("other"));
            pid = f.publisher.publish(Pub("other"), yield).value();
            while (f.otherPubs.size() < 2)
                suspendCoro(yield);
            CHECK( f.dynamicPubs.size() == 2 );
            CHECK( f.staticPubs.size() == 2 );
            REQUIRE( f.otherPubs.size() == 2 );
            CHECK( f.otherPubs.back() == pid );

            // Unsubscribe the dynamic subscription manually.
            f.subscriber.unsubscribe(f.dynamicSub, yield).value();

            // Check that the dynamic slot no longer fires, and that the
            // static slot still fires.
            pid = f.publisher.publish(Pub("str.num").withArgs("three", 3),
                                       yield).value();
            while (f.staticPubs.size() < 3)
                suspendCoro(yield);
            REQUIRE( f.dynamicPubs.size() == 2 );
            REQUIRE( f.staticPubs.size() == 3 );
            CHECK( f.staticPubs.back() == pid );
            CHECK(( f.staticArgs == Array{"three", 3} ));

            // Unsubscribe the static subscription via RAII.
            f.staticSub = ScopedSubscription();

            // Check that the dynamic and static slots no longer fire, and
            // that the "other" slot still fires.
            f.publisher.publish(Pub("str.num").withArgs("four", 4),
                                 yield).value();
            pid = f.publisher.publish(Pub("other"), yield).value();
            while (f.otherPubs.size() < 3)
                suspendCoro(yield);
            CHECK( f.dynamicPubs.size() == 2 );
            CHECK( f.staticPubs.size() == 3 );
            REQUIRE( f.otherPubs.size() == 3 );
            CHECK( f.otherPubs.back() == pid );

            // Make the "other" subscriber leave and rejoin the realm.
            f.otherSubscriber.leave(yield).value();
            f.otherSubscriber.join(testRealm, yield).value();

            // Reestablish the dynamic subscription.
            using namespace std::placeholders;
            f.dynamicSub = f.subscriber.subscribe(
                    "str.num",
                    [&f](Event ev) {f.onDynamicEvent(std::move(ev));},
                    yield).value();

            // Check that only the dynamic slot still fires.
            f.publisher.publish(Pub("other"), yield).value();
            pid = f.publisher.publish(Pub("str.num").withArgs("five", 5),
                                       yield).value();
            while (f.dynamicPubs.size() < 3)
                suspendCoro(yield);
            CHECK( f.dynamicPubs.size() == 3 );
            CHECK( f.staticPubs.size() == 3 );
            REQUIRE( f.otherPubs.size() == 3 );
            CHECK( f.dynamicPubs.back() == pid );
            CHECK(( f.dynamicArgs == Array{"five", 5} ));
        });

        ioctx.run();
    }

    WHEN( "subscribing simple events" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            PubSubFixture f(ioctx, where);
            f.join(yield);
            f.staticSub = f.subscriber.subscribe(
                "str.num",
                simpleEvent<std::string, int>([&](std::string s, int n)
                {
                    f.staticArgs = Array{{s, n}};
                }),
                yield).value();

            f.publisher.publish(Pub("str.num").withArgs("one", 1));

            while (f.staticArgs.size() < 2)
                suspendCoro(yield);
            CHECK(( f.staticArgs == Array{"one", 1} ));
        });
        ioctx.run();
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "WAMP Subscription Lifetimes", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a ConnectionWish" )
{
    IoContext ioctx;
    const auto where = withTcp;

    WHEN( "unsubscribing multiple times" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(ioctx, where);
            f.join(yield);
            f.subscribe(yield);

            // Unsubscribe the dynamic subscription manually.
            f.dynamicSub.unsubscribe();

            // Unsubscribe the dynamic subscription again via RAII.
            f.dynamicSub = ScopedSubscription();

            // Check that the dynamic slot no longer fires, and that the
            // static slot still fires.
            pid = f.publisher.publish(Pub("str.num").withArgs("foo", 42),
                                       yield).value();
            while (f.staticPubs.size() < 1)
                suspendCoro(yield);
            REQUIRE( f.dynamicPubs.size() == 0 );
            REQUIRE( f.staticPubs.size() == 1 );
            CHECK( f.staticPubs.back() == pid );

            // Unsubscribe the static subscription manually.
            f.subscriber.unsubscribe(f.staticSub, yield).value();

            // Unsubscribe the static subscription again manually.
            f.staticSub.unsubscribe();

            // Check that the dynamic and static slots no longer fire.
            // Publish to the "other" subscription so that we know when
            // to stop polling.
            f.publisher.publish(Pub("str.num").withArgs("foo", 42),
                                 yield).value();
            pid = f.publisher.publish(Pub("other"), yield).value();
            while (f.otherPubs.size() < 1)
                suspendCoro(yield);
            CHECK( f.dynamicPubs.size() == 0 );
            CHECK( f.staticPubs.size() == 1 );
            REQUIRE( f.otherPubs.size() == 1 );
            CHECK( f.otherPubs.back() == pid );
        });

        ioctx.run();
    }

    WHEN( "unsubscribing after session is destroyed" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(ioctx, where);
            f.join(yield);
            f.subscribe(yield);

            // Move and destroy f.subscriber.impl_
            {
                Session temp{std::move(f.subscriber)};
            }

            // Unsubscribe the dynamic subscription manually.
            REQUIRE_NOTHROW( f.dynamicSub.unsubscribe() );

            // Unsubscribe the static subscription via RAII.
            REQUIRE_NOTHROW( f.staticSub = ScopedSubscription() );

            // Check that the dynamic and static slots no longer fire.
            // Publish to the "other" subscription so that we know when
            // to stop polling.
            f.publisher.publish(Pub("str.num").withArgs("foo", 42),
                                 yield).value();
            pid = f.publisher.publish(Pub("other"), yield).value();
            while (f.otherPubs.size() < 1)
                suspendCoro(yield);
            CHECK( f.dynamicPubs.size() == 0 );
            CHECK( f.staticPubs.size() == 0 );
            REQUIRE( f.otherPubs.size() == 1 );
            CHECK( f.otherPubs.back() == pid );
        });

        ioctx.run();
    }

    WHEN( "unsubscribing after leaving" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(ioctx, where);
            f.join(yield);
            f.subscribe(yield);

            // Make the subscriber client leave the session.
            f.subscriber.leave(yield).value();

            // Unsubscribe the dynamic subscription via RAII.
            REQUIRE_NOTHROW( f.dynamicSub = ScopedSubscription() );

            // Unsubscribe the static subscription manually.
            auto unsubscribed = f.subscriber.unsubscribe(f.staticSub, yield);
            REQUIRE( unsubscribed.has_value() );
            CHECK( unsubscribed.value() == false );
            REQUIRE_NOTHROW( f.staticSub.unsubscribe() );

            // Check that the dynamic and static slots no longer fire.
            // Publish to the "other" subscription so that we know when
            // to stop polling.
            f.publisher.publish(Pub("str.num").withArgs("foo", 42),
                                 yield).value();
            pid = f.publisher.publish(Pub("other"), yield).value();
            while (f.otherPubs.size() < 1)
                suspendCoro(yield);
            CHECK( f.dynamicPubs.size() == 0 );
            CHECK( f.staticPubs.size() == 0 );
            REQUIRE( f.otherPubs.size() == 1 );
            CHECK( f.otherPubs.back() == pid );
        });

        ioctx.run();
    }

    WHEN( "unsubscribing after disconnecting" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(ioctx, where);
            f.join(yield);
            f.subscribe(yield);

            // Make the subscriber client disconnect.
            f.subscriber.disconnect();

            // Unsubscribe the dynamic subscription manually.
            auto unsubscribed = f.subscriber.unsubscribe(f.dynamicSub, yield);
            REQUIRE( unsubscribed.has_value() );
            CHECK( unsubscribed.value() == false );
            REQUIRE_NOTHROW( f.dynamicSub.unsubscribe() );

            // Unsubscribe the static subscription via RAII.
            REQUIRE_NOTHROW( f.staticSub= ScopedSubscription() );

            // Check that the dynamic and static slots no longer fire.
            // Publish to the "other" subscription so that we know when
            // to stop polling.
            f.publisher.publish(Pub("str.num").withArgs("foo", 42),
                                 yield).value();
            pid = f.publisher.publish(Pub("other"), yield).value();
            while (f.otherPubs.size() < 1)
                suspendCoro(yield);
            CHECK( f.dynamicPubs.size() == 0 );
            CHECK( f.staticPubs.size() == 0 );
            REQUIRE( f.otherPubs.size() == 1 );
            CHECK( f.otherPubs.back() == pid );
        });

        ioctx.run();
    }

    WHEN( "unsubscribing after reset" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(ioctx, where);
            f.join(yield);
            f.subscribe(yield);

            // Move and destroy f.subscriber.impl_
            {
                Session temp{std::move(f.subscriber)};
            }

            // Unsubscribe the static subscription via RAII.
            REQUIRE_NOTHROW( f.staticSub = ScopedSubscription() );

            // Check that the dynamic and static slots no longer fire.
            // Publish to the "other" subscription so that we know when
            // to stop polling.
            f.publisher.publish(Pub("str.num").withArgs("foo", 42),
                                 yield).value();
            pid = f.publisher.publish(Pub("other"), yield).value();
            while (f.otherPubs.size() < 1)
                suspendCoro(yield);
            CHECK( f.dynamicPubs.size() == 0 );
            CHECK( f.staticPubs.size() == 0 );
            REQUIRE( f.otherPubs.size() == 1 );
            CHECK( f.otherPubs.back() == pid );
        });

        ioctx.run();
    }

    WHEN( "moving a ScopedSubscription" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            PubSubFixture f(ioctx, where);
            f.join(yield);
            f.subscribe(yield);

            // Check move construction.
            {
                ScopedSubscription sub(std::move(f.dynamicSub));
                CHECK_FALSE( !sub );
                CHECK( sub.id() >= 0 );
                CHECK( !f.dynamicSub );

                f.publisher.publish(Pub("str.num").withArgs("", 0),
                                     yield).value();
                while (f.dynamicPubs.size() < 1 || f.staticPubs.size() < 1)
                    suspendCoro(yield);
                CHECK( f.dynamicPubs.size() == 1 );
                CHECK( f.staticPubs.size() == 1 );
            }
            // 'sub' goes out of scope here.
            f.publisher.publish(Pub("str.num").withArgs("", 0), yield).value();
            f.publisher.publish(Pub("other"), yield).value();
            while (f.otherPubs.size() < 1)
                suspendCoro(yield);
            CHECK( f.dynamicPubs.size() == 1 );
            CHECK( f.staticPubs.size() == 2 );
            CHECK( f.otherPubs.size() == 1 );

            // Check move assignment.
            {
                ScopedSubscription sub;
                sub = std::move(f.staticSub);
                CHECK_FALSE( !sub );
                CHECK( sub.id() >= 0 );
                CHECK( !f.staticSub );

                f.publisher.publish(Pub("str.num").withArgs("", 0),
                                     yield).value();
                while (f.staticPubs.size() < 3)
                    suspendCoro(yield);
                CHECK( f.staticPubs.size() == 3 );
            }
            // 'sub' goes out of scope here.
            f.publisher.publish(Pub("str.num").withArgs("", 0), yield).value();
            f.publisher.publish(Pub("other"), yield).value();
            while (f.otherPubs.size() < 2)
                suspendCoro(yield);
            CHECK( f.staticPubs.size() == 3 ); // staticPubs count the same
            CHECK( f.otherPubs.size() == 2 );
        });
        ioctx.run();
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "WAMP Pub-Sub Failures", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a ConnectionWish" )
{
    IoContext ioctx;
    const auto where = withTcp;

    WHEN( "receiving a statically-typed event with invalid argument types" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(ioctx, where);
            f.join(yield);
            f.subscribe(yield);

            // Publications with invalid arguments should be ignored.
            CHECK_NOTHROW( f.publisher.publish(
                               Pub("str.num").withArgs(42, 42), yield ).value() );

            // Publish with valid types so that we know when to stop polling.
            pid = f.publisher.publish(Pub("str.num").withArgs("foo", 42),
                                       yield).value();
            while (f.staticPubs.size() < 1)
                suspendCoro(yield);
            REQUIRE( f.staticPubs.size() == 1 );
            CHECK( f.staticPubs.back() == pid );

            // Publications with extra arguments should be handled,
            // as long as the required arguments have valid types.
            CHECK_NOTHROW( pid = f.publisher.publish(
                    Pub("str.num").withArgs("foo", 42, true), yield).value() );
            while (f.staticPubs.size() < 2)
                suspendCoro(yield);
            REQUIRE( f.staticPubs.size() == 2 );
            CHECK( f.staticPubs.back() == pid );
        });
        ioctx.run();
    }

    WHEN( "an event handler throws wamp::error::BadType exceptions" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            std::vector<Incident> incidents;
            PubSubFixture f(ioctx, where);
            f.subscriber.observeIncidents(
                [&incidents](Incident i) {incidents.push_back(i);});

            f.join(yield);
            f.subscribe(yield);

            f.subscriber.subscribe(
                "bad_conversion",
                simpleEvent<Variant>([](Variant v) {v.to<String>();}),
                yield).value();

            f.subscriber.subscribe(
                "bad_access",
                [](Event event) {event.args().front().as<String>();},
                yield).value();

            f.subscriber.subscribe(
                "bad_conversion_coro",
                simpleCoroEvent<Variant>(
                    [](Variant v, YieldContext y) { v.to<String>(); }),
                yield).value();

            f.subscriber.subscribe(
                "bad_access_coro",
                unpackedCoroEvent<Variant>(
                    [](Event ev, Variant v, YieldContext y) {v.to<String>();}),
                yield).value();

            f.publisher.publish(Pub("bad_conversion").withArgs(42));
            f.publisher.publish(Pub("bad_access").withArgs(42));
            f.publisher.publish(Pub("bad_conversion_coro").withArgs(42));
            f.publisher.publish(Pub("bad_access_coro").withArgs(42));
            f.publisher.publish(Pub("other"));

            while (f.otherPubs.empty() || incidents.size() < 2)
                suspendCoro(yield);

            // The coroutine event handlers will not trigger
            // incidents because the error::BadType exeception cannot
            // be propagated to Client by time it's thrown from within
            // the coroutine.
            REQUIRE( incidents.size() == 2 );
            CHECK( incidents.at(0).kind() == IncidentKind::eventError );
            CHECK( incidents.at(0).error() == WampErrc::invalidArgument );
            CHECK( incidents.at(1).kind() == IncidentKind::eventError );
            CHECK( incidents.at(1).error() == WampErrc::invalidArgument );
        });
        ioctx.run();
    }

    WHEN( "Unsubscribing using a Subscription object from another Session" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            Session s1{ioctx};
            s1.connect(withTcp, yield).value();
            s1.join(testRealm, yield).value();
            auto sub = s1.subscribe("foo", [](Event) {}, yield).value();

            Session s2{ioctx};
            s2.connect(withTcp, yield).value();
            s2.join(testRealm, yield).value();
            CHECK_THROWS_AS(s2.unsubscribe(sub), error::Logic);
            CHECK_THROWS_AS(s2.unsubscribe(sub, yield), error::Logic);
        });

        ioctx.run();
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "Invalid WAMP Pub-Sub URIs", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a ConnectionWish" )
{
    IoContext ioctx;
    const auto where = withTcp;

    WHEN( "subscribing with an invalid topic URI" )
    {
        checkInvalidUri(
            [](Session& session, YieldContext yield)
            {
                return session.subscribe("#bad", [](Event) {}, yield);
            } );
    }

    WHEN( "publishing with an invalid topic URI" )
    {
        checkInvalidUri(
            [](Session& session, YieldContext yield)
            {
                return session.publish(Pub("#bad"), yield);
            } );

        AND_WHEN( "publishing with args" )
        {
            checkInvalidUri(
                [](Session& session, YieldContext yield)
                {
                    return session.publish(Pub("#bad").withArgs(42), yield);
                } );
        }
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "WAMP Disconnect/Leave During Async Pub-Sub Ops", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a ConnectionWish" )
{
    IoContext ioctx;
    const auto where = withTcp;

    WHEN( "disconnecting during async subscribe" )
    {
        checkDisconnect<Subscription>(
            [](Session& session, YieldContext yield, bool& completed,
               ErrorOr<Subscription>& result)
            {
                session.join(testRealm, yield).value();
                session.subscribe(
                    "topic",
                    [] (Event) {},
                    [&](ErrorOr<Subscription> sub)
                    {
                        completed = true;
                        result = sub;
                    });
            });
    }

    WHEN( "disconnecting during async unsubscribe" )
    {
        checkDisconnect<bool>([](Session& session, YieldContext yield,
                                 bool& completed, ErrorOr<bool>& result)
        {
            session.join(testRealm, yield).value();
            auto sub = session.subscribe("topic", [] (Event) {}, yield).value();
            session.unsubscribe(sub, [&](ErrorOr<bool> unsubscribed)
            {
                completed = true;
                result = unsubscribed;
            });
        });
    }

    WHEN( "disconnecting during async unsubscribe via session" )
    {
        checkDisconnect<bool>([](Session& session, YieldContext yield,
                                 bool& completed, ErrorOr<bool>& result)
        {
            session.join(testRealm, yield).value();
            auto sub = session.subscribe("topic", [](Event) {}, yield).value();
            session.unsubscribe(sub, [&](ErrorOr<bool> unsubscribed)
            {
                completed = true;
                result = unsubscribed;
            });
        });
    }

    WHEN( "disconnecting during async publish" )
    {
        checkDisconnect<PublicationId>([](Session& session, YieldContext yield,
                                          bool& completed,
                                          ErrorOr<PublicationId>& result)
        {
            session.join(testRealm, yield).value();
            session.publish(Pub("topic"), [&](ErrorOr<PublicationId> pid)
            {
                completed = true;
                result = pid;
            });
        });
    }

    WHEN( "disconnecting during async publish with args" )
    {
        checkDisconnect<PublicationId>([](Session& session, YieldContext yield,
                                          bool& completed,
                                          ErrorOr<PublicationId>& result)
        {
            session.join(testRealm, yield).value();
            session.publish(Pub("topic").withArgs("foo"),
                [&](ErrorOr<PublicationId> pid)
                {
                    completed = true;
                    result = pid;
                });
        });
    }

    WHEN( "asynchronous subscribe just before leaving" )
    {
        ErrorOr<Subscription> sub;
        spawn(ioctx, [&](YieldContext yield)
        {
            Session s(ioctx);
            s.connect(where, yield).value();
            s.join(testRealm, yield).value();
            s.subscribe("topic",
                        [](Event) {},
                        [&](ErrorOr<Subscription> s) {sub = s;});
            s.leave(yield).value();
            CHECK( s.state() == SessionState::closed );
        });

        ioctx.run();
        CHECK( sub.has_value() );
    }

    WHEN( "asynchronous unsubscribe just before leaving" )
    {
        ErrorOr<bool> done;
        spawn(ioctx, [&](YieldContext yield)
        {
            Session s(ioctx);
            s.connect(where, yield).value();
            s.join(testRealm, yield).value();
            auto sub = s.subscribe("topic", [](Event) {}, yield).value();
            s.unsubscribe(sub, [&](ErrorOr<bool> ok) {done = ok;});
            s.leave(yield).value();
            CHECK( s.state() == SessionState::closed );
        });

        ioctx.run();
        REQUIRE( done.has_value() );
        CHECK( done.value() == true );
    }

    WHEN( "asynchronous publish just before leaving" )
    {
        ErrorOr<PublicationId> result;
        spawn(ioctx, [&](YieldContext yield)
        {
            Session s(ioctx);
            s.connect(where, yield).value();
            s.join(testRealm, yield).value();
            s.publish(Pub("topic"),
                      [&](ErrorOr<PublicationId> p) {result = p;});
            s.leave(yield).value();
            CHECK( s.state() == SessionState::closed );
        });

        ioctx.run();
        CHECK( result.has_value() );
    }
}}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
