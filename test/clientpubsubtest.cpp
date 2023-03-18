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
            CHECK( f.publisher.publish(Pub("str.num")
                                           .withArgs("one", 1)).value() );
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
            CHECK( f.publisher.publish(Pub("other")).value() );
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
            f.otherSubscriber.join(Realm(testRealm), yield).value();

            // Reestablish the dynamic subscription.
            using namespace std::placeholders;
            f.dynamicSub = f.subscriber.subscribe(
                    Topic("str.num"),
                    std::bind(&PubSubFixture::onDynamicEvent, &f, _1),
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
                Topic("str.num"),
                simpleEvent<std::string, int>([&](std::string s, int n)
                {
                    f.staticArgs = Array{{s, n}};
                }),
                yield).value();

            CHECK( f.publisher.publish(Pub("str.num")
                                          .withArgs("one", 1)).value() );

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
                return session.subscribe(Topic("#bad"), [](Event) {}, yield);
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

    // TODO: Check for Errc::abandonned

    WHEN( "disconnecting during async subscribe" )
    {
        checkDisconnect<Subscription>(
            [](Session& session, YieldContext yield, bool& completed,
               ErrorOr<Subscription>& result)
            {
                session.join(Realm(testRealm), yield).value();
                session.subscribe(Topic("topic"), [] (Event) {},
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
            session.join(Realm(testRealm), yield).value();
            auto sub = session.subscribe(Topic("topic"), [] (Event) {},
                                         yield).value();
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
            session.join(Realm(testRealm), yield).value();
            auto sub = session.subscribe(Topic("topic"), [](Event) {},
                                         yield).value();
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
            session.join(Realm(testRealm), yield).value();
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
            session.join(Realm(testRealm), yield).value();
            session.publish(Pub("topic").withArgs("foo"),
                [&](ErrorOr<PublicationId> pid)
                {
                    completed = true;
                    result = pid;
                });
        });
    }

    // TODO: Other async pub-sub ops
    WHEN( "asynchronous publish just before leaving" )
    {
        ErrorOr<PublicationId> result;
        spawn(ioctx, [&](YieldContext yield)
        {
            Session s(ioctx);
            s.connect(where, yield).value();
            s.join(Realm(testRealm), yield).value();
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
