/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#if defined(CPPWAMP_TEST_HAS_JSON) || defined(CPPWAMP_TEST_HAS_MSGPACK)
    #define CPPWAMP_TEST_HAS_SERIALIZER 1
#endif

#if defined(CPPWAMP_TEST_HAS_JSON) && defined(CPPWAMP_TEST_HAS_MSGPACK)
    #define CPPWAMP_TEST_HAS_ALTERNATE_SERIALIZER 1
#endif

#if defined(CPPWAMP_TEST_HAS_ALTERNATE_SERIALIZER) || \
    CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS
        #define CPPWAMP_TEST_HAS_ALTERNATE_CONNECTOR 1
#endif

#if defined(CPPWAMP_TEST_HAS_CORO) && defined(CPPWAMP_TEST_HAS_SERIALIZER)

#include <algorithm>
#include <cctype>
#include <catch2/catch.hpp>
#include <cppwamp/coro/corounpacker.hpp>
#include <cppwamp/coro/corosession.hpp>
#include <cppwamp/tcp.hpp>
#include <cppwamp/unpacker.hpp>
#include <cppwamp/internal/config.hpp>

#ifdef CPPWAMP_TEST_HAS_JSON
    #include <cppwamp/json.hpp>
#endif

#ifdef CPPWAMP_TEST_HAS_MSGPACK
    #include <cppwamp/msgpack.hpp>
#endif

#if CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS
    #include <cppwamp/uds.hpp>
#endif

using namespace wamp;
using namespace Catch::Matchers;

namespace
{

#ifndef CPPWAMP_NO_JSON
    using PreferredCodec = Json;
#else
    using PreferredCodec = Msgpack;
#endif

const std::string testRealm = "cppwamp.test";
const unsigned short validPort = 12345;
const unsigned short invalidPort = 54321;
const std::string testUdsPath = "./.crossbar/udstest";

Connector::Ptr tcp(AsioContext& ioctx)
{
    return connector<PreferredCodec>(ioctx, TcpHost("localhost", validPort));
}

Connector::Ptr invalidTcp(AsioContext& ioctx)
{
    return connector<PreferredCodec>(ioctx, TcpHost("localhost", invalidPort));
}

#ifdef CPPWAMP_TEST_HAS_ALTERNATE_CONNECTOR
Connector::Ptr alternateTcp(AsioContext& ioctx)
{
#if CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS
    auto where = UdsPath(testUdsPath);
#else
    auto where = TcpHost("localhost", validPort);
#endif
#ifndef CPPWAMP_NO_MSGPACK
    using CodecType = Msgpack;
#else
    using CodecType = Json;
#endif

    return connector<CodecType>(ioctx, where);
}
#endif // CPPWAMP_TEST_HAS_ALTERNATE_CONNECTOR


//------------------------------------------------------------------------------
struct PubSubFixture
{
    using PubVec = std::vector<PublicationId>;

    template <typename TConnector>
    PubSubFixture(AsioContext& ioctx, TConnector cnct)
        : ioctx(ioctx),
          publisher(CoroSession<>::create(ioctx, cnct)),
          subscriber(CoroSession<>::create(ioctx, cnct)),
          otherSubscriber(CoroSession<>::create(ioctx, cnct))
    {}

    void join(boost::asio::yield_context yield)
    {
        publisher->connect(yield);
        publisher->join(Realm(testRealm), yield);
        subscriber->connect(yield);
        subscriber->join(Realm(testRealm), yield);
        otherSubscriber->connect(yield);
        otherSubscriber->join(Realm(testRealm), yield);
    }

    void subscribe(boost::asio::yield_context yield)
    {
        using namespace std::placeholders;
        dynamicSub = subscriber->subscribe(
                Topic("str.num"),
                std::bind(&PubSubFixture::onDynamicEvent, this, _1),
                yield);

        staticSub = subscriber->subscribe(
                Topic("str.num"),
                unpackedEvent<std::string, int>(
                            std::bind(&PubSubFixture::onStaticEvent, this,
                                      _1, _2, _3)),
                yield);

        otherSub = otherSubscriber->subscribe(
                Topic("other"),
                std::bind(&PubSubFixture::onOtherEvent, this, _1),
                yield);
    }

    void onDynamicEvent(Event event)
    {
        INFO( "in onDynamicEvent" );
        CHECK( event.pubId() <= 9007199254740992ll );
        CHECK( event.executor() == ioctx.get_executor() );
        dynamicArgs = event.args();
        dynamicPubs.push_back(event.pubId());
    }

    void onStaticEvent(Event event, std::string str, int num)
    {
        INFO( "in onStaticEvent" );
        CHECK( event.pubId() <= 9007199254740992ll );
        CHECK( event.executor() == ioctx.get_executor() );
        staticArgs = Array{{str, num}};
        staticPubs.push_back(event.pubId());
    }

    void onOtherEvent(Event event)
    {
        INFO( "in onOtherEvent" );
        CHECK( event.pubId() <= 9007199254740992ll );
        CHECK( event.executor() == ioctx.get_executor() );
        otherPubs.push_back(event.pubId());
    }

    AsioContext& ioctx;

    CoroSession<>::Ptr publisher;
    CoroSession<>::Ptr subscriber;
    CoroSession<>::Ptr otherSubscriber;

    ScopedSubscription dynamicSub;
    ScopedSubscription staticSub;
    ScopedSubscription otherSub;

    PubVec dynamicPubs;
    PubVec staticPubs;
    PubVec otherPubs;

    Array dynamicArgs;
    Array staticArgs;
};

//------------------------------------------------------------------------------
struct RpcFixture
{
    template <typename TConnector>
    RpcFixture(AsioContext& ioctx, TConnector cnct)
        : ioctx(ioctx),
          caller(CoroSession<>::create(ioctx, cnct)),
          callee(CoroSession<>::create(ioctx, cnct))
    {}

    void join(boost::asio::yield_context yield)
    {
        caller->connect(yield);
        caller->join(Realm(testRealm), yield);
        callee->connect(yield);
        callee->join(Realm(testRealm), yield);
    }

    void enroll(boost::asio::yield_context yield)
    {
        using namespace std::placeholders;
        dynamicReg = callee->enroll(
                Procedure("dynamic"),
                std::bind(&RpcFixture::dynamicRpc, this, _1),
                yield);

        staticReg = callee->enroll(
                Procedure("static"),
                unpackedRpc<std::string, int>(std::bind(&RpcFixture::staticRpc,
                                                        this, _1, _2, _3)),
                yield);
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

    AsioContext& ioctx;

    CoroSession<>::Ptr caller;
    CoroSession<>::Ptr callee;

    ScopedRegistration dynamicReg;
    ScopedRegistration staticReg;

    int dynamicCount = 0;
    int staticCount = 0;
};

//------------------------------------------------------------------------------
template <typename TThrowDelegate, typename TErrcDelegate>
void checkInvalidUri(TThrowDelegate&& throwDelegate,
                     TErrcDelegate&& errcDelegate, bool joined = true)
{
    AsioContext ioctx;
    boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
    {
        auto session = CoroSession<>::create(ioctx, tcp(ioctx));
        session->connect(yield);
        if (joined)
            session->join(Realm(testRealm), yield);
        CHECK_THROWS_AS( throwDelegate(*session, yield), error::Failure );
        session->disconnect();

        session->connect(yield);
        if (joined)
            session->join(Realm(testRealm), yield);
        std::error_code ec;
        errcDelegate(*session, yield, ec);
        CHECK( ec );
        if (session->state() == SessionState::established)
            CHECK( ec == SessionErrc::invalidUri );
    });

    ioctx.run();
}

//------------------------------------------------------------------------------
template <typename TResult, typename TDelegate>
void checkDisconnect(TDelegate&& delegate)
{
    AsioContext ioctx;
    bool completed = false;
    AsyncResult<TResult> result;
    boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
    {
        auto session = CoroSession<>::create(ioctx, tcp(ioctx));
        session->connect(yield);
        delegate(*session, yield, completed, result);
        session->disconnect();
        CHECK( session->state() == SessionState::disconnected );
    });

    ioctx.run();
    CHECK( completed );
    CHECK_FALSE( !result.errorCode() );
    CHECK( result.errorCode() == SessionErrc::sessionEnded );
    CHECK_THROWS_AS( result.get(), error::Failure );
}

//------------------------------------------------------------------------------
void checkInvalidConnect(CoroSession<>::Ptr session,
                         boost::asio::yield_context yield)
{
    std::error_code ec;

    CHECK_THROWS_AS( session->connect([](AsyncResult<size_t>){}), error::Logic );
    CHECK_THROWS_AS( session->connect(yield), error::Logic );
    CHECK_THROWS_AS( session->connect(yield, &ec), error::Logic );
}

void checkInvalidJoin(CoroSession<>::Ptr session,
                      boost::asio::yield_context yield)
{
    std::error_code ec;

    CHECK_THROWS_AS( session->join(Realm(testRealm),
                                   [](AsyncResult<SessionInfo>){}),
                     error::Logic );
    CHECK_THROWS_AS( session->join(Realm(testRealm), yield), error::Logic );
    CHECK_THROWS_AS( session->join(Realm(testRealm), yield, &ec), error::Logic );
}

void checkInvalidAuthenticate(CoroSession<>::Ptr session,
                              boost::asio::yield_context yield)
{
    std::error_code ec;

    CHECK_THROWS_AS( session->authenticate(Authentication("signature")),
                     error::Logic );
}

void checkInvalidLeave(CoroSession<>::Ptr session,
                       boost::asio::yield_context yield)
{
    std::error_code ec;

    CHECK_THROWS_AS( session->leave([](AsyncResult<Reason>){}),
                    error::Logic );
    CHECK_THROWS_AS( session->leave(yield), error::Logic );
    CHECK_THROWS_AS( session->leave(yield, &ec), error::Logic );

    CHECK_THROWS_AS( session->leave(Reason(), [](AsyncResult<Reason>){}),
                     error::Logic );
    CHECK_THROWS_AS( session->leave(Reason(), yield), error::Logic );
    CHECK_THROWS_AS( session->leave(Reason(), yield, &ec), error::Logic );
}

