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
#include <cppwamp/coroerrcclient.hpp>
#include <cppwamp/coroclient.hpp>
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
        : publisher(CoroClient<>::create(cnct)),
          subscriber(CoroClient<>::create(cnct)),
          voidSubscriber(CoroClient<>::create(cnct))
    {}

    void join(boost::asio::yield_context yield)
    {
        publisher->connect(yield);
        publisher->join(testRealm, yield);
        subscriber->connect(yield);
        subscriber->join(testRealm, yield);
        voidSubscriber->connect(yield);
        voidSubscriber->join(testRealm, yield);
    }

    void subscribe(boost::asio::yield_context yield)
    {
        using namespace std::placeholders;
        dynamicSub = subscriber->subscribe<Args>(
                "str.num",
                std::bind(&PubSubFixture::onDynamicEvent, this, _1, _2),
                yield);

        staticSub = subscriber->subscribe<std::string, int>(
                "str.num",
                std::bind(&PubSubFixture::onStaticEvent, this, _1, _2, _3),
                yield);

        voidSub = voidSubscriber->subscribe<void>(
                "void",
                std::bind(&PubSubFixture::onVoidEvent, this, _1),
                yield);
    }

    void onDynamicEvent(PublicationId pid, Args args)
    {
        INFO( "in onDynamicEvent" );
        CHECK( pid <= 9007199254740992ull );
        dynamicArgs = args;
        dynamicPubs.push_back(pid);
    }

    void onStaticEvent(PublicationId pid, std::string str, int num)
    {
        INFO( "in onStaticEvent" );
        CHECK( pid <= 9007199254740992ull );
        staticArgs = Args{{str, num}};
        staticPubs.push_back(pid);
    }

    void onVoidEvent(PublicationId pid)
    {
        INFO( "in onVoidEvent" );
        CHECK( pid <= 9007199254740992ull );
        voidPubs.push_back(pid);
    }

    CoroClient<>::Ptr publisher;
    CoroClient<>::Ptr subscriber;
    CoroClient<>::Ptr voidSubscriber;

    Subscription dynamicSub;
    Subscription staticSub;
    Subscription voidSub;

    PubVec dynamicPubs;
    PubVec staticPubs;
    PubVec voidPubs;

    Args dynamicArgs;
    Args staticArgs;
};

//------------------------------------------------------------------------------
struct RpcFixture
{
    using ClientType = CoroErrcClient<CoroClient<>>;

    RpcFixture(legacy::TcpConnector::Ptr cnct)
        : caller(ClientType::create(cnct)),
          callee(ClientType::create(cnct))
    {}

    void join(boost::asio::yield_context yield)
    {
        caller->connect(yield);
        caller->join(testRealm, yield);
        callee->connect(yield);
        callee->join(testRealm, yield);
    }

    void enroll(boost::asio::yield_context yield)
    {
        using namespace std::placeholders;
        dynamicReg = callee->enroll<Args>(
                "dynamic",
                std::bind(&RpcFixture::dynamicRpc, this, _1, _2),
                yield);

        staticReg = callee->enroll<std::string, int>(
                "static",
                std::bind(&RpcFixture::staticRpc, this, _1, _2, _3),
                yield);

        voidReg = callee->enroll<void>(
                "void",
                std::bind(&RpcFixture::voidRpc, this, _1),
                yield);
    }

    void dynamicRpc(Invocation inv, Args args)
    {
        INFO( "in RPC handler" );
        CHECK( inv.requestId() <= 9007199254740992ull );
        ++dynamicCount;
        // Echo back the call arguments as the yield result.
        inv.yield(args);
    }

    void staticRpc(Invocation inv, std::string str, int num)
    {
        INFO( "in RPC handler" );
        CHECK( inv.requestId() <= 9007199254740992ull );
        ++staticCount;
        // Echo back the call arguments as the yield result.
        inv.yield({str, num});
    }

    void voidRpc(Invocation inv)
    {
        INFO( "in RPC handler" );
        CHECK( inv.requestId() <= 9007199254740992ull );
        // Return the call count.
        inv.yield({++voidCount});
    }

    ClientType::Ptr caller;
    ClientType::Ptr callee;

    Registration dynamicReg;
    Registration staticReg;
    Registration voidReg;

    int dynamicCount = 0;
    int staticCount = 0;
    int voidCount = 0;
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
        auto client = CoroErrcClient<CoroClient<>>::create(cnct);
        client->connect(yield);
        if (joined)
            client->join(testRealm, yield);
        CHECK_THROWS_AS( throwDelegate(*client, yield), error::Wamp );
        client->disconnect();

        client->connect(yield);
        if (joined)
            client->join(testRealm, yield);
        std::error_code ec;
        errcDelegate(*client, yield, ec);
        CHECK( ec );
        if (client->state() == SessionState::established)
            CHECK( ec == WampErrc::invalidUri );
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
        auto client = CoroClient<>::create(cnct);
        client->connect(yield);
        delegate(*client, yield, completed, result);
        client->disconnect();
        CHECK( client->state() == SessionState::disconnected );
    });

    iosvc.run();
    CHECK( completed );
    CHECK_FALSE( !result.errorCode() );
    CHECK( result.errorCode() == WampErrc::sessionEnded );
    CHECK_THROWS_AS( result.get(), error::Wamp );
}

//------------------------------------------------------------------------------
void checkInvalidConnect(CoroErrcClient<CoroClient<>>::Ptr client,
                         boost::asio::yield_context yield)
{
    std::error_code ec;

    CHECK_THROWS_AS( client->connect([](AsyncResult<size_t>){}), error::Logic );
    CHECK_THROWS_AS( client->connect(yield), error::Logic );
    CHECK_THROWS_AS( client->connect(yield, ec), error::Logic );
}

void checkInvalidJoin(CoroErrcClient<CoroClient<>>::Ptr client,
                      boost::asio::yield_context yield)
{
    std::error_code ec;

    CHECK_THROWS_AS( client->join(testRealm, [](AsyncResult<SessionId>){}),
                     error::Logic );
    CHECK_THROWS_AS( client->join(testRealm, yield), error::Logic );
    CHECK_THROWS_AS( client->join(testRealm, yield, ec), error::Logic );
}

void checkInvalidLeave(CoroErrcClient<CoroClient<>>::Ptr client,
                       boost::asio::yield_context yield)
{
    std::error_code ec;

    CHECK_THROWS_AS( client->leave([](AsyncResult<std::string>){}),
                     error::Logic );
    CHECK_THROWS_AS( client->leave(yield), error::Logic );
    CHECK_THROWS_AS( client->leave(yield, ec), error::Logic );

    CHECK_THROWS_AS( client->leave("reason", [](AsyncResult<std::string>){}),
                     error::Logic );
    CHECK_THROWS_AS( client->leave("reason", yield), error::Logic );
    CHECK_THROWS_AS( client->leave("reason", yield, ec), error::Logic );
}

