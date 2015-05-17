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

using namespace wamp;

namespace
{

const std::string testRealm = "cppwamp.test";
const short testPort = 12345;

Connector::Ptr tcp(AsioService& iosvc)
{
#ifdef CPPWAMP_USE_LEGACY_CONNECTORS
    return legacyConnector<Json>(iosvc, TcpHost("localhost", testPort));
#else
    return connector<Json>(iosvc, TcpHost("localhost", testPort));
#endif
}

//------------------------------------------------------------------------------
struct RpcFixture
{
    template <typename TConnector>
    RpcFixture(TConnector cnct)
        : caller(CoroSession<>::create(cnct)),
          callee(CoroSession<>::create(cnct))
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
    PubSubFixture(TConnector cnct)
        : publisher(CoroSession<>::create(cnct)),
          subscriber(CoroSession<>::create(cnct))
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

} // anonymous namespace


//------------------------------------------------------------------------------
SCENARIO( "WAMP RPC advanced features", "[WAMP]" )
{
GIVEN( "a caller and a callee" )
{
    AsioService iosvc;
    RpcFixture f(tcp(iosvc));

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
    PubSubFixture f(tcp(iosvc));

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
        auto subscriber2 = CoroSession<>::create(tcp(iosvc));

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

#endif // #if CPPWAMP_TESTING_WAMP