void checkInvalidOps(CoroSession<>::Ptr session,
                     boost::asio::yield_context yield)
{
    std::error_code ec;

    CHECK_THROWS_AS( session->subscribe(Topic("topic"),
                        [](Event){}, [](AsyncResult<Subscription>){}),
                     error::Logic );
    CHECK_THROWS_AS( session->publish(Pub("topic"),
                                     [](AsyncResult<PublicationId>) {}),
                     error::Logic );
    CHECK_THROWS_AS( session->publish(Pub("topic").withArgs(42),
                                     [](AsyncResult<PublicationId>) {}),
                     error::Logic );
    CHECK_THROWS_AS( session->enroll(Procedure("rpc"),
                                     [](Invocation)->Outcome {return {};},
                                     [](AsyncResult<Registration>){}),
                     error::Logic );
    CHECK_THROWS_AS( session->call(Rpc("rpc"), [](AsyncResult<Result>) {}),
                     error::Logic );
    CHECK_THROWS_AS( session->call(Rpc("rpc").withArgs(42),
                                   [](AsyncResult<Result>) {}),
                     error::Logic );

    CHECK_THROWS_AS( session->leave(Reason(), yield), error::Logic );
    CHECK_THROWS_AS( session->subscribe(Topic("topic"), [](Event){}, yield),
                     error::Logic );
    CHECK_THROWS_AS( session->publish(Pub("topic"), yield), error::Logic );
    CHECK_THROWS_AS( session->publish(Pub("topic").withArgs(42), yield),
                     error::Logic );
    CHECK_THROWS_AS( session->enroll(Procedure("rpc"),
                                     [](Invocation)->Outcome{return {};},
                     yield),
                     error::Logic );
    CHECK_THROWS_AS( session->call(Rpc("rpc"), yield), error::Logic );
    CHECK_THROWS_AS( session->call(Rpc("rpc").withArgs(42), yield),
                     error::Logic );

    CHECK_THROWS_AS( session->leave(Reason(), yield, &ec), error::Logic );
    CHECK_THROWS_AS( session->subscribe(Topic("topic"), [](Event){}, yield, &ec),
                     error::Logic );
    CHECK_THROWS_AS( session->publish(Pub("topic"), yield, &ec),
                     error::Logic );
    CHECK_THROWS_AS( session->publish(Pub("topic").withArgs(42), yield, &ec),
                     error::Logic );
    CHECK_THROWS_AS( session->enroll(Procedure("rpc"),
                                     [](Invocation)->Outcome{return {};},
                                    yield, &ec),
                     error::Logic );
    CHECK_THROWS_AS( session->call(Rpc("rpc"), yield, &ec), error::Logic );
    CHECK_THROWS_AS( session->call(Rpc("rpc").withArgs(42), yield, &ec),
                     error::Logic );
}

} // anonymous namespace


//------------------------------------------------------------------------------
SCENARIO( "WAMP session management", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a TCP connector" )
{
    AsioContext ioctx;
    auto cnct = tcp(ioctx);

    WHEN( "connecting and disconnecting" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            {
                // Connect and disconnect a session->
                auto s = CoroSession<>::create(ioctx, cnct);
                CHECK( s->state() == SessionState::disconnected );
                CHECK( s->connect(yield) == 0 );
                CHECK( s->state() == SessionState::closed );
                CHECK_NOTHROW( s->disconnect() );
                CHECK( s->state() == SessionState::disconnected );

                // Disconnecting again should be harmless
                CHECK_NOTHROW( s->disconnect() );
                CHECK( s->state() == SessionState::disconnected );

                // Check that we can reconnect.
                CHECK( s->connect(yield) == 0 );
                CHECK( s->state() == SessionState::closed );

                // Reset by letting session instance go out of scope.
            }

            // Check that another client can connect and disconnect.
            auto s2 = CoroSession<>::create(ioctx, cnct);
            CHECK( s2->state() == SessionState::disconnected );
            CHECK( s2->connect(yield) == 0 );
            CHECK( s2->state() == SessionState::closed );
            CHECK_NOTHROW( s2->disconnect() );
            CHECK( s2->state() == SessionState::disconnected );
        });

        ioctx.run();
    }

    WHEN( "joining and leaving" )
    {
        auto s = CoroSession<>::create(ioctx, cnct);
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            s->connect(yield);
            CHECK( s->state() == SessionState::closed );

            {
                // Check joining.
                SessionInfo info = s->join(Realm(testRealm), yield);
                CHECK( s->state() == SessionState::established );
                CHECK ( info.id() <= 9007199254740992ll );
                CHECK( info.realm()  == testRealm );
                Object details = info.options();
                REQUIRE( details.count("roles") );
                REQUIRE( details["roles"].is<Object>() );
                Object roles = info.optionByKey("roles").as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );
                CHECK( info.supportsRoles({"broker", "dealer"}) );

                // Check leaving.
                Reason reason = s->leave(yield);
                CHECK_FALSE( reason.uri().empty() );
                CHECK( s->state() == SessionState::closed );
            }

            {
                // Check that the same client can rejoin and leave.
                SessionInfo info = s->join(Realm(testRealm), yield);
                CHECK( s->state() == SessionState::established );
                CHECK ( info.id() <= 9007199254740992ll );
                CHECK( info.realm()  == testRealm );
                Object details = info.options();
                REQUIRE( details.count("roles") );
                REQUIRE( details["roles"].is<Object>() );
                Object roles = info.optionByKey("roles").as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );
                CHECK( info.supportsRoles({"broker", "dealer"}) );

                // Try leaving with a reason URI this time.
                Reason reason = s->leave(Reason("wamp.error.system_shutdown"),
                                         yield);
                CHECK_FALSE( reason.uri().empty() );
                CHECK( s->state() == SessionState::closed );
            }

            CHECK_NOTHROW( s->disconnect() );
            CHECK( s->state() == SessionState::disconnected );
        });

        ioctx.run();
    }

    WHEN( "connecting, joining, leaving, and disconnecting" )
    {
        auto s = CoroSession<>::create(ioctx, cnct);
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            {
                // Connect
                CHECK( s->state() == SessionState::disconnected );
                CHECK( s->connect(yield) == 0 );
                CHECK( s->state() == SessionState::closed );

                // Join
                s->join(Realm(testRealm), yield);
                CHECK( s->state() == SessionState::established );

                // Leave
                Reason reason = s->leave(yield);
                CHECK_FALSE( reason.uri().empty() );
                CHECK( s->state() == SessionState::closed );

                // Disconnect
                CHECK_NOTHROW( s->disconnect() );
                CHECK( s->state() == SessionState::disconnected );
            }

            {
                // Connect
                CHECK( s->connect(yield) == 0 );
                CHECK( s->state() == SessionState::closed );

                // Join
                SessionInfo info = s->join(Realm(testRealm), yield);
                CHECK( s->state() == SessionState::established );
                CHECK ( info.id() <= 9007199254740992ll );
                CHECK( info.realm()  == testRealm );
                Object details = info.options();
                REQUIRE( details.count("roles") );
                REQUIRE( details["roles"].is<Object>() );
                Object roles = info.optionByKey("roles").as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );
                CHECK( info.supportsRoles({"broker", "dealer"}) );

                // Leave
                Reason reason = s->leave(yield);
                CHECK_FALSE( reason.uri().empty() );
                CHECK( s->state() == SessionState::closed );

                // Disconnect
                CHECK_NOTHROW( s->disconnect() );
                CHECK( s->state() == SessionState::disconnected );
            }
        });

        ioctx.run();
    }

    WHEN( "disconnecting during connect" )
    {
        std::error_code ec;
        auto s = Session::create(ioctx,
                                 ConnectorList({invalidTcp(ioctx), cnct}));
        bool connectHandlerInvoked = false;
        s->connect([&](AsyncResult<size_t> result)
        {
            connectHandlerInvoked = true;
            ec = result.errorCode();
        });
        s->disconnect();

        ioctx.run();
        ioctx.reset();
        CHECK( connectHandlerInvoked );

        // Depending on how Asio schedules things, the connect operation
        // sometimes completes successfully before the cancellation request
        // can go through.
        if (ec)
        {
            CHECK( ec == TransportErrc::aborted );

            // Check that we can reconnect.
            s->reset();
            ec.clear();
            bool connected = false;
            s->connect([&](AsyncResult<size_t> result)
            {
                ec = result.errorCode();
                connected = !ec;
            });

            ioctx.run();
            CHECK( ec == TransportErrc::success );
            CHECK( connected );
        }
    }

    WHEN( "disconnecting during coroutine join" )
    {
        std::error_code ec;
        bool connected = false;
        auto s = CoroSession<>::create(ioctx, cnct);
        bool disconnectTriggered = false;
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            try
            {
                s->connect(yield);
                disconnectTriggered = true;
                s->join(Realm(testRealm), yield);
                connected = true;
            }
            catch (const error::Failure& e)
            {
                ec = e.code();
            }
        });

        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            while (!disconnectTriggered)
                ioctx.post(yield);
            s->disconnect();
        });

        ioctx.run();
        ioctx.reset();
        CHECK_FALSE( connected );
        CHECK( ec == SessionErrc::sessionEnded );
    }

    WHEN( "resetting during connect" )
    {
        bool handlerWasInvoked = false;
        auto s = Session::create(ioctx, cnct);
        s->connect([&handlerWasInvoked](AsyncResult<size_t>)
        {
            handlerWasInvoked = true;
        });
        s->reset();
        ioctx.run();

        CHECK_FALSE( handlerWasInvoked );
    }

    WHEN( "resetting during join" )
    {
        bool handlerWasInvoked = false;
        auto s = Session::create(ioctx, cnct);
        s->connect([&](AsyncResult<size_t>)
        {
            s->join(Realm(testRealm), [&](AsyncResult<SessionInfo>)
            {
                handlerWasInvoked = true;
            });
            s->reset();
        });
        ioctx.run();

        CHECK_FALSE( handlerWasInvoked );
    }

    WHEN( "session goes out of scope during connect" )
    {
        bool handlerWasInvoked = false;

        auto session = Session::create(ioctx, cnct);
        std::weak_ptr<Session> weakClient(session);

        session->connect([&handlerWasInvoked](AsyncResult<size_t>)
        {
            handlerWasInvoked = true;
        });

        // Reduce session reference count to zero
        session = nullptr;
        REQUIRE( weakClient.expired() );

        ioctx.run();

        CHECK_FALSE( handlerWasInvoked );
    }
}}