void checkInvalidOps(CoroErrcClient<CoroClient<>>::Ptr client,
                     boost::asio::yield_context yield)
{
    std::error_code ec;

    CHECK_THROWS_AS( client->subscribe<void>("topic", [](PublicationId){},
                                            [](AsyncResult<Subscription>){}),
                     error::Logic );
    CHECK_THROWS_AS( client->publish("topic", [](AsyncResult<PublicationId>) {}),
                     error::Logic );
    CHECK_THROWS_AS( client->publish("topic", {42},
                                    [](AsyncResult<PublicationId>) {}),
                     error::Logic );
    CHECK_THROWS_AS( client->enroll<void>("rpc", [](Invocation){},
                                         [](AsyncResult<Registration>){}),
                     error::Logic );
    CHECK_THROWS_AS( client->call("rpc", [](AsyncResult<Args>) {}),
                     error::Logic );
    CHECK_THROWS_AS( client->call("rpc", {42}, [](AsyncResult<Args>) {}),
                     error::Logic );

    CHECK_THROWS_AS( client->leave(yield), error::Logic );
    CHECK_THROWS_AS( client->leave("because", yield), error::Logic );
    CHECK_THROWS_AS( client->subscribe<void>("topic",
            [](PublicationId){}, yield), error::Logic );
    CHECK_THROWS_AS( client->publish("topic", yield), error::Logic );
    CHECK_THROWS_AS( client->publish("topic", {42}, yield),
                     error::Logic );
    CHECK_THROWS_AS( client->enroll<void>("rpc",
            [](Invocation){}, yield), error::Logic );
    CHECK_THROWS_AS( client->call("rpc", yield), error::Logic );
    CHECK_THROWS_AS( client->call("rpc", {42}, yield), error::Logic );

    CHECK_THROWS_AS( client->leave(yield, ec), error::Logic );
    CHECK_THROWS_AS( client->leave("because", yield, ec), error::Logic );
    CHECK_THROWS_AS( client->subscribe<void>("topic",
            [](PublicationId){}, yield, ec), error::Logic );
    CHECK_THROWS_AS( client->publish("topic", yield, ec), error::Logic );
    CHECK_THROWS_AS( client->publish("topic", {42}, yield, ec),
                     error::Logic );
    CHECK_THROWS_AS( client->enroll<void>("rpc",
            [](Invocation){}, yield, ec), error::Logic );
    CHECK_THROWS_AS( client->call("rpc", yield, ec), error::Logic );
    CHECK_THROWS_AS( client->call("rpc", {42}, yield, ec), error::Logic );
}

} // anonymous namespace

using legacy::TcpConnector;
using legacy::UdsConnector;

