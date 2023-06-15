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
#include "routerfixture.hpp"

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
        welcome = publisher.join(Petition(testRealm), yield).value();
        publisherId = welcome.sessionId();
        subscriber.connect(where, yield).value();
        subscriber.join(Petition(testRealm), yield).value();
    }

    void disconnect()
    {
        publisher.disconnect();
        subscriber.disconnect();
    }

    ConnectionWish where;

    Session publisher;
    Session subscriber;

    Welcome welcome;
    SessionId publisherId = -1;
    SessionId subscriberId = -1;
};

} // anonymous namespace


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
            REQUIRE(f.welcome.features().broker().all_of(
                BrokerFeatures::publisherIdentification));

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
            REQUIRE(f.welcome.features().broker().all_of(
                BrokerFeatures::patternBasedSubscription));

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

            // Crossbar treats an unknown match option as a protocol error
            // and aborts the session. The CppWAMP router instead returns
            // an ERROR message. The spec does not mandate the response one
            // way or another.
            if (test::RouterFixture::enabled())
            {
                auto errorOrSub = f.subscriber.subscribe(
                    Topic("com..onEvent").withOption("match", "bogus"),
                    [](Event) {},
                    yield);
                REQUIRE_FALSE(errorOrSub.has_value());
                CHECK(errorOrSub.error() == WampErrc::optionNotAllowed);
            }

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
            REQUIRE(f.welcome.features().broker().all_of(
                BrokerFeatures::publisherExclusion));

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
            REQUIRE(f.welcome.features().broker().all_of(
                BrokerFeatures::subscriberBlackWhiteListing));

            subscriber2.connect(withTcp, yield).value();
            auto subscriber2Id =
                subscriber2.join(Petition(testRealm), yield).value().sessionId();

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

#endif // defined(CPPWAMP_TEST_HAS_CORO)