#ifdef CPPWAMP_TEST_HAS_ALTERNATE_CONNECTOR
//------------------------------------------------------------------------------
SCENARIO( "Using alternate transport and/or serializer", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a TCP connector" )
{
    AsioContext ioctx;
    auto cnct = alternateTcp(ioctx);

    WHEN( "joining and leaving" )
    {
        auto s = CoroSession<>::create(ioctx, cnct);
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            s->connect(yield);
            CHECK( s->state() == SessionState::closed );

            {
                // Check joining.
                SessionInfo info = s->join(Realm(testRealm), yield);
                CHECK( s->state() == SessionState::established );
                CHECK ( info.id() <= 9007199254740992ll );
                CHECK( info.realm()  == testRealm );
                Object details = info.options();
                REQUIRE( details.count("roles") );
                REQUIRE( details["roles"].is<Object>() );
                Object roles = info.optionByKey("roles").as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );
                CHECK( info.supportsRoles({"broker", "dealer"}) );

                // Check leaving.
                Reason reason = s->leave(yield);
                CHECK_FALSE( reason.uri().empty() );
                CHECK( s->state() == SessionState::closed );
            }

            {
                // Check that the same client can rejoin and leave.
                SessionInfo info = s->join(Realm(testRealm), yield);
                CHECK( s->state() == SessionState::established );
                CHECK ( info.id() <= 9007199254740992ll );
                CHECK( info.realm()  == testRealm );
                Object details = info.options();
                REQUIRE( details.count("roles") );
                REQUIRE( details["roles"].is<Object>() );
                Object roles = info.optionByKey("roles").as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );
                CHECK( info.supportsRoles({"broker", "dealer"}) );

                // Try leaving with a reason URI this time.
                Reason reason = s->leave(Reason("wamp.error.system_shutdown"),
                                         yield);
                CHECK_FALSE( reason.uri().empty() );
                CHECK( s->state() == SessionState::closed );
            }

            CHECK_NOTHROW( s->disconnect() );
            CHECK( s->state() == SessionState::disconnected );
        });

        ioctx.run();
    }
}}
#endif // CPPWAMP_TEST_HAS_ALTERNATE_TRANSPORT