//------------------------------------------------------------------------------
SCENARIO( "Using WAMP client interface", "[WAMP]" )
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
                // Connect and disconnect a client->
                auto c = CoroClient<>::create(cnct);
                CHECK( c->state() == SessionState::disconnected );
                CHECK( c->connect(yield) == 0 );
                CHECK( c->state() == SessionState::closed );
                CHECK( c->realm().empty() );
                CHECK( c->peerInfo().empty() );
                CHECK_NOTHROW( c->disconnect() );
                CHECK( c->state() == SessionState::disconnected );
                CHECK( c->realm().empty() );
                CHECK( c->peerInfo().empty() );

                // Disconnecting again should be harmless
                CHECK_NOTHROW( c->disconnect() );
                CHECK( c->state() == SessionState::disconnected );
                CHECK( c->realm().empty() );
                CHECK( c->peerInfo().empty() );

                // Check that we can reconnect.
                CHECK( c->connect(yield) == 0 );
                CHECK( c->state() == SessionState::closed );
                CHECK( c->realm().empty() );
                CHECK( c->peerInfo().empty() );

                // Reset by letting client instance go out of scope.
            }

            // Check that another client can connect and disconnect.
            auto c2 = CoroClient<>::create(cnct);
            CHECK( c2->state() == SessionState::disconnected );
            CHECK( c2->connect(yield) == 0 );
            CHECK( c2->state() == SessionState::closed );
            CHECK( c2->realm().empty() );
            CHECK( c2->peerInfo().empty() );
            CHECK_NOTHROW( c2->disconnect() );
            CHECK( c2->state() == SessionState::disconnected );
            CHECK( c2->realm().empty() );
            CHECK( c2->peerInfo().empty() );
        });

        iosvc.run();
    }

    WHEN( "joining and leaving" )
    {
        auto c = CoroClient<>::create(cnct);
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            c->connect(yield);
            CHECK( c->state() == SessionState::closed );

            {
                // Check joining.
                SessionId sid = c->join(testRealm, yield);
                CHECK ( sid <= 9007199254740992ull );
                CHECK( c->state() == SessionState::established );
                CHECK( c->realm() == testRealm );
                Object info = c->peerInfo();
                REQUIRE( info.count("roles") );
                REQUIRE( info["roles"].is<Object>() );
                Object roles = info["roles"].as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );

                // Check leaving.
                std::string reason = c->leave(yield);
                CHECK_FALSE( reason.empty() );
                CHECK( c->state() == SessionState::closed );
                CHECK( c->realm().empty() );
                CHECK( c->peerInfo().empty() );
            }

            {
                // Check that the same client can rejoin and leave.
                SessionId sid = c->join(testRealm, yield);
                CHECK ( sid <= 9007199254740992ull );
                CHECK( c->state() == SessionState::established );
                CHECK( c->realm() == testRealm );
                Object info = c->peerInfo();
                REQUIRE( info.count("roles") );
                REQUIRE( info["roles"].is<Object>() );
                Object roles = info["roles"].as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );

                // Try leaving with a reason URI this time.
                std::string reason = c->leave("wamp.error.system_shutdown",
                                             yield);
                CHECK_FALSE( reason.empty() );
                CHECK( c->state() == SessionState::closed );
                CHECK( c->realm().empty() );
                CHECK( c->peerInfo().empty() );
            }

            CHECK_NOTHROW( c->disconnect() );
            CHECK( c->state() == SessionState::disconnected );
        });

        iosvc.run();
    }

    WHEN( "connecting, joining, leaving, and disconnecting" )
    {
        auto c = CoroClient<>::create(cnct);
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            {
                // Connect
                CHECK( c->state() == SessionState::disconnected );
                CHECK( c->connect(yield) == 0 );
                CHECK( c->state() == SessionState::closed );
                CHECK( c->realm().empty() );
                CHECK( c->peerInfo().empty() );

                // Join
                SessionId sid = c->join(testRealm, yield);
                CHECK ( sid <= 9007199254740992ull );
                CHECK( c->state() == SessionState::established );
                CHECK( c->realm() == testRealm );
                Object info = c->peerInfo();
                REQUIRE( info.count("roles") );
                REQUIRE( info["roles"].is<Object>() );
                Object roles = info["roles"].as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );

                // Leave
                std::string reason = c->leave(yield);
                CHECK_FALSE( reason.empty() );
                CHECK( c->state() == SessionState::closed );
                CHECK( c->realm().empty() );
                CHECK( c->peerInfo().empty() );

                // Disconnect
                CHECK_NOTHROW( c->disconnect() );
                CHECK( c->state() == SessionState::disconnected );
                CHECK( c->realm().empty() );
                CHECK( c->peerInfo().empty() );
                CHECK( c->peerInfo().empty() );
            }

            {
                // Connect
                CHECK( c->connect(yield) == 0 );
                CHECK( c->state() == SessionState::closed );
                CHECK( c->realm().empty() );
                CHECK( c->peerInfo().empty() );

                // Join
                SessionId sid = c->join(testRealm, yield);
                CHECK ( sid <= 9007199254740992ull );
                CHECK( c->state() == SessionState::established );
                CHECK( c->realm() == testRealm );
                Object info = c->peerInfo();
                REQUIRE( info.count("roles") );
                REQUIRE( info["roles"].is<Object>() );
                Object roles = info["roles"].as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );

                // Leave
                std::string reason = c->leave(yield);
                CHECK_FALSE( reason.empty() );
                CHECK( c->state() == SessionState::closed );
                CHECK( c->realm().empty() );
                CHECK( c->peerInfo().empty() );

                // Disconnect
                CHECK_NOTHROW( c->disconnect() );
                CHECK( c->state() == SessionState::disconnected );
                CHECK( c->realm().empty() );
                CHECK( c->peerInfo().empty() );
                CHECK( c->peerInfo().empty() );
            }
        });

        iosvc.run();
    }

    WHEN( "disconnecting during connect" )
    {
        std::error_code ec;
        auto c = Client::create(cnct);
        c->connect([&](AsyncResult<size_t> result)
        {
            ec = result.errorCode();
        });
        c->disconnect();

        iosvc.run();
        iosvc.reset();
        CHECK( ec == TransportErrc::aborted );

        c->reset();
        ec.clear();
        bool connected = false;
        c->connect([&](AsyncResult<size_t> result)
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
        auto c = CoroClient<>::create(cnct);
        bool disconnectTriggered = false;
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            try
            {
                c->connect(yield);
                disconnectTriggered = true;
                c->join(testRealm, yield);
                connected = true;
            }
            catch (const error::Wamp& e)
            {
                ec = e.code();
            }
        });

        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            while (!disconnectTriggered)
                iosvc.post(yield);
            c->disconnect();
        });

        iosvc.run();
        iosvc.reset();
        CHECK_FALSE( connected );
        CHECK( ec == WampErrc::sessionEnded );
    }

    WHEN( "resetting during connect" )
    {
        bool handlerWasInvoked = false;
        auto c = Client::create(cnct);
        c->connect([&handlerWasInvoked](AsyncResult<size_t>)
        {
            handlerWasInvoked = true;
        });
        c->reset();
        iosvc.run();

        CHECK_FALSE( handlerWasInvoked );
    }

    WHEN( "resetting during join" )
    {
        bool handlerWasInvoked = false;
        auto c = Client::create(cnct);
        c->connect([&](AsyncResult<size_t>)
        {
            c->join(testRealm, [&](AsyncResult<SessionId>)
            {
                handlerWasInvoked = true;
            });
            c->reset();
        });
        iosvc.run();

        CHECK_FALSE( handlerWasInvoked );
    }

    WHEN( "client goes out of scope during connect" )
    {
        bool handlerWasInvoked = false;

        auto client = Client::create(cnct);
        std::weak_ptr<Client> weakClient(client);

        client->connect([&handlerWasInvoked](AsyncResult<size_t>)
        {
            handlerWasInvoked = true;
        });

        // Reduce client reference count to zero
        client = nullptr;
        REQUIRE( weakClient.expired() );

        iosvc.run();

        CHECK_FALSE( handlerWasInvoked );
    }

    WHEN( "joining using a UDS transport and Msgpack serializer" )
    {
        auto cnct2 = UdsConnector::create(iosvc, testUdsPath, Msgpack::id());

        auto c = CoroClient<>::create(cnct2);
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            c->connect(yield);
            CHECK( c->state() == SessionState::closed );

            {
                // Check joining.
                SessionId sid = c->join(testRealm, yield);
                CHECK ( sid <= 9007199254740992ull );
                CHECK( c->state() == SessionState::established );
                CHECK( c->realm() == testRealm );
                Object info = c->peerInfo();
                REQUIRE( info.count("roles") );
                REQUIRE( info["roles"].is<Object>() );
                Object roles = info["roles"].as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );

                // Check leaving.
                std::string reason = c->leave(yield);
                CHECK_FALSE( reason.empty() );
                CHECK( c->state() == SessionState::closed );
                CHECK( c->realm().empty() );
                CHECK( c->peerInfo().empty() );
            }

            {
                // Check that the same client can rejoin and leave.
                SessionId sid = c->join(testRealm, yield);
                CHECK ( sid <= 9007199254740992ull );
                CHECK( c->state() == SessionState::established );
                CHECK( c->realm() == testRealm );
                Object info = c->peerInfo();
                REQUIRE( info.count("roles") );
                REQUIRE( info["roles"].is<Object>() );
                Object roles = info["roles"].as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );

                // Try leaving with a reason URI this time.
                std::string reason = c->leave("wamp.error.system_shutdown",
                                             yield);
                CHECK_FALSE( reason.empty() );
                CHECK( c->state() == SessionState::closed );
                CHECK( c->realm().empty() );
                CHECK( c->peerInfo().empty() );
            }

            CHECK_NOTHROW( c->disconnect() );
            CHECK( c->state() == SessionState::disconnected );
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
            f.publisher->publish("str.num", {"one", 1});
            pid = f.publisher->publish("str.num", {"two", 2}, yield);
            while (f.dynamicPubs.size() < 2)
                f.subscriber->suspend(yield);

            REQUIRE( f.dynamicPubs.size() == 2 );
            CHECK( f.dynamicPubs.back() == pid );
            CHECK(( f.dynamicArgs == Args{{"two", 2}} ));
            REQUIRE( f.staticPubs.size() == 2 );
            CHECK( f.staticPubs.back() == pid );
            CHECK(( f.staticArgs == Args{{"two", 2}} ));
            CHECK( f.voidPubs.empty() );

            // Check void subscription.
            f.publisher->publish("void");
            pid = f.publisher->publish("void", yield);
            while (f.voidPubs.size() < 2)
                f.voidSubscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 2 );
            CHECK( f.staticPubs.size() == 2 );
            REQUIRE( f.voidPubs.size() == 2 );
            CHECK( f.voidPubs.back() == pid );

            // Unsubscribe the dynamic subscription manually.
            f.subscriber->unsubscribe(f.dynamicSub, yield);

            // Check that the dynamic slot no longer fires, and that the
            // static slot still fires.
            pid = f.publisher->publish("str.num", {"three", 3}, yield);
            while (f.staticPubs.size() < 3)
                f.voidSubscriber->suspend(yield);
            REQUIRE( f.dynamicPubs.size() == 2 );
            REQUIRE( f.staticPubs.size() == 3 );
            CHECK( f.staticPubs.back() == pid );
            CHECK(( f.staticArgs == Args{{"three", 3}} ));

            // Unsubscribe the static subscription via RAII.
            f.staticSub = Subscription();

            // Check that the dynamic and static slots no longer fire, and
            // that the void slot still fires.
            f.publisher->publish("str.num", {"four", 4}, yield);
            pid = f.publisher->publish("void", yield);
            while (f.voidPubs.size() < 3)
                f.subscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 2 );
            CHECK( f.staticPubs.size() == 3 );
            REQUIRE( f.voidPubs.size() == 3 );
            CHECK( f.voidPubs.back() == pid );

            // Make the void subscriber leave and rejoin the session.
            f.voidSubscriber->leave(yield);
            f.voidSubscriber->join(testRealm, yield);

            // Reestablish the dynamic subscription.
            using namespace std::placeholders;
            f.dynamicSub = f.subscriber->subscribe<Args>(
                    "str.num",
                    std::bind(&PubSubFixture::onDynamicEvent, &f, _1, _2),
                    yield);

            // Check that only the dynamic slot still fires.
            f.publisher->publish("void", yield);
            pid = f.publisher->publish("str.num", {"five", 5}, yield);
            while (f.dynamicPubs.size() < 3)
                f.subscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 3 );
            CHECK( f.staticPubs.size() == 3 );
            REQUIRE( f.voidPubs.size() == 3 );
            CHECK( f.dynamicPubs.back() == pid );
            CHECK(( f.dynamicArgs == Args{{"five", 5}} ));
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
            f.dynamicSub.unsubscribe();

            // Unsubscribe the dynamic subscription again via RAII.
            f.dynamicSub = Subscription();

            // Check that the dynamic slot no longer fires, and that the
            // static slot still fires.
            pid = f.publisher->publish("str.num", {"foo", 42}, yield);
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
            // Publish to the void subscription so that we know when
            // to stop polling.
            f.publisher->publish("str.num", {"foo", 42}, yield);
            pid = f.publisher->publish("void", yield);
            while (f.voidPubs.size() < 1)
                f.voidSubscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 0 );
            CHECK( f.staticPubs.size() == 1 );
            REQUIRE( f.voidPubs.size() == 1 );
            CHECK( f.voidPubs.back() == pid );
        });

        iosvc.run();
    }

    WHEN( "unsubscribing after client is destroyed" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(cnct);
            f.join(yield);
            f.subscribe(yield);

            // Destroy the subscriber client->
            f.subscriber.reset();

            // Unsubscribe the dynamic subscription manually.
            REQUIRE_NOTHROW( f.dynamicSub.unsubscribe() );

            // Unsubscribe the static subscription via RAII.
            REQUIRE_NOTHROW( f.staticSub = Subscription() );

            // Check that the dynamic and static slots no longer fire.
            // Publish to the void subscription so that we know when
            // to stop polling.
            f.publisher->publish("str.num", {"foo", 42}, yield);
            pid = f.publisher->publish("void", yield);
            while (f.voidPubs.size() < 1)
                f.voidSubscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 0 );
            CHECK( f.staticPubs.size() == 0 );
            REQUIRE( f.voidPubs.size() == 1 );
            CHECK( f.voidPubs.back() == pid );
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
            f.subscriber->leave(yield);

            // Unsubscribe the dynamic subscription via RAII.
            REQUIRE_NOTHROW( f.dynamicSub = Subscription() );

            // Unsubscribe the static subscription manually.
            CHECK_THROWS_AS( f.subscriber->unsubscribe(f.staticSub, yield),
                             error::Logic );
            REQUIRE_NOTHROW( f.staticSub.unsubscribe() );

            // Check that the dynamic and static slots no longer fire.
            // Publish to the void subscription so that we know when
            // to stop polling.
            f.publisher->publish("str.num", {"foo", 42}, yield);
            pid = f.publisher->publish("void", yield);
            while (f.voidPubs.size() < 1)
                f.voidSubscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 0 );
            CHECK( f.staticPubs.size() == 0 );
            REQUIRE( f.voidPubs.size() == 1 );
            CHECK( f.voidPubs.back() == pid );
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
            REQUIRE_NOTHROW( f.dynamicSub.unsubscribe() );

            // Unsubscribe the static subscription via RAII.
            REQUIRE_NOTHROW( f.staticSub = Subscription() );

            // Check that the dynamic and static slots no longer fire.
            // Publish to the void subscription so that we know when
            // to stop polling.
            f.publisher->publish("str.num", {"foo", 42}, yield);
            pid = f.publisher->publish("void", yield);
            while (f.voidPubs.size() < 1)
                f.voidSubscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 0 );
            CHECK( f.staticPubs.size() == 0 );
            REQUIRE( f.voidPubs.size() == 1 );
            CHECK( f.voidPubs.back() == pid );
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
            REQUIRE_NOTHROW( f.staticSub = Subscription() );

            // Check that the dynamic and static slots no longer fire.
            // Publish to the void subscription so that we know when
            // to stop polling.
            f.publisher->publish("str.num", {"foo", 42}, yield);
            pid = f.publisher->publish("void", yield);
            while (f.voidPubs.size() < 1)
                f.voidSubscriber->suspend(yield);
            CHECK( f.dynamicPubs.size() == 0 );
            CHECK( f.staticPubs.size() == 0 );
            REQUIRE( f.voidPubs.size() == 1 );
            CHECK( f.voidPubs.back() == pid );
        });

        iosvc.run();
    }

    WHEN( "calling remote procedures taking dynamically-typed args" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            Args result;
            std::error_code ec;
            RpcFixture f(cnct);
            f.join(yield);
            f.enroll(yield);

            // Check normal RPC
            result = f.caller->call("dynamic", {"one", 1}, yield);
            CHECK( f.dynamicCount == 1 );
            CHECK(( result == Args{{"one", 1}} ));
            result = f.caller->call("dynamic", {"two", 2}, yield);
            CHECK( f.dynamicCount == 2 );
            CHECK(( result == Args{{"two", 2}} ));

            // Manually unregister the slot.
            f.callee->unregister(f.dynamicReg, yield);

            // The router should now report an error when attempting
            // to call the unregistered RPC.
            CHECK_THROWS_AS( f.caller->call("dynamic", {"three", 3}, yield),
                             error::Wamp);
            f.caller->call("dynamic", {"three", 3}, yield, ec);
            CHECK( ec == WampErrc::callError );
            CHECK( ec == WampErrc::noSuchProcedure );

            // Calling should work after re-registering the slot.
            using namespace std::placeholders;
            f.dynamicReg = f.callee->enroll<Args>(
                "dynamic",
                std::bind(&RpcFixture::dynamicRpc, &f, _1, _2),
                yield);
            result = f.caller->call("dynamic", {"four", 4}, yield);
            CHECK( f.dynamicCount == 3 );
            CHECK(( result == Args{{"four", 4}} ));
        });
        iosvc.run();
    }

    WHEN( "calling remote procedures taking statically-typed args" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            Args result;
            std::error_code ec;
            RpcFixture f(cnct);
            f.join(yield);
            f.enroll(yield);

            // Check normal RPC
            result = f.caller->call("static", {"one", 1}, yield);
            CHECK( f.staticCount == 1 );
            CHECK(( result == Args{{"one", 1}} ));

            // Extra arguments should be ignored.
            result = f.caller->call("static", {"two", 2, true}, yield);
            CHECK( f.staticCount == 2 );
            CHECK(( result == Args{{"two", 2}} ));

            // Unregister the slot via RAII.
            f.staticReg = Registration();

            // The router should now report an error when attempting
            // to call the unregistered RPC.
            CHECK_THROWS_AS( f.caller->call("static", {"three", 3}, yield),
                             error::Wamp);
            f.caller->call("static", {"three", 3}, yield, ec);
            CHECK( ec == WampErrc::callError );
            CHECK( ec == WampErrc::noSuchProcedure );

            // Calling should work after re-registering the slot.
            using namespace std::placeholders;
            f.staticReg = f.callee->enroll<std::string, int>(
                "static",
                std::bind(&RpcFixture::staticRpc, &f, _1, _2, _3),
                yield);
            result = f.caller->call("static", {"four", 4}, yield);
            CHECK( f.staticCount == 3 );
            CHECK(( result == Args{{"four", 4}} ));
        });
        iosvc.run();
    }

    WHEN( "calling remote procedures taking no arguments" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            Args result;
            std::error_code ec;
            RpcFixture f(cnct);
            f.join(yield);
            f.enroll(yield);

            // Check normal RPC
            result = f.caller->call("void", yield);
            CHECK( f.voidCount == 1 );
            CHECK(( result == Args{{1}} ));
            result = f.caller->call("void", yield);
            CHECK( f.voidCount == 2 );
            CHECK(( result == Args{{2}} ));

            // Unregister the slot manually.
            f.callee->unregister(f.voidReg, yield);

            // Unregister the RPC again via RAII.
            f.voidReg = Registration();

            // The router should now report an error when attempting
            // to call the unregistered RPC.
            CHECK_THROWS_AS( f.caller->call("void", yield), error::Wamp);
            f.caller->call("void", yield, ec);
            CHECK( ec == WampErrc::callError );
            CHECK( ec == WampErrc::noSuchProcedure );

            // Calling should work after re-registering the slot.
            using namespace std::placeholders;
            f.voidReg = f.callee->enroll<void>(
                "void",
                std::bind(&RpcFixture::voidRpc, &f, _1),
                yield);
            result = f.caller->call("void", yield);
            CHECK( f.voidCount == 3 );
            CHECK(( result == Args{{3}} ));
        });
        iosvc.run();
    }

    WHEN( "unregistering after a client is destroyed" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            std::error_code ec;
            RpcFixture f(cnct);
            f.join(yield);
            f.enroll(yield);

            // Destroy the callee client->
            f.callee.reset();

            // Manually unregister a RPC.
            REQUIRE_NOTHROW( f.dynamicReg.unregister() );

            // Unregister an RPC via RAII.
            REQUIRE_NOTHROW( f.staticReg = Registration() );

            // The router should report an error when attempting
            // to call the unregistered RPCs.
            CHECK_THROWS_AS( f.caller->call("dynamic", {"one", 1}, yield),
                             error::Wamp );
            f.caller->call("dynamic", {"one", 1}, yield, ec);
            CHECK( ec == WampErrc::noSuchProcedure );

            CHECK_THROWS_AS( f.caller->call("static", {"two", 2}, yield),
                             error::Wamp );
            f.caller->call("dynamic", {"two", 2}, yield, ec);
            CHECK( ec == WampErrc::noSuchProcedure );
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
            f.callee->leave(yield);

            // Manually unregister a RPC.
            CHECK_THROWS_AS( f.callee->unregister(f.dynamicReg, yield),
                             error::Logic );
            REQUIRE_NOTHROW( f.dynamicReg.unregister() );

            // Unregister an RPC via RAII.
            REQUIRE_NOTHROW( f.staticReg = Registration() );

            // The router should report an error when attempting
            // to call the unregistered RPCs.
            CHECK_THROWS_AS( f.caller->call("dynamic", {"one", 1}, yield),
                             error::Wamp );
            f.caller->call("dynamic", {"one", 1}, yield, ec);
            CHECK( ec == WampErrc::noSuchProcedure );

            CHECK_THROWS_AS( f.caller->call("static", {"two", 2}, yield),
                             error::Wamp );
            f.caller->call("dynamic", {"two", 2}, yield, ec);
            CHECK( ec == WampErrc::noSuchProcedure );
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
            REQUIRE_NOTHROW( f.dynamicReg.unregister() );

            // Unregister an RPC via RAII.
            REQUIRE_NOTHROW( f.staticReg = Registration() );

            // The router should report an error when attempting
            // to call the unregistered RPCs.
            CHECK_THROWS_AS( f.caller->call("dynamic", {"one", 1}, yield),
                             error::Wamp );
            f.caller->call("dynamic", {"one", 1}, yield, ec);
            CHECK( ec == WampErrc::noSuchProcedure );

            CHECK_THROWS_AS( f.caller->call("static", {"two", 2}, yield),
                             error::Wamp );
            f.caller->call("dynamic", {"two", 2}, yield, ec);
            CHECK( ec == WampErrc::noSuchProcedure );
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
            REQUIRE_NOTHROW( f.staticReg = Registration() );

            // The router should report an error when attempting
            // to call the unregistered RPCs.
            CHECK_THROWS_AS( f.caller->call("dynamic", {"one", 1}, yield),
                             error::Wamp );
            f.caller->call("dynamic", {"one", 1}, yield, ec);
            CHECK( ec == WampErrc::noSuchProcedure );

            CHECK_THROWS_AS( f.caller->call("static", {"two", 2}, yield),
                             error::Wamp );
            f.caller->call("dynamic", {"two", 2}, yield, ec);
            CHECK( ec == WampErrc::noSuchProcedure );
        });
        iosvc.run();
    }

    WHEN( "connecting to an invalid port" )
    {
        auto badCnct = TcpConnector::create(iosvc, "localhost", invalidPort,
                                            Json::id());

        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            auto client = CoroErrcClient<CoroClient<>>::create(badCnct);
            bool throws = false;
            try
            {
                client->connect(yield);
            }
            catch (const error::Wamp& e)
            {
                throws = true;
                CHECK( e.code() == TransportErrc::failed );
            }
            CHECK( throws );

            std::error_code ec;
            client->disconnect();
            client->connect(yield, ec);
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
            auto c = CoroClient<>::create(connectors);

            {
                // Connect
                CHECK( c->state() == SessionState::disconnected );
                CHECK( c->connect(yield) == 1 );
                CHECK( c->state() == SessionState::closed );
                CHECK( c->realm().empty() );
                CHECK( c->peerInfo().empty() );

                // Join
                SessionId sid = c->join(testRealm, yield);
                CHECK ( sid <= 9007199254740992ull );
                CHECK( c->state() == SessionState::established );
                CHECK( c->realm() == testRealm );
                Object info = c->peerInfo();
                REQUIRE( info.count("roles") );
                REQUIRE( info["roles"].is<Object>() );
                Object roles = info["roles"].as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );

                // Disconnect
                CHECK_NOTHROW( c->disconnect() );
                CHECK( c->state() == SessionState::disconnected );
                CHECK( c->realm().empty() );
                CHECK( c->peerInfo().empty() );
            }

            {
                // Connect
                CHECK( c->connect(yield) == 1 );
                CHECK( c->state() == SessionState::closed );
                CHECK( c->realm().empty() );
                CHECK( c->peerInfo().empty() );

                // Join
                SessionId sid = c->join(testRealm, yield);
                CHECK ( sid <= 9007199254740992ull );
                CHECK( c->state() == SessionState::established );
                CHECK( c->realm() == testRealm );
                Object info = c->peerInfo();
                REQUIRE( info.count("roles") );
                REQUIRE( info["roles"].is<Object>() );
                Object roles = info["roles"].as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );
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
            Registration reg;
            auto handler = [](Invocation) {};

            CHECK_THROWS_AS( f.callee->enroll<void>("void", handler, yield),
                             error::Wamp );
            f.callee->enroll<void>("void", handler, yield, ec);
            CHECK( ec == WampErrc::registerError );
            CHECK( ec == WampErrc::procedureAlreadyExists );
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

            auto reg = f.callee->enroll<void>(
                "rpc",
                [&callCount](Invocation inv)
                {
                    ++callCount;
                    inv.fail("wamp.error.not_authorized");
                },
                yield);

            CHECK_THROWS_AS( f.caller->call("rpc", yield), error::Wamp );
            f.caller->call("rpc", yield, ec);
            CHECK( ec == WampErrc::notAuthorized );
            CHECK( callCount == 2 );
        });
        iosvc.run();
    }

    WHEN( "yielding an invocation more than once" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            bool thrown = false;
            RpcFixture f(cnct);
            f.join(yield);
            f.enroll(yield);

            auto reg = f.callee->enroll<void>(
                "rpc",
                [&thrown](Invocation inv)
                {
                    inv.yield();
                    try
                    {
                        inv.yield();
                    }
                    catch(const error::Logic& e)
                    {
                        thrown = true;
                    }
                },
                yield);

            CHECK_NOTHROW( f.caller->call("rpc", yield) );
            CHECK( thrown );
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
            CHECK_THROWS_AS( f.caller->call("static", {42, 42}, yield),
                             error::Wamp );
            f.caller->call("static", {42, 42}, yield, ec);
            CHECK( ec == WampErrc::callError );
            CHECK( ec == WampErrc::invalidArgument );
            CHECK( f.staticCount == 0 );

            // Check insufficient arguments
            CHECK_THROWS_AS( f.caller->call("static", {42}, yield),
                             error::Wamp );
            f.caller->call("static", {42}, yield, ec);
            CHECK( ec == WampErrc::callError );
            CHECK( ec == WampErrc::invalidArgument );
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
            CHECK_NOTHROW( f.publisher->publish("str.num", {42, 42}, yield ) );

            // Publish with valid types so that we know when to stop polling.
            pid = f.publisher->publish("str.num", {"foo", 42}, yield);
            while (f.staticPubs.size() < 1)
                f.subscriber->suspend(yield);
            REQUIRE( f.staticPubs.size() == 1 );
            CHECK( f.staticPubs.back() == pid );

            // Publications with extra arguments should be handled,
            // as long as the required arguments have valid types.
            CHECK_NOTHROW( pid = f.publisher->publish("str.num",
                    {"foo", 42, true}, yield) );
            while (f.staticPubs.size() < 2)
                f.subscriber->suspend(yield);
            REQUIRE( f.staticPubs.size() == 2 );
            CHECK( f.staticPubs.back() == pid );
        });
        iosvc.run();
    }

    WHEN( "joining with an invalid realm URI" )
    {
        using ClientType = CoroErrcClient<CoroClient<>>;
        using Yield = boost::asio::yield_context;
        checkInvalidUri(
            [](ClientType& client, Yield yield)
                {client.join("#bad", yield);},
            [](ClientType& client, Yield yield, std::error_code& ec)
                {client.join("#bad", yield, ec);},
            false );
    }

    WHEN( "leaving with an invalid reason URI" )
    {
        using ClientType = CoroErrcClient<CoroClient<>>;
        using Yield = boost::asio::yield_context;
        checkInvalidUri(
            [](ClientType& client, Yield yield)
                {client.leave("#bad", yield);},
            [](ClientType& client, Yield yield, std::error_code& ec)
                {client.leave("#bad", yield, ec);} );
    }

    WHEN( "subscribing with an invalid topic URI" )
    {
        using ClientType = CoroErrcClient<CoroClient<>>;
        using Yield = boost::asio::yield_context;
        checkInvalidUri(
            [](ClientType& client, Yield yield)
                {client.subscribe<void>("#bad", [](PublicationId) {}, yield);},
            [](ClientType& client, Yield yield, std::error_code& ec)
                {client.subscribe<void>("#bad", [](PublicationId) {},
                                        yield, ec);} );
    }

    WHEN( "publishing with an invalid topic URI" )
    {
        using ClientType = CoroErrcClient<CoroClient<>>;
        using Yield = boost::asio::yield_context;
        checkInvalidUri(
            [](ClientType& client, Yield yield)
                {client.publish("#bad", yield);},
            [](ClientType& client, Yield yield, std::error_code& ec)
                {client.publish("#bad", yield, ec);} );

        AND_WHEN( "publishing with args" )
        {
            checkInvalidUri(
                [](ClientType& client, Yield yield)
                    {client.publish("#bad", {42}, yield);},
                [](ClientType& client, Yield yield, std::error_code& ec)
                    {client.publish("#bad", {42}, yield, ec);} );
        }
    }

    WHEN( "enrolling with an invalid procedure URI" )
    {
        using ClientType = CoroErrcClient<CoroClient<>>;
        using Yield = boost::asio::yield_context;
        checkInvalidUri(
            [](ClientType& client, Yield yield)
                {client.enroll<void>("#bad", [](Invocation) {}, yield);},
            [](ClientType& client, Yield yield, std::error_code& ec)
                {client.enroll<void>("#bad", [](Invocation) {},
                                     yield, ec);} );
    }

    WHEN( "calling with an invalid procedure URI" )
    {
        using ClientType = CoroErrcClient<CoroClient<>>;
        using Yield = boost::asio::yield_context;
        checkInvalidUri(
            [](ClientType& client, Yield yield)
                {client.call("#bad", yield);},
            [](ClientType& client, Yield yield, std::error_code& ec)
                {client.call("#bad", yield, ec);} );

        AND_WHEN( "calling with args" )
        {
            checkInvalidUri(
                [](ClientType& client, Yield yield)
                    {client.call("#bad", {42}, yield);},
                [](ClientType& client, Yield yield, std::error_code& ec)
                    {client.call("#bad", {42}, yield, ec);} );
        }
    }

    WHEN( "joining a non-existing realm" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            auto client = CoroErrcClient<CoroClient<>>::create(cnct);
            client->connect(yield);

            bool throws = false;
            try
            {
                client->join("nonexistent", yield);
            }
            catch (const error::Wamp& e)
            {
                throws = true;
                CHECK( e.code() == WampErrc::joinError );
                CHECK( e.code() == WampErrc::noSuchRealm );
            }
            CHECK( throws );

            std::error_code ec;
            client->join("nonexistent", yield, ec);
            CHECK( ec == WampErrc::joinError );
            CHECK( ec == WampErrc::noSuchRealm );
        });

        iosvc.run();
    }

    WHEN( "constructing a client with an empty connector list" )
    {
        CHECK_THROWS_AS( Client::create(ConnectorList{}), error::Logic );
    }

    WHEN( "using invalid operations while disconnected" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            std::error_code ec;
            auto client = CoroErrcClient<CoroClient<>>::create(cnct);
            REQUIRE( client->state() == SessionState::disconnected );
            checkInvalidJoin(client, yield);
            checkInvalidLeave(client, yield);
            checkInvalidOps(client, yield);
        });

        iosvc.run();
    }

    WHEN( "using invalid operations while connecting" )
    {
        auto client = CoroErrcClient<CoroClient<>>::create(cnct);
        client->connect( [](AsyncResult<size_t>){} );

        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            iosvc.stop();
            iosvc.reset();
            REQUIRE( client->state() == SessionState::connecting );
            checkInvalidConnect(client, yield);
            checkInvalidJoin(client, yield);
            checkInvalidLeave(client, yield);
            checkInvalidOps(client, yield);
        });

        iosvc.run();
    }

    WHEN( "using invalid operations while failed" )
    {
        auto badCnct = TcpConnector::create(iosvc, "localhost", invalidPort,
                                            Json::id());

        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            auto client = CoroErrcClient<CoroClient<>>::create(badCnct);
            CHECK_THROWS( client->connect(yield) );
            REQUIRE( client->state() == SessionState::failed );
            checkInvalidJoin(client, yield);
            checkInvalidLeave(client, yield);
            checkInvalidOps(client, yield);
        });

        iosvc.run();
    }

    WHEN( "using invalid operations while closed" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            auto client = CoroErrcClient<CoroClient<>>::create(cnct);
            client->connect(yield);
            REQUIRE( client->state() == SessionState::closed );
            checkInvalidConnect(client, yield);
            checkInvalidLeave(client, yield);
            checkInvalidOps(client, yield);
        });

        iosvc.run();
    }

    WHEN( "using invalid operations while establishing" )
    {
        auto client = CoroErrcClient<CoroClient<>>::create(cnct);
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            client->connect(yield);
        });

        iosvc.run();

        client->join(testRealm, [](AsyncResult<SessionId>){});

        AsioService iosvc2;
        boost::asio::spawn(iosvc2, [&](boost::asio::yield_context yield)
        {
            REQUIRE( client->state() == SessionState::establishing );
            checkInvalidConnect(client, yield);
            checkInvalidJoin(client, yield);
            checkInvalidLeave(client, yield);
            checkInvalidOps(client, yield);
        });

        CHECK_NOTHROW( iosvc2.run() );
    }

    WHEN( "using invalid operations while established" )
    {
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            auto client = CoroErrcClient<CoroClient<>>::create(cnct);
            client->connect(yield);
            client->join(testRealm, yield);
            REQUIRE( client->state() == SessionState::established );
            checkInvalidConnect(client, yield);
            checkInvalidJoin(client, yield);
        });

        iosvc.run();
    }

    WHEN( "using invalid operations while shutting down" )
    {
        auto client = CoroErrcClient<CoroClient<>>::create(cnct);
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            client->connect(yield);
            client->join(testRealm, yield);
            iosvc.stop();
        });
        iosvc.run();
        iosvc.reset();

        client->leave([](AsyncResult<std::string>){});

        AsioService iosvc2;
        boost::asio::spawn(iosvc2, [&](boost::asio::yield_context yield)
        {
            REQUIRE( client->state() == SessionState::shuttingDown );
            checkInvalidConnect(client, yield);
            checkInvalidJoin(client, yield);
            checkInvalidLeave(client, yield);
            checkInvalidOps(client, yield);
        });
        CHECK_NOTHROW( iosvc2.run() );
    }

    WHEN( "disconnecting during async join" )
    {
        checkDisconnect<SessionId>([](CoroClient<>& client,
                                      boost::asio::yield_context,
                                      bool& completed,
                                      AsyncResult<SessionId>& result)
        {
            client.join(testRealm, [&](AsyncResult<SessionId> sid)
            {
                completed = true;
                result = sid;
            });
        });
    }

    WHEN( "disconnecting during async leave" )
    {
        checkDisconnect<std::string>([](CoroClient<>& client,
                                        boost::asio::yield_context yield,
                                        bool& completed,
                                        AsyncResult<std::string>& result)
        {
            client.join(testRealm, yield);
            client.leave([&](AsyncResult<std::string> reason)
            {
                completed = true;
                result = reason;
            });
        });
    }

    WHEN( "disconnecting during async leave with reason" )
    {
        checkDisconnect<std::string>([](CoroClient<>& client,
                                        boost::asio::yield_context yield,
                                        bool& completed,
                                        AsyncResult<std::string>& result)
        {
            client.join(testRealm, yield);
            client.leave("because", [&](AsyncResult<std::string> reason)
            {
                completed = true;
                result = reason;
            });
        });
    }

    WHEN( "disconnecting during async subscribe" )
    {
        checkDisconnect<Subscription>([](CoroClient<>& client,
                                         boost::asio::yield_context yield,
                                         bool& completed,
                                         AsyncResult<Subscription>& result)
        {
            client.join(testRealm, yield);
            client.subscribe<void>("topic", [] (PublicationId) {},
                [&](AsyncResult<Subscription> sub)
                {
                    completed = true;
                    result = sub;
                });
        });
    }

    WHEN( "disconnecting during async unsubscribe" )
    {
        checkDisconnect<bool>([](CoroClient<>& client,
                                 boost::asio::yield_context yield,
                                 bool& completed,
                                 AsyncResult<bool>& result)
        {
            client.join(testRealm, yield);
            auto sub = client.subscribe<void>("topic", [] (PublicationId) {},
                    yield);
            sub.unsubscribe([&](AsyncResult<bool> unsubscribed)
            {
                completed = true;
                result = unsubscribed;
            });
        });
    }

    WHEN( "disconnecting during async unsubscribe via client" )
    {
        checkDisconnect<bool>([](CoroClient<>& client,
                                 boost::asio::yield_context yield,
                                 bool& completed,
                                 AsyncResult<bool>& result)
        {
            client.join(testRealm, yield);
            auto sub = client.subscribe<void>("topic", [] (PublicationId) {},
                    yield);
            client.unsubscribe(sub, [&](AsyncResult<bool> unsubscribed)
            {
                completed = true;
                result = unsubscribed;
            });
        });
    }

    WHEN( "disconnecting during async publish" )
    {
        checkDisconnect<PublicationId>([](CoroClient<>& client,
                                          boost::asio::yield_context yield,
                                          bool& completed,
                                          AsyncResult<PublicationId>& result)
        {
            client.join(testRealm, yield);
            client.publish("topic", [&](AsyncResult<PublicationId> pid)
            {
                completed = true;
                result = pid;
            });
        });
    }

    WHEN( "disconnecting during async publish with args" )
    {
        checkDisconnect<PublicationId>([](CoroClient<>& client,
                                          boost::asio::yield_context yield,
                                          bool& completed,
                                          AsyncResult<PublicationId>& result)
        {
            client.join(testRealm, yield);
            client.publish("topic", {"foo"}, [&](AsyncResult<PublicationId> pid)
            {
                completed = true;
                result = pid;
            });
        });
    }

    WHEN( "disconnecting during async enroll" )
    {
        checkDisconnect<Registration>([](CoroClient<>& client,
                                         boost::asio::yield_context yield,
                                         bool& completed,
                                         AsyncResult<Registration>& result)
        {
            client.join(testRealm, yield);
            client.enroll<void>("rpc", [] (Invocation) {},
                [&](AsyncResult<Registration> reg)
                {
                    completed = true;
                    result = reg;
                });
        });
    }

    WHEN( "disconnecting during async unregister" )
    {
        checkDisconnect<bool>([](CoroClient<>& client,
                                 boost::asio::yield_context yield,
                                 bool& completed,
                                 AsyncResult<bool>& result)
        {
            client.join(testRealm, yield);
            auto reg = client.enroll<void>("rpc", [] (Invocation) {}, yield);
            reg.unregister([&](AsyncResult<bool> unregistered)
            {
                completed = true;
                result = unregistered;
            });
        });
    }

    WHEN( "disconnecting during async unregister via client" )
    {
        checkDisconnect<bool>([](CoroClient<>& client,
                                 boost::asio::yield_context yield,
                                 bool& completed,
                                 AsyncResult<bool>& result)
        {
            client.join(testRealm, yield);
            auto reg = client.enroll<void>("rpc", [] (Invocation) {}, yield);
            client.unregister(reg, [&](AsyncResult<bool> unregistered)
            {
                completed = true;
                result = unregistered;
            });
        });
    }

    WHEN( "disconnecting during async call" )
    {
        checkDisconnect<Args>([](CoroClient<>& client,
                                 boost::asio::yield_context yield,
                                 bool& completed,
                                 AsyncResult<Args>& result)
        {
            client.join(testRealm, yield);
            client.call("rpc", [&](AsyncResult<Args> args)
            {
                completed = true;
                result = args;
            });
        });
    }

    WHEN( "disconnecting during async call with args" )
    {
        checkDisconnect<Args>([](CoroClient<>& client,
                                 boost::asio::yield_context yield,
                                 bool& completed,
                                 AsyncResult<Args>& result)
        {
            client.join(testRealm, yield);
            client.call("rpc", {"foo"}, [&](AsyncResult<Args> args)
            {
                completed = true;
                result = args;
            });
        });
    }

    WHEN( "disconnecting during async call with args" )
    {
        checkDisconnect<Args>([](CoroClient<>& client,
                                 boost::asio::yield_context yield,
                                 bool& completed,
                                 AsyncResult<Args>& result)
        {
            client.join(testRealm, yield);
            client.call("rpc", {"foo"}, [&](AsyncResult<Args> args)
            {
                completed = true;
                result = args;
            });
        });
    }

    WHEN( "issuing an asynchronous operation just before leaving" )
    {
        bool published = false;
        boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
        {
            auto c = CoroClient<>::create(cnct);
            c->connect(yield);
            c->join(testRealm, yield);
            c->publish("topic",
                      [&](AsyncResult<PublicationId>) {published = true;});
            c->leave(yield);
            CHECK( c->state() == SessionState::closed );
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

