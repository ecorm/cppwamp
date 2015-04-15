/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

// TODO: Test publishing/calling from within slot.

#if CPPWAMP_TESTING_WAMP

#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <catch.hpp>
#include <cppwamp/corosession.hpp>
#include <cppwamp/legacytcpconnector.hpp>
#include <cppwamp/legacyudsconnector.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/msgpack.hpp>

using namespace wamp;

namespace
{

const std::string testRealm = "cppwamp.test";
const short validPort = 12345;
const short invalidPort = 54321;
const std::string testUdsPath = "./.crossbar/udstest";

//------------------------------------------------------------------------------
struct PubSubFixture
{
    using PubVec = std::vector<PublicationId>;

    PubSubFixture(legacy::TcpConnector::Ptr cnct)
        : publisher(CoroSession<>::create(cnct)),
          subscriber(CoroSession<>::create(cnct)),
          otherSubscriber(CoroSession<>::create(cnct))
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

        staticSub = subscriber->subscribe<std::string, int>(
                Topic("str.num"),
                std::bind(&PubSubFixture::onStaticEvent, this, _1, _2, _3),
                yield);

        otherSub = otherSubscriber->subscribe(
                Topic("other"),
                std::bind(&PubSubFixture::onOtherEvent, this, _1),
                yield);
    }

    void onDynamicEvent(Event event)
    {
        INFO( "in onDynamicEvent" );
        CHECK( event.pubId() <= 9007199254740992ull );
        dynamicArgs = event.args();
        dynamicPubs.push_back(event.pubId());
    }

    void onStaticEvent(Event event, std::string str, int num)
    {
        INFO( "in onStaticEvent" );
        CHECK( event.pubId() <= 9007199254740992ull );
        staticArgs = Array{{str, num}};
        staticPubs.push_back(event.pubId());
    }

    void onOtherEvent(Event event)
    {
        INFO( "in onOtherEvent" );
        CHECK( event.pubId() <= 9007199254740992ull );
        otherPubs.push_back(event.pubId());
    }

    CoroSession<>::Ptr publisher;
    CoroSession<>::Ptr subscriber;
    CoroSession<>::Ptr otherSubscriber;

    Subscription::Ptr dynamicSub;
    Subscription::Ptr staticSub;
    Subscription::Ptr otherSub;

    PubVec dynamicPubs;
    PubVec staticPubs;
    PubVec otherPubs;

    Array dynamicArgs;
    Array staticArgs;
};

//------------------------------------------------------------------------------
struct RpcFixture
{
    RpcFixture(legacy::TcpConnector::Ptr cnct)
        : caller(CoroSession<>::create(cnct)),
          callee(CoroSession<>::create(cnct))
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

        staticReg = callee->enroll<std::string, int>(
                Procedure("static"),
                std::bind(&RpcFixture::staticRpc, this, _1, _2, _3),
                yield);
    }

    void dynamicRpc(Invocation inv)
    {
        INFO( "in RPC handler" );
        CHECK( inv.requestId() <= 9007199254740992ull );
        ++dynamicCount;
        // Echo back the call arguments as the yield result.
        inv.yield(Result().withArgs(inv.args()));
    }

    void staticRpc(Invocation inv, std::string str, int num)
    {
        INFO( "in RPC handler" );
        CHECK( inv.requestId() <= 9007199254740992ull );
        ++staticCount;
        // Echo back the call arguments as the yield result.
        inv.yield({str, num});
    }

    CoroSession<>::Ptr caller;
    CoroSession<>::Ptr callee;

    Registration::Ptr dynamicReg;
    Registration::Ptr staticReg;

    int dynamicCount = 0;
    int staticCount = 0;
};