//------------------------------------------------------------------------------
SCENARIO( "WAMP Pub-Sub", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a TCP connector" )
{
    AsioContext ioctx;
    auto cnct = tcp(ioctx);

    WHEN( "publishing and subscribing" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(ioctx, cnct);
            f.join(yield);
            f.subscribe(yield);

            // Check dynamic and static subscriptions.
            f.publisher->publish(Pub("str.num").withArgs("one", 1));
            pid = f.publisher->publish(Pub("str.num").withArgs("two", 2),
                                       yield);
            while (f.dynamicPubs.size() < 2)
                f.subscriber->suspend(yield);

            REQUIRE( f.dynamicPubs.size() == 2 );
            CHECK( f.dynamicPubs.back() == pid );
            CHECK(( f.dynamicArgs == Array{"two", 2} ));
            REQUIRE( f.staticPubs.size() == 2 );
            CHECK( f.staticPubs.back() == pid );
            CHECK(( f.staticArgs == Array{"two", 2} ));
            CHECK( f.otherPubs.empty() );

            // Check subscription from another client.
            f.publisher->publish(Pub("other"));
            pid = f.publisher->publish(Pub("other"), yield);
            while (f.otherPubs.size() < 2)
                f.otherSubscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 2 );
            CHECK( f.staticPubs.size() == 2 );
            REQUIRE( f.otherPubs.size() == 2 );
            CHECK( f.otherPubs.back() == pid );

            // Unsubscribe the dynamic subscription manually.
            f.subscriber->unsubscribe(f.dynamicSub, yield);

            // Check that the dynamic slot no longer fires, and that the
            // static slot still fires.
            pid = f.publisher->publish(Pub("str.num").withArgs("three", 3),
                                       yield);
            while (f.staticPubs.size() < 3)
                f.otherSubscriber->suspend(yield);
            REQUIRE( f.dynamicPubs.size() == 2 );
            REQUIRE( f.staticPubs.size() == 3 );
            CHECK( f.staticPubs.back() == pid );
            CHECK(( f.staticArgs == Array{"three", 3} ));

            // Unsubscribe the static subscription via RAII.
            f.staticSub = ScopedSubscription();

            // Check that the dynamic and static slots no longer fire, and
            // that the "other" slot still fires.
            f.publisher->publish(Pub("str.num").withArgs("four", 4), yield);
            pid = f.publisher->publish(Pub("other"), yield);
            while (f.otherPubs.size() < 3)
                f.subscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 2 );
            CHECK( f.staticPubs.size() == 3 );
            REQUIRE( f.otherPubs.size() == 3 );
            CHECK( f.otherPubs.back() == pid );

            // Make the "other" subscriber leave and rejoin the realm.
            f.otherSubscriber->leave(yield);
            f.otherSubscriber->join(Realm(testRealm), yield);

            // Reestablish the dynamic subscription.
            using namespace std::placeholders;
            f.dynamicSub = f.subscriber->subscribe(
                    Topic("str.num"),
                    std::bind(&PubSubFixture::onDynamicEvent, &f, _1),
                    yield);

            // Check that only the dynamic slot still fires.
            f.publisher->publish(Pub("other"), yield);
            pid = f.publisher->publish(Pub("str.num").withArgs("five", 5),
                                       yield);
            while (f.dynamicPubs.size() < 3)
                f.subscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 3 );
            CHECK( f.staticPubs.size() == 3 );
            REQUIRE( f.otherPubs.size() == 3 );
            CHECK( f.dynamicPubs.back() == pid );
            CHECK(( f.dynamicArgs == Array{"five", 5} ));
        });

        ioctx.run();
    }

    WHEN( "subscribing basic events" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            PubSubFixture f(ioctx, cnct);
            f.join(yield);
            f.staticSub = f.subscriber->subscribe(
                Topic("str.num"),
                basicEvent<std::string, int>([&](std::string s, int n)
                {
                    f.staticArgs = Array{{s, n}};
                }),
                yield);

            f.publisher->publish(Pub("str.num").withArgs("one", 1));

            while (f.staticArgs.size() < 2)
                f.subscriber->suspend(yield);
            CHECK(( f.staticArgs == Array{"one", 1} ));
        });
        ioctx.run();
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "WAMP Subscription Lifetimes", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a TCP connector" )
{
    AsioContext ioctx;
    auto cnct = tcp(ioctx);

    WHEN( "unsubscribing multiple times" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(ioctx, cnct);
            f.join(yield);
            f.subscribe(yield);

            // Unsubscribe the dynamic subscription manually.
            f.dynamicSub.unsubscribe();

            // Unsubscribe the dynamic subscription again via RAII.
            f.dynamicSub = ScopedSubscription();

            // Check that the dynamic slot no longer fires, and that the
            // static slot still fires.
            pid = f.publisher->publish(Pub("str.num").withArgs("foo", 42),
                                       yield);
            while (f.staticPubs.size() < 1)
                f.subscriber->suspend(yield);
            REQUIRE( f.dynamicPubs.size() == 0 );
            REQUIRE( f.staticPubs.size() == 1 );
            CHECK( f.staticPubs.back() == pid );

            // Unsubscribe the static subscription manually.
            f.subscriber->unsubscribe(f.staticSub, yield);

            // Unsubscribe the static subscription again manually.
            f.staticSub.unsubscribe();

            // Check that the dynamic and static slots no longer fire.
            // Publish to the "other" subscription so that we know when
            // to stop polling.
            f.publisher->publish(Pub("str.num").withArgs("foo", 42), yield);
            pid = f.publisher->publish(Pub("other"), yield);
            while (f.otherPubs.size() < 1)
                f.otherSubscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 0 );
            CHECK( f.staticPubs.size() == 1 );
            REQUIRE( f.otherPubs.size() == 1 );
            CHECK( f.otherPubs.back() == pid );
        });

        ioctx.run();
    }

    WHEN( "unsubscribing after session is destroyed" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(ioctx, cnct);
            f.join(yield);
            f.subscribe(yield);

            // Destroy the subscriber session
            f.subscriber.reset();

            // Unsubscribe the dynamic subscription manually.
            REQUIRE_NOTHROW( f.dynamicSub.unsubscribe() );

            // Unsubscribe the static subscription via RAII.
            REQUIRE_NOTHROW( f.staticSub = ScopedSubscription() );

            // Check that the dynamic and static slots no longer fire.
            // Publish to the "other" subscription so that we know when
            // to stop polling.
            f.publisher->publish(Pub("str.num").withArgs("foo", 42), yield);
            pid = f.publisher->publish(Pub("other"), yield);
            while (f.otherPubs.size() < 1)
                f.otherSubscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 0 );
            CHECK( f.staticPubs.size() == 0 );
            REQUIRE( f.otherPubs.size() == 1 );
            CHECK( f.otherPubs.back() == pid );
        });

        ioctx.run();
    }

    WHEN( "unsubscribing after leaving" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(ioctx, cnct);
            f.join(yield);
            f.subscribe(yield);

            // Make the subscriber client leave the session.
            f.subscriber->leave(yield);

            // Unsubscribe the dynamic subscription via RAII.
            REQUIRE_NOTHROW( f.dynamicSub = ScopedSubscription() );

            // Unsubscribe the static subscription manually.
            CHECK_THROWS_AS( f.subscriber->unsubscribe(f.staticSub, yield),
                             error::Logic );
            REQUIRE_NOTHROW( f.staticSub.unsubscribe() );

            // Check that the dynamic and static slots no longer fire.
            // Publish to the "other" subscription so that we know when
            // to stop polling.
            f.publisher->publish(Pub("str.num").withArgs("foo", 42), yield);
            pid = f.publisher->publish(Pub("other"), yield);
            while (f.otherPubs.size() < 1)
                f.otherSubscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 0 );
            CHECK( f.staticPubs.size() == 0 );
            REQUIRE( f.otherPubs.size() == 1 );
            CHECK( f.otherPubs.back() == pid );
        });

        ioctx.run();
    }

    WHEN( "unsubscribing after disconnecting" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(ioctx, cnct);
            f.join(yield);
            f.subscribe(yield);

            // Make the subscriber client disconnect.
            f.subscriber->disconnect();

            // Unsubscribe the dynamic subscription manually.
            CHECK_THROWS_AS( f.subscriber->unsubscribe(f.dynamicSub, yield),
                             error::Logic );
            REQUIRE_NOTHROW( f.dynamicSub.unsubscribe() );

            // Unsubscribe the static subscription via RAII.
            REQUIRE_NOTHROW( f.staticSub= ScopedSubscription() );

            // Check that the dynamic and static slots no longer fire.
            // Publish to the "other" subscription so that we know when
            // to stop polling.
            f.publisher->publish(Pub("str.num").withArgs("foo", 42), yield);
            pid = f.publisher->publish(Pub("other"), yield);
            while (f.otherPubs.size() < 1)
                f.otherSubscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 0 );
            CHECK( f.staticPubs.size() == 0 );
            REQUIRE( f.otherPubs.size() == 1 );
            CHECK( f.otherPubs.back() == pid );
        });

        ioctx.run();
    }

    WHEN( "unsubscribing after reset" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(ioctx, cnct);
            f.join(yield);
            f.subscribe(yield);

            // Destroy the subscriber
            f.subscriber.reset();

            // Unsubscribe the static subscription via RAII.
            REQUIRE_NOTHROW( f.staticSub = ScopedSubscription() );

            // Check that the dynamic and static slots no longer fire.
            // Publish to the "other" subscription so that we know when
            // to stop polling.
            f.publisher->publish(Pub("str.num").withArgs("foo", 42), yield);
            pid = f.publisher->publish(Pub("other"), yield);
            while (f.otherPubs.size() < 1)
                f.otherSubscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 0 );
            CHECK( f.staticPubs.size() == 0 );
            REQUIRE( f.otherPubs.size() == 1 );
            CHECK( f.otherPubs.back() == pid );
        });

        ioctx.run();
    }

    WHEN( "moving a ScopedSubscription" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            PubSubFixture f(ioctx, cnct);
            f.join(yield);
            f.subscribe(yield);

            // Check move construction.
            {
                ScopedSubscription sub(std::move(f.dynamicSub));
                CHECK( !!sub );
                CHECK( sub.id() >= 0 );
                CHECK( !f.dynamicSub );

                f.publisher->publish(Pub("str.num").withArgs("", 0), yield);
                while (f.dynamicPubs.size() < 1)
                    f.subscriber->suspend(yield);
                CHECK( f.dynamicPubs.size() == 1 );
                CHECK( f.staticPubs.size() == 1 );
            }
            // 'sub' goes out of scope here.
            f.publisher->publish(Pub("str.num").withArgs("", 0), yield);
            f.publisher->publish(Pub("other"), yield);
            while (f.otherPubs.size() < 1)
                f.subscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 1 );
            CHECK( f.staticPubs.size() == 2 );
            CHECK( f.otherPubs.size() == 1 );

            // Check move assignment.
            {
                ScopedSubscription sub;
                sub = std::move(f.staticSub);
                CHECK( !!sub );
                CHECK( sub.id() >= 0 );
                CHECK( !f.staticSub );

                f.publisher->publish(Pub("str.num").withArgs("", 0), yield);
                while (f.staticPubs.size() < 3)
                    f.subscriber->suspend(yield);
                CHECK( f.staticPubs.size() == 3 );
            }
            // 'sub' goes out of scope here.
            f.publisher->publish(Pub("str.num").withArgs("", 0), yield);
            f.publisher->publish(Pub("other"), yield);
            while (f.otherPubs.size() < 2)
                f.subscriber->suspend(yield);
            CHECK( f.staticPubs.size() == 3 ); // staticPubs count the same
            CHECK( f.otherPubs.size() == 2 );
        });
        ioctx.run();
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "WAMP RPCs", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a TCP connector" )
{
    AsioContext ioctx;
    auto cnct = tcp(ioctx);

    WHEN( "calling remote procedures taking dynamically-typed args" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            Result result;
            std::error_code ec;
            RpcFixture f(ioctx, cnct);
            f.join(yield);
            f.enroll(yield);

            // Check normal RPC
            Error error;
            result = f.caller->call(Rpc("dynamic").withArgs("one", 1)
                                    .captureError(error), yield);
            CHECK( !error );
            CHECK( error.reason().empty() );
            CHECK( f.dynamicCount == 1 );
            CHECK(( result.args() == Array{"one", 1} ));
            result = f.caller->call(Rpc("dynamic").withArgs("two", 2),
                                    yield);
            CHECK( f.dynamicCount == 2 );
            CHECK(( result.args() == Array{"two", 2} ));

            // Manually unregister the slot.
            f.callee->unregister(f.dynamicReg, yield);

            // The router should now report an error when attempting
            // to call the unregistered RPC.
            CHECK_THROWS_AS( f.caller->call(
                                 Rpc("dynamic").withArgs("three", 3),
                                 yield),
                             error::Failure);
            f.caller->call(Rpc("dynamic").withArgs("three", 3), yield, &ec);
            CHECK( ec == SessionErrc::callError );
            CHECK( ec == SessionErrc::noSuchProcedure );

            // Calling should work after re-registering the slot.
            using namespace std::placeholders;
            f.dynamicReg = f.callee->enroll(
                Procedure("dynamic"),
                std::bind(&RpcFixture::dynamicRpc, &f, _1),
                yield);
            result = f.caller->call(Rpc("dynamic").withArgs("four", 4),
                                    yield);
            CHECK( f.dynamicCount == 3 );
            CHECK(( result.args() == Array{"four", 4} ));
        });
        ioctx.run();
    }

    WHEN( "calling remote procedures taking statically-typed args" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            Result result;
            std::error_code ec;
            RpcFixture f(ioctx, cnct);
            f.join(yield);
            f.enroll(yield);

            // Check normal RPC
            result = f.caller->call(Rpc("static").withArgs("one", 1),
                                    yield);
            CHECK( f.staticCount == 1 );
            CHECK(( result.args() == Array{"one", 1} ));

            // Extra arguments should be ignored.
            result = f.caller->call(Rpc("static").withArgs("two", 2, true),
                                    yield);
            CHECK( f.staticCount == 2 );
            CHECK(( result.args() == Array{"two", 2} ));

            // Unregister the slot via RAII.
            f.staticReg = ScopedRegistration();

            // The router should now report an error when attempting
            // to call the unregistered RPC.
            CHECK_THROWS_AS( f.caller->call(
                                 Rpc("static").withArgs("three", 3),
                                 yield),
                             error::Failure );
            f.caller->call(Rpc("static").withArgs("three", 3), yield, &ec);
            CHECK( ec == SessionErrc::callError );
            CHECK( ec == SessionErrc::noSuchProcedure );

            // Calling should work after re-registering the slot.
            using namespace std::placeholders;
            f.staticReg = f.callee->enroll(
                Procedure("static"),
                unpackedRpc<std::string, int>(std::bind(&RpcFixture::staticRpc,
                                                        &f, _1, _2, _3)),
                yield);
            result = f.caller->call(Rpc("static").withArgs("four", 4), yield);
            CHECK( f.staticCount == 3 );
            CHECK(( result.args() == Array{"four", 4} ));
        });
        ioctx.run();
    }

    WHEN( "calling basic remote procedures" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            Result result;
            std::error_code ec;
            RpcFixture f(ioctx, cnct);
            f.join(yield);

            f.staticReg = f.callee->enroll(
                    Procedure("static"),
                    basicRpc<int, std::string, int>([&](std::string, int n)
                    {
                        ++f.staticCount;
                        return n; // Echo back the integer argument
                    }),
                    yield);

            // Check normal RPC
            result = f.caller->call(Rpc("static").withArgs("one", 1),
                                    yield);
            CHECK( f.staticCount == 1 );
            CHECK(( result.args() == Array{1} ));

            // Extra arguments should be ignored.
            result = f.caller->call(Rpc("static").withArgs("two", 2, true),
                                    yield);
            CHECK( f.staticCount == 2 );
            CHECK(( result.args() == Array{2} ));

            // Unregister the slot via RAII.
            f.staticReg = ScopedRegistration();

            // The router should now report an error when attempting
            // to call the unregistered RPC.
            CHECK_THROWS_AS( f.caller->call(
                                 Rpc("static").withArgs("three", 3),
                                 yield),
                             error::Failure );
            f.caller->call(Rpc("static").withArgs("three", 3), yield, &ec);
            CHECK( ec == SessionErrc::callError );
            CHECK( ec == SessionErrc::noSuchProcedure );

            // Calling should work after re-registering the slot.
            using namespace std::placeholders;
            f.staticReg = f.callee->enroll(
                    Procedure("static"),
                    basicRpc<int, std::string, int>([&](std::string, int n)
                    {
                        ++f.staticCount;
                        return n; // Echo back the integer argument
                    }),
                    yield);
            result = f.caller->call(Rpc("static").withArgs("four", 4), yield);
            CHECK( f.staticCount == 3 );
            CHECK(( result.args() == Array{4} ));
        });
        ioctx.run();
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "WAMP Registation Lifetimes", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a TCP connector" )
{
    AsioContext ioctx;
    auto cnct = tcp(ioctx);

    WHEN( "unregistering after a session is destroyed" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            std::error_code ec;
            RpcFixture f(ioctx, cnct);
            f.join(yield);
            f.enroll(yield);

            // Destroy the callee session
            f.callee.reset();

            // Manually unregister a RPC.
            REQUIRE_NOTHROW( f.dynamicReg.unregister() );

            // Unregister an RPC via RAII.
            REQUIRE_NOTHROW( f.staticReg = ScopedRegistration() );

            // The router should report an error when attempting
            // to call the unregistered RPCs.
            CHECK_THROWS_AS( f.caller->call(Rpc("dynamic").withArgs("one", 1),
                                            yield),
                             error::Failure );
            f.caller->call(Rpc("dynamic").withArgs("one", 1), yield, &ec);
            CHECK( ec == SessionErrc::noSuchProcedure );

            CHECK_THROWS_AS( f.caller->call(Rpc("static").withArgs("two", 2),
                                            yield),
                             error::Failure );
            f.caller->call(Rpc("dynamic").withArgs("two", 2), yield, &ec);
            CHECK( ec == SessionErrc::noSuchProcedure );
        });
        ioctx.run();
    }

    WHEN( "unregistering after leaving" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            std::error_code ec;
            RpcFixture f(ioctx, cnct);
            f.join(yield);
            f.enroll(yield);

            // Make the callee leave the session.
            f.callee->leave(yield);

            // Manually unregister a RPC.
            CHECK_THROWS_AS( f.callee->unregister(f.dynamicReg, yield),
                             error::Logic );
            REQUIRE_NOTHROW( f.dynamicReg.unregister() );

            // Unregister an RPC via RAII.
            REQUIRE_NOTHROW( f.staticReg = ScopedRegistration() );

            // The router should report an error when attempting
            // to call the unregistered RPCs.
            CHECK_THROWS_AS( f.caller->call(Rpc("dynamic").withArgs("one", 1),
                                            yield),
                             error::Failure );
            f.caller->call(Rpc("dynamic").withArgs("one", 1), yield, &ec);
            CHECK( ec == SessionErrc::noSuchProcedure );

            CHECK_THROWS_AS( f.caller->call(Rpc("static").withArgs("two", 2),
                                            yield),
                             error::Failure );
            f.caller->call(Rpc("dynamic").withArgs("two", 2), yield, &ec);
            CHECK( ec == SessionErrc::noSuchProcedure );
        });
        ioctx.run();
    }

    WHEN( "unregistering after disconnecting" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            std::error_code ec;
            RpcFixture f(ioctx, cnct);
            f.join(yield);
            f.enroll(yield);

            // Make the callee disconnect.
            f.callee->disconnect();

            // Manually unregister a RPC.
            CHECK_THROWS_AS( f.callee->unregister(f.dynamicReg, yield),
                             error::Logic );
            REQUIRE_NOTHROW( f.dynamicReg.unregister() );

            // Unregister an RPC via RAII.
            REQUIRE_NOTHROW( f.staticReg = ScopedRegistration() );

            // The router should report an error when attempting
            // to call the unregistered RPCs.
            CHECK_THROWS_AS( f.caller->call(Rpc("dynamic").withArgs("one", 1),
                                            yield),
                             error::Failure );
            f.caller->call(Rpc("dynamic").withArgs("one", 1), yield, &ec);
            CHECK( ec == SessionErrc::noSuchProcedure );

            CHECK_THROWS_AS( f.caller->call(Rpc("static").withArgs("two", 2),
                                            yield),
                             error::Failure );
            f.caller->call(Rpc("dynamic").withArgs("two", 2), yield, &ec);
            CHECK( ec == SessionErrc::noSuchProcedure );
        });
        ioctx.run();
    }

    WHEN( "unregistering after reset" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            std::error_code ec;
            RpcFixture f(ioctx, cnct);
            f.join(yield);
            f.enroll(yield);

            // Destroy the callee.
            f.callee.reset();

            // Unregister an RPC via RAII.
            REQUIRE_NOTHROW( f.staticReg = ScopedRegistration() );

            // The router should report an error when attempting
            // to call the unregistered RPCs.
            CHECK_THROWS_AS( f.caller->call(Rpc("dynamic").withArgs("one", 1),
                                            yield),
                             error::Failure );
            f.caller->call(Rpc("dynamic").withArgs("one", 1), yield, &ec);
            CHECK( ec == SessionErrc::noSuchProcedure );

            CHECK_THROWS_AS( f.caller->call(Rpc("static").withArgs("two", 2),
                                            yield),
                             error::Failure );
            f.caller->call(Rpc("dynamic").withArgs("two", 2), yield, &ec);
            CHECK( ec == SessionErrc::noSuchProcedure );
        });
        ioctx.run();
    }
    WHEN( "moving a ScopedRegistration" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            RpcFixture f(ioctx, cnct);
            f.join(yield);
            f.enroll(yield);

            // Check move construction.
            {
                ScopedRegistration reg(std::move(f.dynamicReg));
                CHECK( !!reg );
                CHECK( reg.id() >= 0 );
                CHECK( !f.dynamicReg );

                f.caller->call(Rpc("dynamic"), yield);
                CHECK( f.dynamicCount == 1 );
            }
            // 'reg' goes out of scope here.
            CHECK_THROWS( f.caller->call(Rpc("dynamic"), yield) );
            CHECK( f.dynamicCount == 1 );

            // Check move assignment.
            {
                ScopedRegistration reg;
                reg = std::move(f.staticReg);
                CHECK( !!reg );
                CHECK( reg.id() >= 0 );
                CHECK( !f.staticReg );

                f.caller->call(Rpc("static").withArgs("", 0), yield);
                CHECK( f.staticCount == 1 );
            }
            // 'reg' goes out of scope here.
            CHECK_THROWS( f.caller->call(Rpc("static").withArgs("", 0),
                                         yield) );
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
    using Yield = boost::asio::yield_context;

    AsioContext ioctx;
    auto cnct = tcp(ioctx);
    auto session1 = CoroSession<>::create(ioctx, cnct);
    auto session2 = CoroSession<>::create(ioctx, cnct);

    // Regular RPC handler
    auto upperify = [](Invocation, std::string str) -> Outcome
    {
        std::transform(str.begin(), str.end(), str.begin(),
                       ::toupper);
        return {str};
    };

    WHEN( "calling remote procedures within an invocation" )
    {
        auto uppercat = [session2](std::string str1, std::string str2,
                boost::asio::yield_context yield) -> String
        {
            auto upper1 = session2->call(
                    Rpc("upperify").withArgs(str1), yield);
            auto upper2 = session2->call(
                    Rpc("upperify").withArgs(str2), yield);
            return upper1[0].to<std::string>() + upper2[0].to<std::string>();
        };

        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            session1->connect(yield);
            session1->join(Realm(testRealm), yield);
            session1->enroll(Procedure("upperify"),
                             unpackedRpc<std::string>(upperify), yield);


            session2->connect(yield);
            session2->join(Realm(testRealm), yield);
            session2->enroll(
                Procedure("uppercat"),
                basicCoroRpc<std::string, std::string, std::string>(uppercat),
                yield);

            std::string s1 = "hello ";
            std::string s2 = "world";
            auto result = session1->call(Rpc("uppercat").withArgs(s1, s2),
                                         yield);
            CHECK( result[0] == "HELLO WORLD" );
            session1->disconnect();
            session2->disconnect();
        });

        ioctx.run();
    }

    WHEN( "calling remote procedures within an event handler" )
    {
        auto callee = session1;
        auto subscriber = session2;

        std::string upperized;
        auto onEvent =
            [&upperized, subscriber](std::string str,
                                     boost::asio::yield_context yield)
            {
                auto result = subscriber->call(Rpc("upperify").withArgs(str),
                                               yield);
                upperized = result[0].to<std::string>();
            };

        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            callee->connect(yield);
            callee->join(Realm(testRealm), yield);
            callee->enroll(Procedure("upperify"),
                           unpackedRpc<std::string>(upperify), yield);

            subscriber->connect(yield);
            subscriber->join(Realm(testRealm), yield);
            subscriber->subscribe(Topic("onEvent"),
                                  basicCoroEvent<std::string>(onEvent),
                                  yield);

            callee->publish(Pub("onEvent").withArgs("Hello"), yield);
            while (upperized.empty())
                callee->suspend(yield);
            CHECK_THAT( upperized, Equals("HELLO") );
            callee->disconnect();
            subscriber->disconnect();
        });

        ioctx.run();
    }

    WHEN( "publishing within an invocation" )
    {
        auto callee = session1;
        auto subscriber = session2;

        std::string upperized;
        auto onEvent = [&upperized](Event, std::string str)
        {
            upperized = str;
        };

        auto shout =
            [callee](Invocation, std::string str, Yield yield) -> Outcome
            {
                std::string upper = str;
                std::transform(upper.begin(), upper.end(),
                               upper.begin(), ::toupper);
                callee->publish(Pub("grapevine").withArgs(upper), yield);
                return Result({upper});
            };

        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            callee->connect(yield);
            callee->join(Realm(testRealm), yield);
            callee->enroll(Procedure("shout"),
                           unpackedCoroRpc<std::string>(shout), yield);

            subscriber->connect(yield);
            subscriber->join(Realm(testRealm), yield);
            subscriber->subscribe(Topic("grapevine"),
                                  unpackedEvent<std::string>(onEvent),
                                  yield);

            subscriber->call(Rpc("shout").withArgs("hello"), yield);
            while (upperized.empty())
                subscriber->suspend(yield);
            CHECK_THAT( upperized, Equals("HELLO") );
            callee->disconnect();
            subscriber->disconnect();
        });

        ioctx.run();
    }

    WHEN( "unregistering within an invocation" )
    {
        auto callee = session1;
        auto caller = session2;

        int callCount = 0;
        Registration reg;

        auto oneShot = [&callCount, &reg, callee](Yield yield)
        {
            // We need a yield context here for a blocking unregister.
            ++callCount;
            callee->unregister(reg, yield);
        };

        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            callee->connect(yield);
            callee->join(Realm(testRealm), yield);
            reg = callee->enroll(Procedure("oneShot"),
                                 basicCoroRpc<void>(oneShot), yield);

            caller->connect(yield);
            caller->join(Realm(testRealm), yield);

            caller->call(Rpc("oneShot"), yield);
            while (callCount == 0)
                caller->suspend(yield);
            CHECK( callCount == 1 );

            std::error_code ec;
            caller->call(Rpc("oneShot"), yield, &ec);
            CHECK( ec == SessionErrc::noSuchProcedure );

            callee->disconnect();
            caller->disconnect();
        });

        ioctx.run();
    }

    WHEN( "publishing within an event" )
    {
        std::string upperized;

        auto onTalk = [session1](std::string str, Yield yield)
        {
            // We need a separate yield context here for a blocking
            // publish.
            std::string upper = str;
            std::transform(upper.begin(), upper.end(),
                           upper.begin(), ::toupper);
            session1->publish(Pub("onShout").withArgs(upper), yield);
        };

        auto onShout = [&upperized](Event, std::string str)
        {
            upperized = str;
        };

        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            session1->connect(yield);
            session1->join(Realm(testRealm), yield);
            session1->subscribe(Topic("onTalk"),
                                basicCoroEvent<std::string>(onTalk), yield);

            session2->connect(yield);
            session2->join(Realm(testRealm), yield);
            session2->subscribe(Topic("onShout"),
                                unpackedEvent<std::string>(onShout), yield);

            session2->publish(Pub("onTalk").withArgs("hello"), yield);
            while (upperized.empty())
                session2->suspend(yield);
            CHECK_THAT( upperized, Equals("HELLO") );
            session1->disconnect();
            session2->disconnect();
        });

        ioctx.run();
    }

    WHEN( "unsubscribing within an event" )
    {
        auto publisher = session1;
        auto subscriber = session2;

        int eventCount = 0;
        Subscription sub;

        auto onEvent = [&eventCount, &sub, subscriber](Event, Yield yield)
        {
            // We need a yield context here for a blocking unsubscribe.
            ++eventCount;
            subscriber->unsubscribe(sub, yield);
        };

        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            publisher->connect(yield);
            publisher->join(Realm(testRealm), yield);

            subscriber->connect(yield);
            subscriber->join(Realm(testRealm), yield);
            sub = subscriber->subscribe(Topic("onEvent"),
                                        unpackedCoroEvent(onEvent),
                                        yield);

            // Dummy RPC used to end polling
            int rpcCount = 0;
            subscriber->enroll(Procedure("dummy"),
                [&rpcCount](Invocation) -> Outcome
                {
                   ++rpcCount;
                   return {};
                },
                yield);

            publisher->publish(Pub("onEvent"), yield);
            while (eventCount == 0)
                publisher->suspend(yield);

            // This publish should not have any subscribers
            publisher->publish(Pub("onEvent"), yield);

            // Invoke dummy RPC so that we know when to stop
            publisher->call(Rpc("dummy"), yield);

            // The event count should still be one
            CHECK( eventCount == 1 );

            publisher->disconnect();
            subscriber->disconnect();
        });

        ioctx.run();
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "WAMP Connection Failures", "[WAMP][Basic]" )
{
GIVEN( "an IO service, a valid TCP connector, and an invalid connector" )
{
    AsioContext ioctx;
    auto cnct = tcp(ioctx);
    auto badCnct = invalidTcp(ioctx);

    WHEN( "connecting to an invalid port" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            auto session = CoroSession<>::create(ioctx, badCnct);
            bool throws = false;
            try
            {
                session->connect(yield);
            }
            catch (const error::Failure& e)
            {
                throws = true;
                CHECK( e.code() == TransportErrc::failed );
            }
            CHECK( throws );

            std::error_code ec;
            session->disconnect();
            session->connect(yield, &ec);
            CHECK( ec == TransportErrc::failed );
        });

        ioctx.run();
    }

    WHEN( "connecting with multiple transports" )
    {
        ConnectorList connectors = {badCnct, cnct};

        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            auto s = CoroSession<>::create(ioctx, connectors);

            {
                // Connect
                CHECK( s->state() == SessionState::disconnected );
                CHECK( s->connect(yield) == 1 );
                CHECK( s->state() == SessionState::closed );

                // Join
                SessionInfo info = s->join(Realm(testRealm), yield);
                CHECK( s->state() == SessionState::established );
                CHECK ( info.id() <= 9007199254740992ll );
                CHECK( info.realm()  == testRealm );
                Object details = info.options();
                REQUIRE( details.count("roles") );
                REQUIRE( details["roles"].is<Object>() );
                Object roles = info.optionByKey("roles").as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );
                CHECK( info.supportsRoles({"broker", "dealer"}) );

                // Disconnect
                CHECK_NOTHROW( s->disconnect() );
                CHECK( s->state() == SessionState::disconnected );
            }

            {
                // Connect
                CHECK( s->connect(yield) == 1 );
                CHECK( s->state() == SessionState::closed );

                // Join
                SessionInfo info = s->join(Realm(testRealm), yield);
                CHECK( s->state() == SessionState::established );
                CHECK ( info.id() <= 9007199254740992ll );
                CHECK( info.realm()  == testRealm );
                Object details = info.options();
                REQUIRE( details.count("roles") );
                REQUIRE( details["roles"].is<Object>() );
                Object roles = info.optionByKey("roles").as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );
                CHECK( info.supportsRoles({"broker", "dealer"}) );
            }
        });

        ioctx.run();
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "WAMP RPC Failures", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a TCP connector" )
{
    AsioContext ioctx;
    auto cnct = tcp(ioctx);

    WHEN( "registering an already existing procedure" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            RpcFixture f(ioctx, cnct);
            f.join(yield);
            f.enroll(yield);

            std::error_code ec;
            Registration reg;
            auto handler = [](Invocation)->Outcome {return {};};

            CHECK_THROWS_AS( f.callee->enroll(Procedure("dynamic"), handler,
                                              yield),
                             error::Failure );
            reg = f.callee->enroll(Procedure("dynamic"), handler, yield, &ec);
            CHECK( ec == SessionErrc::registerError );
            CHECK( ec == SessionErrc::procedureAlreadyExists );
            CHECK( reg.id() == -1 );
        });
        ioctx.run();
    }

    WHEN( "an RPC returns an error URI" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            std::error_code ec;
            int callCount = 0;
            RpcFixture f(ioctx, cnct);
            f.join(yield);
            f.enroll(yield);

            auto reg = f.callee->enroll(
                Procedure("rpc"),
                [&callCount](Invocation)->Outcome
                {
                    ++callCount;
                    return Error("wamp.error.not_authorized")
                           .withArgs(123)
                           .withKwargs(Object{{{"foo"},{"bar"}}});
                },
                yield);

            {
                Error error;
                CHECK_THROWS_AS( f.caller->call(Rpc("rpc").captureError(error),
                                                yield),
                                 error::Failure );
                CHECK( !!error );
                CHECK_THAT( error.reason(),
                            Equals("wamp.error.not_authorized") );
                CHECK( error.args() == Array{123} );
                CHECK( error.kwargs() == (Object{{{"foo"},{"bar"}}}) );
            }

            {
                Error error;
                f.caller->call(Rpc("rpc").captureError(error), yield, &ec);
                CHECK( ec == SessionErrc::notAuthorized );
                CHECK( !!error );
                CHECK_THAT( error.reason(),
                            Equals("wamp.error.not_authorized") );
                CHECK( error.args() == Array{123} );
                CHECK( error.kwargs() == (Object{{{"foo"},{"bar"}}}) );
            }

            CHECK( callCount == 2 );
        });
        ioctx.run();
    }

    WHEN( "an RPC throws an error URI" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            std::error_code ec;
            int callCount = 0;
            RpcFixture f(ioctx, cnct);
            f.join(yield);
            f.enroll(yield);

            auto reg = f.callee->enroll(
                Procedure("rpc"),
                [&callCount](Invocation)->Outcome
                {
                    ++callCount;
                    throw Error("wamp.error.not_authorized")
                          .withArgs(123)
                          .withKwargs(Object{{{"foo"},{"bar"}}});;
                    return {};
                },
                yield);

            {
                Error error;
                CHECK_THROWS_AS( f.caller->call(Rpc("rpc").captureError(error),
                                                yield),
                                 error::Failure );
                CHECK( !!error );
                CHECK_THAT( error.reason(),
                            Equals("wamp.error.not_authorized") );
                CHECK( error.args() == Array{123} );
                CHECK( error.kwargs() == (Object{{{"foo"},{"bar"}}}) );
            }

            {
                Error error;
                f.caller->call(Rpc("rpc").captureError(error), yield, &ec);
                CHECK( ec == SessionErrc::notAuthorized );
                CHECK( !!error );
                CHECK_THAT( error.reason(),
                            Equals("wamp.error.not_authorized") );
                CHECK( error.args() == Array{123} );
                CHECK( error.kwargs() == (Object{{{"foo"},{"bar"}}}) );
            }

            CHECK( callCount == 2 );
        });
        ioctx.run();
    }

    WHEN( "invoking a statically-typed RPC with invalid argument types" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            std::error_code ec;
            RpcFixture f(ioctx, cnct);
            f.join(yield);
            f.enroll(yield);

            // Check type mismatch
            CHECK_THROWS_AS( f.caller->call(Rpc("static").withArgs(42, 42),
                                            yield),
                             error::Failure );
            f.caller->call(Rpc("static").withArgs(42, 42), yield, &ec);
            CHECK( ec == SessionErrc::callError );
            CHECK( ec == SessionErrc::invalidArgument );
            CHECK( f.staticCount == 0 );

            // Check insufficient arguments
            CHECK_THROWS_AS( f.caller->call(Rpc("static").withArgs(42), yield),
                             error::Failure );
            f.caller->call(Rpc("static").withArgs(42), yield, &ec);
            CHECK( ec == SessionErrc::callError );
            CHECK( ec == SessionErrc::invalidArgument );
            CHECK( f.staticCount == 0 );
        });
        ioctx.run();
    }

    WHEN( "receiving a statically-typed event with invalid argument types" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(ioctx, cnct);
            f.subscriber->setWarningHandler( [](std::string){} );
            f.join(yield);
            f.subscribe(yield);

            // Publications with invalid arguments should be ignored.
            CHECK_NOTHROW( f.publisher->publish(
                               Pub("str.num").withArgs(42, 42), yield ) );

            // Publish with valid types so that we know when to stop polling.
            pid = f.publisher->publish(Pub("str.num").withArgs("foo", 42),
                                       yield);
            while (f.staticPubs.size() < 1)
                f.subscriber->suspend(yield);
            REQUIRE( f.staticPubs.size() == 1 );
            CHECK( f.staticPubs.back() == pid );

            // Publications with extra arguments should be handled,
            // as long as the required arguments have valid types.
            CHECK_NOTHROW( pid = f.publisher->publish(
                    Pub("str.num").withArgs("foo", 42, true), yield) );
            while (f.staticPubs.size() < 2)
                f.subscriber->suspend(yield);
            REQUIRE( f.staticPubs.size() == 2 );
            CHECK( f.staticPubs.back() == pid );
        });
        ioctx.run();
    }

    WHEN( "invoking an RPC that throws a wamp::error::BadType exceptions" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            std::error_code ec;
            RpcFixture f(ioctx, cnct);
            f.join(yield);

            f.callee->enroll(
                Procedure("bad_conversion"),
                [](Invocation inv)
                {
                    inv.args().front().to<String>();
                    return Result();
                },
                yield);

            f.callee->enroll(
                Procedure("bad_conv_coro"),
                basicCoroRpc<void, Variant>(
                [](Variant v, boost::asio::yield_context yield)
                {
                    v.to<String>();
                }),
                yield);

            f.callee->enroll(
                Procedure("bad_access"),
                basicRpc<void, Variant>( [](Variant v){v.as<String>();} ),
                yield);

            f.callee->enroll(
                Procedure("bad_access_coro"),
                unpackedCoroRpc<Variant>(
                [](Invocation inv, Variant v, boost::asio::yield_context yield)
                {
                    v.as<String>();
                    return Result();
                }),
                yield);


            // Check bad conversion
            CHECK_THROWS_AS( f.caller->call(Rpc("bad_conversion").withArgs(42),
                                            yield),
                             error::Failure );

            f.caller->call(Rpc("bad_conversion").withArgs(42), yield, &ec);
            CHECK( ec == SessionErrc::callError );
            CHECK( ec == SessionErrc::invalidArgument );

            // Check bad conversion in coroutine handler
            CHECK_THROWS_AS( f.caller->call(Rpc("bad_conv_coro").withArgs(42),
                                            yield),
                             error::Failure );

            f.caller->call(Rpc("bad_conv_coro").withArgs(42), yield, &ec);
            CHECK( ec == SessionErrc::callError );
            CHECK( ec == SessionErrc::invalidArgument );

            // Check bad access
            CHECK_THROWS_AS( f.caller->call(Rpc("bad_access").withArgs(42),
                                            yield),
                             error::Failure );

            f.caller->call(Rpc("bad_access").withArgs(42), yield, &ec);
            CHECK( ec == SessionErrc::callError );
            CHECK( ec == SessionErrc::invalidArgument );

            // Check bad access in couroutine handler
            CHECK_THROWS_AS( f.caller->call(Rpc("bad_access_coro").withArgs(42),
                                            yield),
                             error::Failure );

            f.caller->call(Rpc("bad_access_coro").withArgs(42), yield, &ec);
            CHECK( ec == SessionErrc::callError );
            CHECK( ec == SessionErrc::invalidArgument );
        });
        ioctx.run();
    }

    WHEN( "an event handler throws wamp::error::BadType exceptions" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            unsigned warningCount = 0;
            PubSubFixture f(ioctx, cnct);
            f.subscriber->setWarningHandler(
                [&warningCount](std::string){++warningCount;}
                );
            f.join(yield);
            f.subscribe(yield);

            f.subscriber->subscribe(
                Topic("bad_conversion"),
                basicEvent<Variant>([](Variant v) {v.to<String>();}),
                yield);

            f.subscriber->subscribe(
                Topic("bad_access"),
                [](Event event) {event.args().front().as<String>();},
                yield);

            f.subscriber->subscribe(
                Topic("bad_conversion_coro"),
                basicCoroEvent<Variant>(
                    [](Variant v, boost::asio::yield_context y)
                    {
                        v.to<String>();
                    }),
                yield);

            f.subscriber->subscribe(
                Topic("bad_access_coro"),
                unpackedCoroEvent<Variant>(
                    [](Event ev, Variant v, boost::asio::yield_context y)
                    {
                        v.to<String>();
                    }),
                yield);

            f.publisher->publish(Pub("bad_conversion").withArgs(42));
            f.publisher->publish(Pub("bad_access").withArgs(42));
            f.publisher->publish(Pub("bad_conversion_coro").withArgs(42));
            f.publisher->publish(Pub("bad_access_coro").withArgs(42));
            f.publisher->publish(Pub("other"));

            while (f.otherPubs.empty() || warningCount < 2)
                f.subscriber->suspend(yield);

            // The coroutine event handlers will not trigger
            // warning logs because the error::BadType exeception cannot
            // be propagated to Client by time it's thrown from within
            // the coroutine.
            CHECK( warningCount == 2 );
        });
        ioctx.run();
    }
}}

//------------------------------------------------------------------------------
SCENARIO( "Invalid WAMP URIs", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a TCP connector" )
{
    AsioContext ioctx;
    auto cnct = tcp(ioctx);

    WHEN( "joining with an invalid realm URI" )
    {
        using Yield = boost::asio::yield_context;
        checkInvalidUri(
            [](CoroSession<>& session, Yield yield)
                {session.join(Realm("#bad"), yield);},
            [](CoroSession<>& session, Yield yield, std::error_code& ec)
                {session.join(Realm("#bad"), yield, &ec);},
            false );
    }

    WHEN( "leaving with an invalid reason URI" )
    {
        using Yield = boost::asio::yield_context;
        checkInvalidUri(
            [](CoroSession<>& session, Yield yield)
                {session.leave(Reason("#bad"), yield);},
            [](CoroSession<>& session, Yield yield, std::error_code& ec)
                {session.leave(Reason("#bad"), yield, &ec);} );
    }

    WHEN( "subscribing with an invalid topic URI" )
    {
        using Yield = boost::asio::yield_context;
        checkInvalidUri(
            [](CoroSession<>& session, Yield yield)
                {session.subscribe(Topic("#bad"), [](Event) {}, yield);},
            [](CoroSession<>& session, Yield yield, std::error_code& ec)
                {session.subscribe(Topic("#bad"), [](Event) {}, yield, &ec);} );
    }

    WHEN( "publishing with an invalid topic URI" )
    {
        using Yield = boost::asio::yield_context;
        checkInvalidUri(
            [](CoroSession<>& session, Yield yield)
                {session.publish(Pub("#bad"), yield);},
            [](CoroSession<>& session, Yield yield, std::error_code& ec)
                {session.publish(Pub("#bad"), yield, &ec);} );

        AND_WHEN( "publishing with args" )
        {
            checkInvalidUri(
                [](CoroSession<>& session, Yield yield)
                    {session.publish(Pub("#bad").withArgs(42), yield);},
                [](CoroSession<>& session, Yield yield, std::error_code& ec)
                {
                    session.publish(Pub("#bad").withArgs(42), yield, &ec);
                });
        }
    }

    WHEN( "enrolling with an invalid procedure URI" )
    {
        using Yield = boost::asio::yield_context;
        checkInvalidUri(
            [](CoroSession<>& session, Yield yield)
            {
                session.enroll(Procedure("#bad"),
                               [](Invocation)->Outcome {return {};},
                yield);
            },
            [](CoroSession<>& session, Yield yield, std::error_code& ec)
            {
                session.enroll(Procedure("#bad"),
                               [](Invocation)->Outcome {return {};},
                yield, &ec);
            }
        );
    }

    WHEN( "calling with an invalid procedure URI" )
    {
        using Yield = boost::asio::yield_context;
        checkInvalidUri(
            [](CoroSession<>& session, Yield yield)
                {session.call(Rpc("#bad"), yield);},
            [](CoroSession<>& session, Yield yield, std::error_code& ec)
                {session.call(Rpc("#bad"), yield, &ec);} );

        AND_WHEN( "calling with args" )
        {
            checkInvalidUri(
                [](CoroSession<>& session, Yield yield)
                    {session.call(Rpc("#bad").withArgs(42), yield);},
                [](CoroSession<>& session, Yield yield, std::error_code& ec)
                    {session.call(Rpc("#bad").withArgs(42), yield, &ec);} );
        }
    }

    WHEN( "joining a non-existing realm" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            auto session = CoroSession<>::create(ioctx, cnct);
            session->connect(yield);

            bool throws = false;
            try
            {
                session->join(Realm("nonexistent"), yield);
            }
            catch (const error::Failure& e)
            {
                throws = true;
                CHECK( e.code() == SessionErrc::joinError );
                CHECK( e.code() == SessionErrc::noSuchRealm );
            }
            CHECK( throws );

            std::error_code ec;
            session->join(Realm("nonexistent"), yield, &ec);
            CHECK( ec == SessionErrc::joinError );
            CHECK( ec == SessionErrc::noSuchRealm );
        });

        ioctx.run();
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "WAMP Precondition Failures", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a TCP connector" )
{
    AsioContext ioctx;
    auto cnct = tcp(ioctx);

    WHEN( "constructing a session with an empty connector list" )
    {
        CHECK_THROWS_AS( Session::create(ioctx, ConnectorList{}),
                         error::Logic );
    }

    WHEN( "using invalid operations while disconnected" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            std::error_code ec;
            auto session = CoroSession<>::create(ioctx, cnct);
            REQUIRE( session->state() == SessionState::disconnected );
            checkInvalidJoin(session, yield);
            checkInvalidAuthenticate(session, yield);
            checkInvalidLeave(session, yield);
            checkInvalidOps(session, yield);
        });

        ioctx.run();
    }

    WHEN( "using invalid operations while connecting" )
    {
        auto session = CoroSession<>::create(ioctx, cnct);
        session->connect( [](AsyncResult<size_t>){} );

        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            ioctx.stop();
            ioctx.reset();
            REQUIRE( session->state() == SessionState::connecting );
            checkInvalidConnect(session, yield);
            checkInvalidJoin(session, yield);
            checkInvalidAuthenticate(session, yield);
            checkInvalidLeave(session, yield);
            checkInvalidOps(session, yield);
        });

        ioctx.run();
    }

    WHEN( "using invalid operations while failed" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            auto session = CoroSession<>::create(ioctx, invalidTcp(ioctx));
            CHECK_THROWS( session->connect(yield) );
            REQUIRE( session->state() == SessionState::failed );
            checkInvalidJoin(session, yield);
            checkInvalidAuthenticate(session, yield);
            checkInvalidLeave(session, yield);
            checkInvalidOps(session, yield);
        });

        ioctx.run();
    }

    WHEN( "using invalid operations while closed" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            auto session = CoroSession<>::create(ioctx, cnct);
            session->connect(yield);
            REQUIRE( session->state() == SessionState::closed );
            checkInvalidConnect(session, yield);
            checkInvalidAuthenticate(session, yield);
            checkInvalidLeave(session, yield);
            checkInvalidOps(session, yield);
        });

        ioctx.run();
    }

    WHEN( "using invalid operations while establishing" )
    {
        auto session = CoroSession<>::create(ioctx, cnct);
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            session->connect(yield);
        });

        ioctx.run();

        session->join(Realm(testRealm), [](AsyncResult<SessionInfo>){});

        AsioContext ioctx2;
        boost::asio::spawn(ioctx2, [&](boost::asio::yield_context yield)
        {
            REQUIRE( session->state() == SessionState::establishing );
            checkInvalidConnect(session, yield);
            checkInvalidJoin(session, yield);
            checkInvalidAuthenticate(session, yield);
            checkInvalidLeave(session, yield);
            checkInvalidOps(session, yield);
        });

        CHECK_NOTHROW( ioctx2.run() );
    }

    WHEN( "using invalid operations while established" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            auto session = CoroSession<>::create(ioctx, cnct);
            session->connect(yield);
            session->join(Realm(testRealm), yield);
            REQUIRE( session->state() == SessionState::established );
            checkInvalidConnect(session, yield);
            checkInvalidJoin(session, yield);
            checkInvalidAuthenticate(session, yield);
        });

        ioctx.run();
    }

    WHEN( "using invalid operations while shutting down" )
    {
        auto session = CoroSession<>::create(ioctx, cnct);
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            session->connect(yield);
            session->join(Realm(testRealm), yield);
            ioctx.stop();
        });
        ioctx.run();
        ioctx.reset();

        session->leave([](AsyncResult<Reason>){});

        AsioContext ioctx2;
        boost::asio::spawn(ioctx2, [&](boost::asio::yield_context yield)
        {
            REQUIRE( session->state() == SessionState::shuttingDown );
            checkInvalidConnect(session, yield);
            checkInvalidJoin(session, yield);
            checkInvalidAuthenticate(session, yield);
            checkInvalidLeave(session, yield);
            checkInvalidOps(session, yield);
        });
        CHECK_NOTHROW( ioctx2.run() );
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "WAMP Disconnect/Leave During Async Ops", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a TCP connector" )
{
    AsioContext ioctx;
    auto cnct = tcp(ioctx);

    WHEN( "disconnecting during async join" )
    {
        checkDisconnect<SessionInfo>([](CoroSession<>& session,
                                        boost::asio::yield_context,
                                        bool& completed,
                                        AsyncResult<SessionInfo>& result)
        {
            session.join(Realm(testRealm), [&](AsyncResult<SessionInfo> info)
            {
                completed = true;
                result = info;
            });
        });
    }

    WHEN( "disconnecting during async leave" )
    {
        checkDisconnect<Reason>([](CoroSession<>& session,
                                   boost::asio::yield_context yield,
                                   bool& completed,
                                   AsyncResult<Reason>& result)
        {
            session.join(Realm(testRealm), yield);
            session.leave([&](AsyncResult<Reason> reason)
            {
                completed = true;
                result = reason;
            });
        });
    }

    WHEN( "disconnecting during async subscribe" )
    {
        checkDisconnect<Subscription>(
                    [](CoroSession<>& session,
                    boost::asio::yield_context yield,
                    bool& completed,
                    AsyncResult<Subscription>& result)
        {
            session.join(Realm(testRealm), yield);
            session.subscribe(Topic("topic"), [] (Event) {},
                [&](AsyncResult<Subscription> sub)
                {
                    completed = true;
                    result = sub;
                });
        });
    }

    WHEN( "disconnecting during async unsubscribe" )
    {
        checkDisconnect<bool>([](CoroSession<>& session,
                                 boost::asio::yield_context yield,
                                 bool& completed,
                                 AsyncResult<bool>& result)
        {
            session.join(Realm(testRealm), yield);
            auto sub = session.subscribe(Topic("topic"), [] (Event) {}, yield);
            session.unsubscribe(sub, [&](AsyncResult<bool> unsubscribed)
            {
                completed = true;
                result = unsubscribed;
            });
        });
    }

    WHEN( "disconnecting during async unsubscribe via session" )
    {
        checkDisconnect<bool>([](CoroSession<>& session,
                                 boost::asio::yield_context yield,
                                 bool& completed,
                                 AsyncResult<bool>& result)
        {
            session.join(Realm(testRealm), yield);
            auto sub = session.subscribe(Topic("topic"), [](Event) {}, yield);
            session.unsubscribe(sub, [&](AsyncResult<bool> unsubscribed)
            {
                completed = true;
                result = unsubscribed;
            });
        });
    }

    WHEN( "disconnecting during async publish" )
    {
        checkDisconnect<PublicationId>([](CoroSession<>& session,
                                          boost::asio::yield_context yield,
                                          bool& completed,
                                          AsyncResult<PublicationId>& result)
        {
            session.join(Realm(testRealm), yield);
            session.publish(Pub("topic"), [&](AsyncResult<PublicationId> pid)
            {
                completed = true;
                result = pid;
            });
        });
    }

    WHEN( "disconnecting during async publish with args" )
    {
        checkDisconnect<PublicationId>([](CoroSession<>& session,
                                          boost::asio::yield_context yield,
                                          bool& completed,
                                          AsyncResult<PublicationId>& result)
        {
            session.join(Realm(testRealm), yield);
            session.publish(Pub("topic").withArgs("foo"),
                [&](AsyncResult<PublicationId> pid)
                {
                    completed = true;
                    result = pid;
                });
        });
    }

    WHEN( "disconnecting during async enroll" )
    {
        checkDisconnect<Registration>(
                    [](CoroSession<>& session,
                    boost::asio::yield_context yield,
                    bool& completed,
                    AsyncResult<Registration>& result)
        {
            session.join(Realm(testRealm), yield);
            session.enroll(Procedure("rpc"),
                           [](Invocation)->Outcome {return {};},
                           [&](AsyncResult<Registration> reg)
                           {
                               completed = true;
                               result = reg;
                           });
        });
    }

    WHEN( "disconnecting during async unregister" )
    {
        checkDisconnect<bool>([](CoroSession<>& session,
                                 boost::asio::yield_context yield,
                                 bool& completed,
                                 AsyncResult<bool>& result)
        {
            session.join(Realm(testRealm), yield);
            auto reg = session.enroll(Procedure("rpc"),
                                      [](Invocation)->Outcome{return {};},
                    yield);
            session.unregister(reg, [&](AsyncResult<bool> unregistered)
            {
                completed = true;
                result = unregistered;
            });
        });
    }

    WHEN( "disconnecting during async unregister via session" )
    {
        checkDisconnect<bool>([](CoroSession<>& session,
                                 boost::asio::yield_context yield,
                                 bool& completed,
                                 AsyncResult<bool>& result)
        {
            session.join(Realm(testRealm), yield);
            auto reg = session.enroll(Procedure("rpc"),
                                      [](Invocation)->Outcome{return {};},
                                      yield);
            session.unregister(reg, [&](AsyncResult<bool> unregistered)
            {
                completed = true;
                result = unregistered;
            });
        });
    }

    WHEN( "disconnecting during async call" )
    {
        checkDisconnect<Result>([](CoroSession<>& session,
                                   boost::asio::yield_context yield,
                                   bool& completed,
                                   AsyncResult<Result>& result)
        {
            session.join(Realm(testRealm), yield);
            session.call(Rpc("rpc").withArgs("foo"),
                [&](AsyncResult<Result> callResult)
                {
                    completed = true;
                    result = callResult;
                });
        });
    }

    WHEN( "issuing an asynchronous operation just before leaving" )
    {
        bool published = false;
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            auto s = CoroSession<>::create(ioctx, cnct);
            s->connect(yield);
            s->join(Realm(testRealm), yield);
            s->publish(Pub("topic"),
                       [&](AsyncResult<PublicationId>) {published = true;});
            s->leave(yield);
            CHECK( s->state() == SessionState::closed );
        });

        ioctx.run();
        CHECK( published == true );
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "Outbound Messages are Properly Enqueued", "[WAMP][Basic]" )
{
GIVEN( "these test fixture objects" )
{
    AsioContext ioctx;
    auto cnct = tcp(ioctx);
    auto session1 = CoroSession<>::create(ioctx, cnct);
    auto session2 = CoroSession<>::create(ioctx, cnct);

    WHEN( "an RPC is invoked while a large event payload is being transmitted" )
    {
        auto callee = session1;
        auto subscriber = session2;
        std::string eventString;

        // Fill large string with repeating character sequence
        std::string largeString(1024*1024, ' ');
        for (size_t i=0; i<largeString.length(); ++i)
            largeString[i] = '0' + (i % 64);

        auto onEvent = [&eventString](Event, std::string str)
        {
            eventString = std::move(str);
        };

        // Simple RPC that returns the string argument back to the caller.
        auto echo =
            [callee](Invocation, std::string str) -> Outcome
            {
                return Result({str});
            };

        // RPC that triggers the publishing of a large event payload
        auto trigger =
            [callee, &largeString] (Invocation) -> Outcome
            {
                callee->publish(Pub("grapevine").withArgs(largeString));
                return Result();
            };

        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            callee->connect(yield);
            callee->join(Realm(testRealm), yield);
            callee->enroll(Procedure("echo"), unpackedRpc<std::string>(echo),
                           yield);
            callee->enroll(Procedure("trigger"), trigger, yield);

            subscriber->connect(yield);
            subscriber->join(Realm(testRealm), yield);
            subscriber->subscribe(Topic("grapevine"),
                                  unpackedEvent<std::string>(onEvent),
                                  yield);

            for (int i=0; i<10; ++i)
            {
                // Use async call so that it doesn't block until completion.
                subscriber->call(Rpc("trigger").withArgs("hello"),
                                 [](AsyncResult<Result>) {});

                /*  Try to get callee to send an RPC response while it's still
                    transmitting the large event payload. AsioTransport should
                    properly enqueue the RPC response while the large event
                    payload is being transmitted. */
                while (eventString.empty())
                    subscriber->call(Rpc("echo").withArgs("hello"), yield);

                CHECK_THAT( eventString, Equals(largeString) );
                eventString.clear();
            }
            callee->disconnect();
            subscriber->disconnect();
        });

        ioctx.run();
    }
}
}

#endif // defined(CPPWAMP_TEST_HAS_CORO) && defined(CPPWAMP_TEST_HAS_SERIALIZER)