//------------------------------------------------------------------------------
template <typename TThrowDelegate, typename TErrcDelegate>
void checkInvalidUri(TThrowDelegate&& throwDelegate,
                     TErrcDelegate&& errcDelegate, bool joined = true)
{
    AsioService iosvc;
    using legacy::TcpConnector;
    auto cnct = TcpConnector::create(iosvc, "localhost", validPort, Json::id());
    boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
    {
        auto session = CoroSession<>::create(cnct);
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

    iosvc.run();
}

//------------------------------------------------------------------------------
template <typename TResult, typename TDelegate>
void checkDisconnect(TDelegate&& delegate)
{
    AsioService iosvc;
    using legacy::TcpConnector;
    auto cnct = TcpConnector::create(iosvc, "localhost", validPort, Json::id());
    bool completed = false;
    AsyncResult<TResult> result;
    boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
    {
        auto session = CoroSession<>::create(cnct);
        session->connect(yield);
        delegate(*session, yield, completed, result);
        session->disconnect();
        CHECK( session->state() == SessionState::disconnected );
    });

    iosvc.run();
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

void checkInvalidLeave(CoroSession<>::Ptr session,
                       boost::asio::yield_context yield)
{
    std::error_code ec;

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
                        [](Event){}, [](AsyncResult<Subscription::Ptr>){}),
                     error::Logic );
    CHECK_THROWS_AS( session->publish(Pub("topic"),
                                     [](AsyncResult<PublicationId>) {}),
                     error::Logic );
    CHECK_THROWS_AS( session->publish(Pub("topic").withArgs({42}),
                                     [](AsyncResult<PublicationId>) {}),
                     error::Logic );
    CHECK_THROWS_AS( session->enroll(Procedure("rpc"), [](Invocation){},
                                    [](AsyncResult<Registration::Ptr>){}),
                     error::Logic );
    CHECK_THROWS_AS( session->call(Rpc("rpc"), [](AsyncResult<Result>) {}),
                     error::Logic );
    CHECK_THROWS_AS( session->call(Rpc("rpc").withArgs({42}),
                                  [](AsyncResult<Result>) {}),
                     error::Logic );

    CHECK_THROWS_AS( session->leave(Reason(), yield), error::Logic );
    CHECK_THROWS_AS( session->subscribe(Topic("topic"), [](Event){}, yield),
                     error::Logic );
    CHECK_THROWS_AS( session->publish(Pub("topic"), yield), error::Logic );
    CHECK_THROWS_AS( session->publish(Pub("topic").withArgs({42}), yield),
                     error::Logic );
    CHECK_THROWS_AS( session->enroll(Procedure("rpc"), [](Invocation){}, yield),
                     error::Logic );
    CHECK_THROWS_AS( session->call(Rpc("rpc"), yield), error::Logic );
    CHECK_THROWS_AS( session->call(Rpc("rpc").withArgs({42}), yield),
                     error::Logic );

    CHECK_THROWS_AS( session->leave(Reason(), yield, &ec), error::Logic );
    CHECK_THROWS_AS( session->subscribe(Topic("topic"), [](Event){}, yield, &ec),
                     error::Logic );
    CHECK_THROWS_AS( session->publish(Pub("topic"), yield, &ec),
                     error::Logic );
    CHECK_THROWS_AS( session->publish(Pub("topic").withArgs({42}), yield, &ec),
                     error::Logic );
    CHECK_THROWS_AS( session->enroll(Procedure("rpc"), [](Invocation){},
                                    yield, &ec),
                     error::Logic );
    CHECK_THROWS_AS( session->call(Rpc("rpc"), yield, &ec), error::Logic );
    CHECK_THROWS_AS( session->call(Rpc("rpc").withArgs({42}), yield, &ec),
                     error::Logic );
}

} // anonymous namespace

using legacy::TcpConnector;
using legacy::UdsConnector;

//------------------------------------------------------------------------------
SCENARIO( "Using WAMP session interface", "[WAMP]" )
{
#if CPPWAMP_TEST_SPAWN_CROSSBAR == 1
auto pid = fork();
REQUIRE( pid != -1 );

if (pid == 0)
{
    int fd = ::open("crossbar.log", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    ::dup2(fd, 1); // make stdout go to file
    ::dup2(fd, 2); // make stderr go to file
    ::close(fd);

    assert(::execlp("crossbar", "crossbar", "start", (char*)0) != -1);
}
else
{
    sleep(5);
#endif

    GIVEN( "an IO service and a TCP connector" )
    {
        AsioService iosvc;
        auto cnct = TcpConnector::create(iosvc, "localhost", validPort,
                                         Json::id());

    WHEN( "connecting and disconnecting" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            {
                // Connect and disconnect a session->
                auto s = CoroSession<>::create(cnct);
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
            auto s2 = CoroSession<>::create(cnct);
            CHECK( s2->state() == SessionState::disconnected );
            CHECK( s2->connect(yield) == 0 );
            CHECK( s2->state() == SessionState::closed );
            CHECK_NOTHROW( s2->disconnect() );
            CHECK( s2->state() == SessionState::disconnected );
        });

        iosvc.run();
    }

    WHEN( "joining and leaving" )
    {
        auto s = CoroSession<>::create(cnct);
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            s->connect(yield);
            CHECK( s->state() == SessionState::closed );

            {
                // Check joining.
                SessionInfo info = s->join(Realm(testRealm), yield);
                CHECK( s->state() == SessionState::established );
                CHECK ( info.id() <= 9007199254740992ull );
                CHECK( info.realm()  == testRealm );
                Object details = info.options();
                REQUIRE( details.count("roles") );
                REQUIRE( details["roles"].is<Object>() );
                Object roles = info.optionByKey("roles").as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );
                CHECK( info.supportsRoles({"broker", "dealer"}) );

                // Check leaving.
                Reason reason = s->leave(Reason(), yield);
                CHECK_FALSE( reason.uri().empty() );
                CHECK( s->state() == SessionState::closed );
            }

            {
                // Check that the same client can rejoin and leave.
                SessionInfo info = s->join(Realm(testRealm), yield);
                CHECK( s->state() == SessionState::established );
                CHECK ( info.id() <= 9007199254740992ull );
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

        iosvc.run();
    }

    WHEN( "connecting, joining, leaving, and disconnecting" )
    {
        auto s = CoroSession<>::create(cnct);
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
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
                Reason reason = s->leave(Reason(), yield);
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
                CHECK ( info.id() <= 9007199254740992ull );
                CHECK( info.realm()  == testRealm );
                Object details = info.options();
                REQUIRE( details.count("roles") );
                REQUIRE( details["roles"].is<Object>() );
                Object roles = info.optionByKey("roles").as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );
                CHECK( info.supportsRoles({"broker", "dealer"}) );

                // Leave
                Reason reason = s->leave(Reason(), yield);
                CHECK_FALSE( reason.uri().empty() );
                CHECK( s->state() == SessionState::closed );

                // Disconnect
                CHECK_NOTHROW( s->disconnect() );
                CHECK( s->state() == SessionState::disconnected );
            }
        });

        iosvc.run();
    }

    WHEN( "disconnecting during connect" )
    {
        std::error_code ec;
        auto s = Session::create(cnct);
        s->connect([&](AsyncResult<size_t> result)
        {
            ec = result.errorCode();
        });
        s->disconnect();

        iosvc.run();
        iosvc.reset();
        CHECK( ec == TransportErrc::aborted );

        s->reset();
        ec.clear();
        bool connected = false;
        s->connect([&](AsyncResult<size_t> result)
        {
            ec = result.errorCode();
            connected = !ec;
        });

        iosvc.run();
        CHECK( ec == TransportErrc::success );
        CHECK( connected );
    }

    WHEN( "disconnecting during coroutine join" )
    {
        std::error_code ec;
        bool connected = false;
        auto s = CoroSession<>::create(cnct);
        bool disconnectTriggered = false;
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
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

        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            while (!disconnectTriggered)
                iosvc.post(yield);
            s->disconnect();
        });

        iosvc.run();
        iosvc.reset();
        CHECK_FALSE( connected );
        CHECK( ec == SessionErrc::sessionEnded );
    }

    WHEN( "resetting during connect" )
    {
        bool handlerWasInvoked = false;
        auto s = Session::create(cnct);
        s->connect([&handlerWasInvoked](AsyncResult<size_t>)
        {
            handlerWasInvoked = true;
        });
        s->reset();
        iosvc.run();

        CHECK_FALSE( handlerWasInvoked );
    }

    WHEN( "resetting during join" )
    {
        bool handlerWasInvoked = false;
        auto s = Session::create(cnct);
        s->connect([&](AsyncResult<size_t>)
        {
            s->join(Realm(testRealm), [&](AsyncResult<SessionInfo>)
            {
                handlerWasInvoked = true;
            });
            s->reset();
        });
        iosvc.run();

        CHECK_FALSE( handlerWasInvoked );
    }

    WHEN( "session goes out of scope during connect" )
    {
        bool handlerWasInvoked = false;

        auto session = Session::create(cnct);
        std::weak_ptr<Session> weakClient(session);

        session->connect([&handlerWasInvoked](AsyncResult<size_t>)
        {
            handlerWasInvoked = true;
        });

        // Reduce session reference count to zero
        session = nullptr;
        REQUIRE( weakClient.expired() );

        iosvc.run();

        CHECK_FALSE( handlerWasInvoked );
    }

    WHEN( "joining using a UDS transport and Msgpack serializer" )
    {
        auto cnct2 = UdsConnector::create(iosvc, testUdsPath, Msgpack::id());

        auto s = CoroSession<>::create(cnct2);
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            s->connect(yield);
            CHECK( s->state() == SessionState::closed );

            {
                // Check joining.
                SessionInfo info = s->join(Realm(testRealm), yield);
                CHECK( s->state() == SessionState::established );
                CHECK ( info.id() <= 9007199254740992ull );
                CHECK( info.realm()  == testRealm );
                Object details = info.options();
                REQUIRE( details.count("roles") );
                REQUIRE( details["roles"].is<Object>() );
                Object roles = info.optionByKey("roles").as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );
                CHECK( info.supportsRoles({"broker", "dealer"}) );

                // Check leaving.
                Reason reason = s->leave(Reason(), yield);
                CHECK_FALSE( reason.uri().empty() );
                CHECK( s->state() == SessionState::closed );
            }

            {
                // Check that the same client can rejoin and leave.
                SessionInfo info = s->join(Realm(testRealm), yield);
                CHECK( s->state() == SessionState::established );
                CHECK ( info.id() <= 9007199254740992ull );
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

        iosvc.run();
    }

    WHEN( "publishing and subscribing" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(cnct);
            f.join(yield);
            f.subscribe(yield);

            // Check dynamic and static subscriptions.
            f.publisher->publish(Pub("str.num").withArgs({"one", 1}));
            pid = f.publisher->publish(Pub("str.num").withArgs({"two", 2}),
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
            pid = f.publisher->publish(Pub("str.num").withArgs({"three", 3}),
                                       yield);
            while (f.staticPubs.size() < 3)
                f.otherSubscriber->suspend(yield);
            REQUIRE( f.dynamicPubs.size() == 2 );
            REQUIRE( f.staticPubs.size() == 3 );
            CHECK( f.staticPubs.back() == pid );
            CHECK(( f.staticArgs == Array{"three", 3} ));

            // Unsubscribe the static subscription via RAII.
            f.staticSub.reset();

            // Check that the dynamic and static slots no longer fire, and
            // that the "other" slot still fires.
            f.publisher->publish(Pub("str.num").withArgs({"four", 4}), yield);
            pid = f.publisher->publish(Pub("other"), yield);
            while (f.otherPubs.size() < 3)
                f.subscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 2 );
            CHECK( f.staticPubs.size() == 3 );
            REQUIRE( f.otherPubs.size() == 3 );
            CHECK( f.otherPubs.back() == pid );

            // Make the "other" subscriber leave and rejoin the realm.
            f.otherSubscriber->leave(Reason(), yield);
            f.otherSubscriber->join(Realm(testRealm), yield);

            // Reestablish the dynamic subscription.
            using namespace std::placeholders;
            f.dynamicSub = f.subscriber->subscribe(
                    Topic("str.num"),
                    std::bind(&PubSubFixture::onDynamicEvent, &f, _1),
                    yield);

            // Check that only the dynamic slot still fires.
            f.publisher->publish(Pub("other"), yield);
            pid = f.publisher->publish(Pub("str.num").withArgs({"five", 5}),
                                       yield);
            while (f.dynamicPubs.size() < 3)
                f.subscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 3 );
            CHECK( f.staticPubs.size() == 3 );
            REQUIRE( f.otherPubs.size() == 3 );
            CHECK( f.dynamicPubs.back() == pid );
            CHECK(( f.dynamicArgs == Array{"five", 5} ));
        });

        iosvc.run();
    }

    WHEN( "unsubscribing multiple times" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(cnct);
            f.join(yield);
            f.subscribe(yield);

            // Unsubscribe the dynamic subscription manually.
            f.dynamicSub->unsubscribe();

            // Unsubscribe the dynamic subscription again via RAII.
            f.dynamicSub.reset();

            // Check that the dynamic slot no longer fires, and that the
            // static slot still fires.
            pid = f.publisher->publish(Pub("str.num").withArgs({"foo", 42}),
                                       yield);
            while (f.staticPubs.size() < 1)
                f.subscriber->suspend(yield);
            REQUIRE( f.dynamicPubs.size() == 0 );
            REQUIRE( f.staticPubs.size() == 1 );
            CHECK( f.staticPubs.back() == pid );

            // Unsubscribe the static subscription manually.
            f.subscriber->unsubscribe(f.staticSub, yield);

            // Unsubscribe the static subscription again manually.
            f.staticSub->unsubscribe();

            // Check that the dynamic and static slots no longer fire.
            // Publish to the "other" subscription so that we know when
            // to stop polling.
            f.publisher->publish(Pub("str.num").withArgs({"foo", 42}), yield);
            pid = f.publisher->publish(Pub("other"), yield);
            while (f.otherPubs.size() < 1)
                f.otherSubscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 0 );
            CHECK( f.staticPubs.size() == 1 );
            REQUIRE( f.otherPubs.size() == 1 );
            CHECK( f.otherPubs.back() == pid );
        });

        iosvc.run();
    }

    WHEN( "unsubscribing after session is destroyed" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(cnct);
            f.join(yield);
            f.subscribe(yield);

            // Destroy the subscriber session
            f.subscriber.reset();

            // Unsubscribe the dynamic subscription manually.
            REQUIRE_NOTHROW( f.dynamicSub->unsubscribe() );

            // Unsubscribe the static subscription via RAII.
            REQUIRE_NOTHROW( f.staticSub.reset() );

            // Check that the dynamic and static slots no longer fire.
            // Publish to the "other" subscription so that we know when
            // to stop polling.
            f.publisher->publish(Pub("str.num").withArgs({"foo", 42}), yield);
            pid = f.publisher->publish(Pub("other"), yield);
            while (f.otherPubs.size() < 1)
                f.otherSubscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 0 );
            CHECK( f.staticPubs.size() == 0 );
            REQUIRE( f.otherPubs.size() == 1 );
            CHECK( f.otherPubs.back() == pid );
        });

        iosvc.run();
    }

    WHEN( "unsubscribing after leaving" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(cnct);
            f.join(yield);
            f.subscribe(yield);

            // Make the subscriber client leave the session.
            f.subscriber->leave(Reason(), yield);

            // Unsubscribe the dynamic subscription via RAII.
            REQUIRE_NOTHROW( f.dynamicSub.reset() );

            // Unsubscribe the static subscription manually.
            CHECK_THROWS_AS( f.subscriber->unsubscribe(f.staticSub, yield),
                             error::Logic );
            REQUIRE_NOTHROW( f.staticSub->unsubscribe() );

            // Check that the dynamic and static slots no longer fire.
            // Publish to the "other" subscription so that we know when
            // to stop polling.
            f.publisher->publish(Pub("str.num").withArgs({"foo", 42}), yield);
            pid = f.publisher->publish(Pub("other"), yield);
            while (f.otherPubs.size() < 1)
                f.otherSubscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 0 );
            CHECK( f.staticPubs.size() == 0 );
            REQUIRE( f.otherPubs.size() == 1 );
            CHECK( f.otherPubs.back() == pid );
        });

        iosvc.run();
    }

    WHEN( "unsubscribing after disconnecting" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(cnct);
            f.join(yield);
            f.subscribe(yield);

            // Make the subscriber client disconnect.
            f.subscriber->disconnect();

            // Unsubscribe the dynamic subscription manually.
            CHECK_THROWS_AS( f.subscriber->unsubscribe(f.dynamicSub, yield),
                             error::Logic );
            REQUIRE_NOTHROW( f.dynamicSub->unsubscribe() );

            // Unsubscribe the static subscription via RAII.
            REQUIRE_NOTHROW( f.staticSub.reset() );

            // Check that the dynamic and static slots no longer fire.
            // Publish to the "other" subscription so that we know when
            // to stop polling.
            f.publisher->publish(Pub("str.num").withArgs({"foo", 42}), yield);
            pid = f.publisher->publish(Pub("other"), yield);
            while (f.otherPubs.size() < 1)
                f.otherSubscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 0 );
            CHECK( f.staticPubs.size() == 0 );
            REQUIRE( f.otherPubs.size() == 1 );
            CHECK( f.otherPubs.back() == pid );
        });

        iosvc.run();
    }

    WHEN( "unsubscribing after reset" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(cnct);
            f.join(yield);
            f.subscribe(yield);

            // Destroy the subscriber
            f.subscriber.reset();

            // Unsubscribe the static subscription via RAII.
            REQUIRE_NOTHROW( f.staticSub.reset() );

            // Check that the dynamic and static slots no longer fire.
            // Publish to the "other" subscription so that we know when
            // to stop polling.
            f.publisher->publish(Pub("str.num").withArgs({"foo", 42}), yield);
            pid = f.publisher->publish(Pub("other"), yield);
            while (f.otherPubs.size() < 1)
                f.otherSubscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 0 );
            CHECK( f.staticPubs.size() == 0 );
            REQUIRE( f.otherPubs.size() == 1 );
            CHECK( f.otherPubs.back() == pid );
        });

        iosvc.run();
    }

    WHEN( "calling remote procedures taking dynamically-typed args" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            Result result;
            std::error_code ec;
            RpcFixture f(cnct);
            f.join(yield);
            f.enroll(yield);

            // Check normal RPC
            result = f.caller->call(Rpc("dynamic").withArgs({"one", 1}),
                                    yield);
            CHECK( f.dynamicCount == 1 );
            CHECK(( result.args() == Array{"one", 1} ));
            result = f.caller->call(Rpc("dynamic").withArgs({"two", 2}),
                                    yield);
            CHECK( f.dynamicCount == 2 );
            CHECK(( result.args() == Array{"two", 2} ));

            // Manually unregister the slot.
            f.callee->unregister(f.dynamicReg, yield);

            // The router should now report an error when attempting
            // to call the unregistered RPC.
            CHECK_THROWS_AS( f.caller->call(
                                 Rpc("dynamic").withArgs({"three", 3}),
                                 yield),
                             error::Failure);
            f.caller->call(Rpc("dynamic").withArgs({"three", 3}), yield, &ec);
            CHECK( ec == SessionErrc::callError );
            CHECK( ec == SessionErrc::noSuchProcedure );

            // Calling should work after re-registering the slot.
            using namespace std::placeholders;
            f.dynamicReg = f.callee->enroll(
                Procedure("dynamic"),
                std::bind(&RpcFixture::dynamicRpc, &f, _1),
                yield);
            result = f.caller->call(Rpc("dynamic").withArgs({"four", 4}),
                                    yield);
            CHECK( f.dynamicCount == 3 );
            CHECK(( result.args() == Array{"four", 4} ));
        });
        iosvc.run();
    }

    WHEN( "calling remote procedures taking statically-typed args" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            Result result;
            std::error_code ec;
            RpcFixture f(cnct);
            f.join(yield);
            f.enroll(yield);

            // Check normal RPC
            result = f.caller->call(Rpc("static").withArgs({"one", 1}),
                                    yield);
            CHECK( f.staticCount == 1 );
            CHECK(( result.args() == Array{"one", 1} ));

            // Extra arguments should be ignored.
            result = f.caller->call(Rpc("static").withArgs({"two", 2, true}),
                                    yield);
            CHECK( f.staticCount == 2 );
            CHECK(( result.args() == Array{"two", 2} ));

            // Unregister the slot via RAII.
            f.staticReg.reset();

            // The router should now report an error when attempting
            // to call the unregistered RPC.
            CHECK_THROWS_AS( f.caller->call(
                                 Rpc("static").withArgs({"three", 3}),
                                 yield),
                             error::Failure );
            f.caller->call(Rpc("static").withArgs({"three", 3}), yield, &ec);
            CHECK( ec == SessionErrc::callError );
            CHECK( ec == SessionErrc::noSuchProcedure );

            // Calling should work after re-registering the slot.
            using namespace std::placeholders;
            f.staticReg = f.callee->enroll<std::string, int>(
                Procedure("static"),
                std::bind(&RpcFixture::staticRpc, &f, _1, _2, _3),
                yield);
            result = f.caller->call(Rpc("static").withArgs({"four", 4}), yield);
            CHECK( f.staticCount == 3 );
            CHECK(( result.args() == Array{"four", 4} ));
        });
        iosvc.run();
    }

    WHEN( "unregistering after a session is destroyed" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            std::error_code ec;
            RpcFixture f(cnct);
            f.join(yield);
            f.enroll(yield);

            // Destroy the callee session
            f.callee.reset();

            // Manually unregister a RPC.
            REQUIRE_NOTHROW( f.dynamicReg->unregister() );

            // Unregister an RPC via RAII.
            REQUIRE_NOTHROW( f.staticReg.reset() );

            // The router should report an error when attempting
            // to call the unregistered RPCs.
            CHECK_THROWS_AS( f.caller->call(Rpc("dynamic").withArgs({"one", 1}),
                                            yield),
                             error::Failure );
            f.caller->call(Rpc("dynamic").withArgs({"one", 1}), yield, &ec);
            CHECK( ec == SessionErrc::noSuchProcedure );

            CHECK_THROWS_AS( f.caller->call(Rpc("static").withArgs({"two", 2}),
                                            yield),
                             error::Failure );
            f.caller->call(Rpc("dynamic").withArgs({"two", 2}), yield, &ec);
            CHECK( ec == SessionErrc::noSuchProcedure );
        });
        iosvc.run();
    }

    WHEN( "unregistering after leaving" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            std::error_code ec;
            RpcFixture f(cnct);
            f.join(yield);
            f.enroll(yield);

            // Make the callee leave the session.
            f.callee->leave(Reason(), yield);

            // Manually unregister a RPC.
            CHECK_THROWS_AS( f.callee->unregister(f.dynamicReg, yield),
                             error::Logic );
            REQUIRE_NOTHROW( f.dynamicReg->unregister() );

            // Unregister an RPC via RAII.
            REQUIRE_NOTHROW( f.staticReg.reset() );

            // The router should report an error when attempting
            // to call the unregistered RPCs.
            CHECK_THROWS_AS( f.caller->call(Rpc("dynamic").withArgs({"one", 1}),
                                            yield),
                             error::Failure );
            f.caller->call(Rpc("dynamic").withArgs({"one", 1}), yield, &ec);
            CHECK( ec == SessionErrc::noSuchProcedure );

            CHECK_THROWS_AS( f.caller->call(Rpc("static").withArgs({"two", 2}),
                                            yield),
                             error::Failure );
            f.caller->call(Rpc("dynamic").withArgs({"two", 2}), yield, &ec);
            CHECK( ec == SessionErrc::noSuchProcedure );
        });
        iosvc.run();
    }

    WHEN( "unregistering after disconnecting" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            std::error_code ec;
            RpcFixture f(cnct);
            f.join(yield);
            f.enroll(yield);

            // Make the callee disconnect.
            f.callee->disconnect();

            // Manually unregister a RPC.
            CHECK_THROWS_AS( f.callee->unregister(f.dynamicReg, yield),
                             error::Logic );
            REQUIRE_NOTHROW( f.dynamicReg->unregister() );

            // Unregister an RPC via RAII.
            REQUIRE_NOTHROW( f.staticReg.reset() );

            // The router should report an error when attempting
            // to call the unregistered RPCs.
            CHECK_THROWS_AS( f.caller->call(Rpc("dynamic").withArgs({"one", 1}),
                                            yield),
                             error::Failure );
            f.caller->call(Rpc("dynamic").withArgs({"one", 1}), yield, &ec);
            CHECK( ec == SessionErrc::noSuchProcedure );

            CHECK_THROWS_AS( f.caller->call(Rpc("static").withArgs({"two", 2}),
                                            yield),
                             error::Failure );
            f.caller->call(Rpc("dynamic").withArgs({"two", 2}), yield, &ec);
            CHECK( ec == SessionErrc::noSuchProcedure );
        });
        iosvc.run();
    }

    WHEN( "unregistering after reset" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            std::error_code ec;
            RpcFixture f(cnct);
            f.join(yield);
            f.enroll(yield);

            // Destroy the callee.
            f.callee.reset();

            // Unregister an RPC via RAII.
            REQUIRE_NOTHROW( f.staticReg.reset() );

            // The router should report an error when attempting
            // to call the unregistered RPCs.
            CHECK_THROWS_AS( f.caller->call(Rpc("dynamic").withArgs({"one", 1}),
                                            yield),
                             error::Failure );
            f.caller->call(Rpc("dynamic").withArgs({"one", 1}), yield, &ec);
            CHECK( ec == SessionErrc::noSuchProcedure );

            CHECK_THROWS_AS( f.caller->call(Rpc("static").withArgs({"two", 2}),
                                            yield),
                             error::Failure );
            f.caller->call(Rpc("dynamic").withArgs({"two", 2}), yield, &ec);
            CHECK( ec == SessionErrc::noSuchProcedure );
        });
        iosvc.run();
    }

    WHEN( "connecting to an invalid port" )
    {
        auto badCnct = TcpConnector::create(iosvc, "localhost", invalidPort,
                                            Json::id());

        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            auto session = CoroSession<>::create(badCnct);
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

        iosvc.run();
    }

    WHEN( "connecting with multiple transports" )
    {
        auto badCnct = TcpConnector::create(iosvc, "localhost", invalidPort,
                                            Json::id());
        ConnectorList connectors = {badCnct, cnct};

        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            auto s = CoroSession<>::create(connectors);

            {
                // Connect
                CHECK( s->state() == SessionState::disconnected );
                CHECK( s->connect(yield) == 1 );
                CHECK( s->state() == SessionState::closed );

                // Join
                SessionInfo info = s->join(Realm(testRealm), yield);
                CHECK( s->state() == SessionState::established );
                CHECK ( info.id() <= 9007199254740992ull );
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
                CHECK ( info.id() <= 9007199254740992ull );
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

        iosvc.run();
    }

    WHEN( "registering an already existing procedure" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            RpcFixture f(cnct);
            f.join(yield);
            f.enroll(yield);

            std::error_code ec;
            Registration::Ptr reg;
            auto handler = [](Invocation) {};

            CHECK_THROWS_AS( f.callee->enroll(Procedure("dynamic"), handler,
                                              yield),
                             error::Failure );
            reg = f.callee->enroll(Procedure("dynamic"), handler, yield, &ec);
            CHECK( ec == SessionErrc::registerError );
            CHECK( ec == SessionErrc::procedureAlreadyExists );
            CHECK( !reg );
        });
        iosvc.run();
    }

    WHEN( "an RPC returns an error URI" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            std::error_code ec;
            int callCount = 0;
            RpcFixture f(cnct);
            f.join(yield);
            f.enroll(yield);

            auto reg = f.callee->enroll(
                Procedure("rpc"),
                [&callCount](Invocation inv)
                {
                    ++callCount;
                    inv.yield(Error("wamp.error.not_authorized"));
                },
                yield);

            CHECK_THROWS_AS( f.caller->call(Rpc("rpc"), yield),
                             error::Failure );
            f.caller->call(Rpc("rpc"), yield, &ec);
            CHECK( ec == SessionErrc::notAuthorized );
            CHECK( callCount == 2 );
        });
        iosvc.run();
    }

    WHEN( "invoking a statically-typed RPC with invalid argument types" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            std::error_code ec;
            RpcFixture f(cnct);
            f.join(yield);
            f.enroll(yield);

            // Check type mismatch
            CHECK_THROWS_AS( f.caller->call(Rpc("static").withArgs({42, 42}),
                                            yield),
                             error::Failure );
            f.caller->call(Rpc("static").withArgs({42, 42}), yield, &ec);
            CHECK( ec == SessionErrc::callError );
            CHECK( ec == SessionErrc::invalidArgument );
            CHECK( f.staticCount == 0 );

            // Check insufficient arguments
            CHECK_THROWS_AS( f.caller->call(Rpc("static").withArgs({42}),
                                            yield),
                             error::Failure );
            f.caller->call(Rpc("static").withArgs({42}), yield, &ec);
            CHECK( ec == SessionErrc::callError );
            CHECK( ec == SessionErrc::invalidArgument );
            CHECK( f.staticCount == 0 );
        });
        iosvc.run();
    }

    WHEN( "receiving a statically-typed event with invalid argument types" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(cnct);
            f.join(yield);
            f.subscribe(yield);

            // Publications with invalid arguments should be ignored.
            CHECK_NOTHROW( f.publisher->publish(
                               Pub("str.num").withArgs({42, 42}), yield ) );

            // Publish with valid types so that we know when to stop polling.
            pid = f.publisher->publish(Pub("str.num").withArgs({"foo", 42}),
                                       yield);
            while (f.staticPubs.size() < 1)
                f.subscriber->suspend(yield);
            REQUIRE( f.staticPubs.size() == 1 );
            CHECK( f.staticPubs.back() == pid );

            // Publications with extra arguments should be handled,
            // as long as the required arguments have valid types.
            CHECK_NOTHROW( pid = f.publisher->publish(
                    Pub("str.num").withArgs({"foo", 42, true}), yield) );
            while (f.staticPubs.size() < 2)
                f.subscriber->suspend(yield);
            REQUIRE( f.staticPubs.size() == 2 );
            CHECK( f.staticPubs.back() == pid );
        });
        iosvc.run();
    }

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
                    {session.publish(Pub("#bad").withArgs({42}), yield);},
                [](CoroSession<>& session, Yield yield, std::error_code& ec)
                {
                    session.publish(Pub("#bad").withArgs({42}), yield, &ec);
                });
        }
    }

    WHEN( "enrolling with an invalid procedure URI" )
    {
        using Yield = boost::asio::yield_context;
        checkInvalidUri(
            [](CoroSession<>& session, Yield yield)
            {
                session.enroll(Procedure("#bad"), [](Invocation) {}, yield);
            },
            [](CoroSession<>& session, Yield yield, std::error_code& ec)
            {
                session.enroll(Procedure("#bad"), [](Invocation) {}, yield, &ec);
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
                    {session.call(Rpc("#bad").withArgs({42}), yield);},
                [](CoroSession<>& session, Yield yield, std::error_code& ec)
                    {session.call(Rpc("#bad").withArgs({42}), yield, &ec);} );
        }
    }

    WHEN( "joining a non-existing realm" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            auto session = CoroSession<>::create(cnct);
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

        iosvc.run();
    }

    WHEN( "constructing a session with an empty connector list" )
    {
        CHECK_THROWS_AS( Session::create(ConnectorList{}), error::Logic );
    }

    WHEN( "using invalid operations while disconnected" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            std::error_code ec;
            auto session = CoroSession<>::create(cnct);
            REQUIRE( session->state() == SessionState::disconnected );
            checkInvalidJoin(session, yield);
            checkInvalidLeave(session, yield);
            checkInvalidOps(session, yield);
        });

        iosvc.run();
    }

    WHEN( "using invalid operations while connecting" )
    {
        auto session = CoroSession<>::create(cnct);
        session->connect( [](AsyncResult<size_t>){} );

        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            iosvc.stop();
            iosvc.reset();
            REQUIRE( session->state() == SessionState::connecting );
            checkInvalidConnect(session, yield);
            checkInvalidJoin(session, yield);
            checkInvalidLeave(session, yield);
            checkInvalidOps(session, yield);
        });

        iosvc.run();
    }

    WHEN( "using invalid operations while failed" )
    {
        auto badCnct = TcpConnector::create(iosvc, "localhost", invalidPort,
                                            Json::id());

        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            auto session = CoroSession<>::create(badCnct);
            CHECK_THROWS( session->connect(yield) );
            REQUIRE( session->state() == SessionState::failed );
            checkInvalidJoin(session, yield);
            checkInvalidLeave(session, yield);
            checkInvalidOps(session, yield);
        });

        iosvc.run();
    }

    WHEN( "using invalid operations while closed" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            auto session = CoroSession<>::create(cnct);
            session->connect(yield);
            REQUIRE( session->state() == SessionState::closed );
            checkInvalidConnect(session, yield);
            checkInvalidLeave(session, yield);
            checkInvalidOps(session, yield);
        });

        iosvc.run();
    }

    WHEN( "using invalid operations while establishing" )
    {
        auto session = CoroSession<>::create(cnct);
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            session->connect(yield);
        });

        iosvc.run();

        session->join(Realm(testRealm), [](AsyncResult<SessionInfo>){});

        AsioService iosvc2;
        boost::asio::spawn(iosvc2, [&](boost::asio::yield_context yield)
        {
            REQUIRE( session->state() == SessionState::establishing );
            checkInvalidConnect(session, yield);
            checkInvalidJoin(session, yield);
            checkInvalidLeave(session, yield);
            checkInvalidOps(session, yield);
        });

        CHECK_NOTHROW( iosvc2.run() );
    }

    WHEN( "using invalid operations while established" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            auto session = CoroSession<>::create(cnct);
            session->connect(yield);
            session->join(Realm(testRealm), yield);
            REQUIRE( session->state() == SessionState::established );
            checkInvalidConnect(session, yield);
            checkInvalidJoin(session, yield);
        });

        iosvc.run();
    }

    WHEN( "using invalid operations while shutting down" )
    {
        auto session = CoroSession<>::create(cnct);
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            session->connect(yield);
            session->join(Realm(testRealm), yield);
            iosvc.stop();
        });
        iosvc.run();
        iosvc.reset();

        session->leave(Reason(), [](AsyncResult<Reason>){});

        AsioService iosvc2;
        boost::asio::spawn(iosvc2, [&](boost::asio::yield_context yield)
        {
            REQUIRE( session->state() == SessionState::shuttingDown );
            checkInvalidConnect(session, yield);
            checkInvalidJoin(session, yield);
            checkInvalidLeave(session, yield);
            checkInvalidOps(session, yield);
        });
        CHECK_NOTHROW( iosvc2.run() );
    }

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
            session.leave(Reason(), [&](AsyncResult<Reason> reason)
            {
                completed = true;
                result = reason;
            });
        });
    }

    WHEN( "disconnecting during async subscribe" )
    {
        checkDisconnect<Subscription::Ptr>(
                    [](CoroSession<>& session,
                    boost::asio::yield_context yield,
                    bool& completed,
                    AsyncResult<Subscription::Ptr>& result)
        {
            session.join(Realm(testRealm), yield);
            session.subscribe(Topic("topic"), [] (Event) {},
                [&](AsyncResult<Subscription::Ptr> sub)
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
            sub->unsubscribe([&](AsyncResult<bool> unsubscribed)
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
            session.publish(Pub("topic").withArgs({"foo"}),
                [&](AsyncResult<PublicationId> pid)
                {
                    completed = true;
                    result = pid;
                });
        });
    }

    WHEN( "disconnecting during async enroll" )
    {
        checkDisconnect<Registration::Ptr>(
                    [](CoroSession<>& session,
                    boost::asio::yield_context yield,
                    bool& completed,
                    AsyncResult<Registration::Ptr>& result)
        {
            session.join(Realm(testRealm), yield);
            session.enroll(Procedure("rpc"), [] (Invocation) {},
                [&](AsyncResult<Registration::Ptr> reg)
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
            auto reg = session.enroll(Procedure("rpc"), [](Invocation){}, yield);
            reg->unregister([&](AsyncResult<bool> unregistered)
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
            auto reg = session.enroll(Procedure("rpc"), [](Invocation){}, yield);
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
            session.call(Rpc("rpc").withArgs({"foo"}),
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
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            auto s = CoroSession<>::create(cnct);
            s->connect(yield);
            s->join(Realm(testRealm), yield);
            s->publish(Pub("topic"),
                       [&](AsyncResult<PublicationId>) {published = true;});
            s->leave(Reason(), yield);
            CHECK( s->state() == SessionState::closed );
        });

        iosvc.run();
        CHECK( published == true );
    }

    } // GIVEN( "an IO service and a TCP connector" )

#if CPPWAMP_TEST_SPAWN_CROSSBAR == 1
    ::kill(pid, SIGTERM);
    ::sleep(3);
    int status = 0;
    if (::waitpid(pid, &status, WNOHANG) != pid)
    {
        ::kill(pid, SIGKILL);
        ::waitpid(pid, &status, 0);
    }
}
#endif
}

#endif // #if CPPWAMP_TESTING_WAMP

