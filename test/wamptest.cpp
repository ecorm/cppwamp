/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#if defined(CPPWAMP_TEST_HAS_CORO)

#include <algorithm>
#include <atomic>
#include <cctype>
#include <mutex>
#include <thread>
#include <boost/asio/thread_pool.hpp>
#include <catch2/catch.hpp>
#include <cppwamp/config.hpp>
#include <cppwamp/corounpacker.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/msgpack.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/tcp.hpp>
#include <cppwamp/unpacker.hpp>

#if CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS
    #include <cppwamp/uds.hpp>
#endif

using namespace wamp;
using namespace Catch::Matchers;

namespace
{

const std::string testRealm = "cppwamp.test";
const unsigned short validPort = 12345;
const unsigned short invalidPort = 54321;
const std::string testUdsPath = "./.crossbar/udstest";
const auto withTcp = TcpHost("localhost", validPort).withFormat(json);
const auto invalidTcp = TcpHost("localhost", invalidPort).withFormat(json);

#if CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS
const auto alternateTcp = UdsPath(testUdsPath).withFormat(msgpack);
#else
const auto alternateTcp = TcpHost("localhost", validPort).withFormat(msgpack);
#endif

//------------------------------------------------------------------------------
void suspendCoro(boost::asio::yield_context& yield)
{
    auto exec = boost::asio::get_associated_executor(yield);
    boost::asio::post(exec, yield);
}

//------------------------------------------------------------------------------
struct PubSubFixture
{
    using PubVec = std::vector<PublicationId>;

    PubSubFixture(AsioContext& ioctx, ConnectionWish wish)
        : ioctx(ioctx),
          where(std::move(wish)),
          publisher(Session::create(ioctx)),
          subscriber(Session::create(ioctx)),
          otherSubscriber(Session::create(ioctx))
    {}

    void join(boost::asio::yield_context yield)
    {
        publisher->connect(where, yield).value();
        publisher->join(Realm(testRealm), yield).value();
        subscriber->connect(where, yield).value();
        subscriber->join(Realm(testRealm), yield).value();
        otherSubscriber->connect(where, yield).value();
        otherSubscriber->join(Realm(testRealm), yield).value();
    }

    void subscribe(boost::asio::yield_context yield)
    {
        using namespace std::placeholders;
        dynamicSub = subscriber->subscribe(
                Topic("str.num"),
                std::bind(&PubSubFixture::onDynamicEvent, this, _1),
                yield).value();

        staticSub = subscriber->subscribe(
                Topic("str.num"),
                unpackedEvent<std::string, int>(
                            std::bind(&PubSubFixture::onStaticEvent, this,
                                      _1, _2, _3)),
                yield).value();

        otherSub = otherSubscriber->subscribe(
                Topic("other"),
                std::bind(&PubSubFixture::onOtherEvent, this, _1),
                yield).value();
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
    ConnectionWish where;

    Session::Ptr publisher;
    Session::Ptr subscriber;
    Session::Ptr otherSubscriber;

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
    RpcFixture(AsioContext& ioctx, ConnectionWish wish)
        : ioctx(ioctx),
          where(std::move(wish)),
          caller(Session::create(ioctx)),
          callee(Session::create(ioctx))
    {}

    void join(boost::asio::yield_context yield)
    {
        caller->connect(where, yield).value();
        caller->join(Realm(testRealm), yield).value();
        callee->connect(where, yield).value();
        callee->join(Realm(testRealm), yield).value();
    }

    void enroll(boost::asio::yield_context yield)
    {
        using namespace std::placeholders;
        dynamicReg = callee->enroll(
                Procedure("dynamic"),
                std::bind(&RpcFixture::dynamicRpc, this, _1),
                yield).value();

        staticReg = callee->enroll(
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

    AsioContext& ioctx;
    ConnectionWish where;

    Session::Ptr caller;
    Session::Ptr callee;

    ScopedRegistration dynamicReg;
    ScopedRegistration staticReg;

    int dynamicCount = 0;
    int staticCount = 0;
};

//------------------------------------------------------------------------------
template <typename TDelegate>
void checkInvalidUri(TDelegate&& delegate, bool joined = true)
{
    AsioContext ioctx;
    boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
    {
        auto session = Session::create(ioctx);
        session->connect(withTcp, yield).value();
        if (joined)
            session->join(Realm(testRealm), yield).value();
        auto result = delegate(*session, yield);
        REQUIRE( !result );
        CHECK( result.error() );
        if (session->state() == SessionState::established)
            CHECK( result.error() == SessionErrc::invalidUri );
        CHECK_THROWS_AS( result.value(), error::Failure );
        session->disconnect();
    });

    ioctx.run();
}

//------------------------------------------------------------------------------
template <typename TResult, typename TDelegate>
void checkDisconnect(TDelegate&& delegate)
{
    AsioContext ioctx;
    bool completed = false;
    ErrorOr<TResult> result;
    boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
    {
        auto session = Session::create(ioctx);
        session->connect(withTcp, yield).value();
        delegate(*session, yield, completed, result);
        session->disconnect();
        CHECK( session->state() == SessionState::disconnected );
    });

    ioctx.run();
    CHECK( completed );
    CHECK( result == makeUnexpected(SessionErrc::sessionEnded) );
    CHECK_THROWS_AS( result.value(), error::Failure );
}

//------------------------------------------------------------------------------
void checkInvalidConnect(Session::Ptr session,
                         boost::asio::yield_context yield)
{
    auto index = session->connect(withTcp, yield);
    CHECK( index == makeUnexpected(SessionErrc::invalidState) );
    CHECK_THROWS_AS( index.value(), error::Failure );
}

void checkInvalidJoin(Session::Ptr session,
                      boost::asio::yield_context yield)
{
    auto info = session->join(Realm(testRealm), yield);
    CHECK( info == makeUnexpected(SessionErrc::invalidState) );
    CHECK_THROWS_AS( session->join(Realm(testRealm), yield).value(),
                     error::Failure );
}

void checkInvalidAuthenticate(Session::Ptr session,
                              boost::asio::yield_context yield)
{
    std::string warning;
    auto exec = boost::asio::get_associated_executor(yield);
    session->setWarningHandler(
        boost::asio::bind_executor(
            exec,
            [&warning](std::string msg){warning = msg;}
    ));
    session->authenticate(Authentication("signature"));
    int tries = 0;
    while (warning.empty() && tries < 100)
    {
        suspendCoro(yield);
        ++tries;
    }
    CHECK_FALSE( warning.empty() );
}

void checkInvalidLeave(Session::Ptr session,
                       boost::asio::yield_context yield)
{
    auto reason = session->leave(yield);
    CHECK( reason == makeUnexpected(SessionErrc::invalidState) );
    CHECK_THROWS_AS( reason.value(), error::Failure );
}

void checkInvalidOps(Session::Ptr session,
                     boost::asio::yield_context yield)
{
    std::string warning;
    int tries= 0;

    auto exec = boost::asio::get_associated_executor(yield);
    session->setWarningHandler(
        boost::asio::bind_executor(
            exec,
            [&warning](std::string msg) {warning = msg;}
    ));

    session->publish(Pub("topic"));
    while (warning.empty() && tries < 100)
    {
        suspendCoro(yield);
        ++tries;
    }
    CHECK( !warning.empty() );
    warning.clear();
    session->publish(Pub("topic").withArgs(42));
    while (warning.empty() && tries < 100)
    {
        suspendCoro(yield);
        ++tries;
    }
    CHECK( !warning.empty() );
    auto pub = session->publish(Pub("topic"), yield);
    CHECK( pub == makeUnexpected(SessionErrc::invalidState) );
    CHECK_THROWS_AS( pub.value(), error::Failure );
    pub = session->publish(Pub("topic").withArgs(42), yield);
    CHECK( pub == makeUnexpected(SessionErrc::invalidState) );
    CHECK_THROWS_AS( pub.value(), error::Failure );

    auto reason = session->leave(yield);
    CHECK( reason == makeUnexpected(SessionErrc::invalidState) );
    CHECK_THROWS_AS( reason.value(), error::Failure );

    auto sub = session->subscribe(Topic("topic"), [](Event){}, yield);
    CHECK( sub == makeUnexpected(SessionErrc::invalidState) );
    CHECK_THROWS_AS( sub.value(), error::Failure );

    auto reg = session->enroll(Procedure("rpc"),
                               [](Invocation)->Outcome{return {};},
                               yield);
    CHECK( reg == makeUnexpected(SessionErrc::invalidState) );
    CHECK_THROWS_AS( reg.value(), error::Failure );

    auto result = session->call(Rpc("rpc"), yield);
    CHECK( result == makeUnexpected(SessionErrc::invalidState) );
    CHECK_THROWS_AS( result.value(), error::Failure );
    result = session->call(Rpc("rpc").withArgs(42), yield);
    CHECK( result == makeUnexpected(SessionErrc::invalidState) );
    CHECK_THROWS_AS( result.value(), error::Failure );
}

//------------------------------------------------------------------------------
struct StateChangeListener
{
    static std::vector<SessionState>& changes()
    {
        static std::vector<SessionState> theChanges;
        return theChanges;
    }

    void operator()(SessionState s)
    {
        changes().push_back(s);
    }

    void clear() {changes().clear();}

    bool empty() const {return changes().empty();}

    bool check(const Session::Ptr& session,
               const std::vector<SessionState>& expected,
               boost::asio::yield_context yield)
    {
        int triesLeft = 1000;
        while (triesLeft > 0)
        {
            if (changes().size() >= expected.size())
                break;
            suspendCoro(yield);
            --triesLeft;
        }
        CHECK( triesLeft > 0 );

        return checkNow(session, expected);
    };

    bool check(const Session::Ptr& session,
               const std::vector<SessionState>& expected,
               AsioContext& ioctx)
    {
        int triesLeft = 1000;
        while (triesLeft > 0)
        {
            if (changes().size() >= expected.size())
                break;
            ioctx.poll();
            --triesLeft;
        }
        ioctx.restart();
        CHECK( triesLeft > 0 );

        return checkNow(session, expected);
    };

    bool checkNow(const Session::Ptr& session,
                  const std::vector<SessionState>& expected)
    {
        bool isEmpty = expected.empty();
        SessionState last = {};
        if (!isEmpty)
            last = expected.back();

        bool ok = are(expected);

        if (!isEmpty)
            CHECK(session->state() == last);
        ok = ok && (session->state() == last);
        return ok;
    };

    bool are(const std::vector<SessionState>& expected)
    {
        // Workaround for Catch2 not being able to compare vectors of enums
        std::vector<int> changedInts;
        for (auto s: changes())
            changedInts.push_back(static_cast<int>(s));
        std::vector<int> expectedInts;
        for (auto s: expected)
            expectedInts.push_back(static_cast<int>(s));
        CHECK_THAT(changedInts, Catch::Matchers::Equals(expectedInts));
        changes().clear();
        return changedInts == expectedInts;
    }
};

} // anonymous namespace

//------------------------------------------------------------------------------
SCENARIO( "WAMP session management", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a TCP connector" )
{
    using SS = SessionState;
    AsioContext ioctx;
    const auto where = withTcp;
    StateChangeListener changes;

    WHEN( "connecting and disconnecting" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            {
                // Connect and disconnect a session
                auto s = Session::create(ioctx);
                s->setStateChangeHandler(changes);
                CHECK( s->state() == SS::disconnected );
                CHECK( changes.empty() );
                CHECK( s->connect(where, yield).value() == 0 );
                CHECK( changes.check(s, {SS::connecting, SS::closed}, yield) );
                CHECK_NOTHROW( s->disconnect() );
                CHECK( changes.check(s, {SS::disconnected}, yield) );

                // Disconnecting again should be harmless
                CHECK_NOTHROW( s->disconnect() );
                CHECK( s->state() == SS::disconnected );
                CHECK( changes.empty() );

                // Check that we can reconnect.
                CHECK( s->connect(where, yield).value() == 0 );
                CHECK( changes.check(s, {SS::connecting, SS::closed}, yield) );

                // Reset by letting session instance go out of scope.
            }

            // State change events should not be fired when the
            // session is destructing.
            CHECK(changes.empty());

            // Check that another client can connect and disconnect.
            auto s2 = Session::create(ioctx);
            s2->setStateChangeHandler(changes);
            CHECK( s2->state() == SS::disconnected );
            CHECK( changes.empty() );
            CHECK( s2->connect(where, yield).value() == 0 );
            CHECK( changes.check(s2, {SS::connecting, SS::closed}, yield) );
            CHECK_NOTHROW( s2->disconnect() );
            CHECK( changes.check(s2, {SS::disconnected}, yield) );
        });

        ioctx.run();
    }

    WHEN( "joining and leaving" )
    {
        auto s = Session::create(ioctx);
        s->setStateChangeHandler(changes);
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            s->connect(where, yield).value();
            CHECK( s->state() == SessionState::closed );

            {
                // Check joining.
                SessionInfo info = s->join(Realm(testRealm), yield).value();
                CHECK( changes.check(s, {SS::connecting, SS::closed,
                                         SS::establishing, SS::established},
                                     yield) );
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
                Reason reason = s->leave(yield).value();
                CHECK_FALSE( reason.uri().empty() );
                CHECK( changes.check(s, {SS::shuttingDown, SS::closed}, yield) );
            }

            {
                // Check that the same client can rejoin and leave.
                SessionInfo info = s->join(Realm(testRealm), yield).value();
                CHECK( changes.check(s, {SS::establishing, SS::established},
                                     yield) );
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
                                         yield).value();
                CHECK_FALSE( reason.uri().empty() );
                CHECK( changes.check(s, {SS::shuttingDown, SS::closed}, yield) );
            }

            CHECK_NOTHROW( s->disconnect() );
            CHECK( changes.check(s, {SS::disconnected}, yield) );
        });

        ioctx.run();
    }

    WHEN( "connecting, joining, leaving, and disconnecting" )
    {
        auto s = Session::create(ioctx);
        s->setStateChangeHandler(changes);
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            {
                // Connect
                CHECK( s->state() == SessionState::disconnected );
                CHECK( s->connect(where, yield).value() == 0 );
                CHECK( changes.check(s, {SS::connecting, SS::closed}, yield) );

                // Join
                s->join(Realm(testRealm), yield).value();
                CHECK( changes.check(s, {SS::establishing, SS::established},
                                     yield) );

                // Leave
                Reason reason = s->leave(yield).value();
                CHECK_FALSE( reason.uri().empty() );
                CHECK( changes.check(s, {SS::shuttingDown, SS::closed}, yield) );

                // Disconnect
                CHECK_NOTHROW( s->disconnect() );
                CHECK( changes.check(s, {SS::disconnected}, yield) );
            }

            {
                // Connect
                CHECK( s->connect(where, yield).value() == 0 );
                CHECK( changes.check(s, {SS::connecting, SS::closed}, yield) );

                // Join
                SessionInfo info = s->join(Realm(testRealm), yield).value();
                CHECK( changes.check(s, {SS::establishing, SS::established},
                                     yield) );
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
                Reason reason = s->leave(yield).value();
                CHECK_FALSE( reason.uri().empty() );
                CHECK( changes.check(s, {SS::shuttingDown, SS::closed}, yield) );

                // Disconnect
                CHECK_NOTHROW( s->disconnect() );
                CHECK( changes.check(s, {SS::disconnected}, yield) );
            }
        });

        ioctx.run();
    }

    WHEN( "disconnecting during connect" )
    {
        std::error_code ec;
        auto s = Session::create(ioctx);
        s->setStateChangeHandler(changes);
        bool connectHandlerInvoked = false;
        s->connect(
            ConnectionWishList{invalidTcp, where},
            [&](ErrorOr<size_t> result)
            {
                connectHandlerInvoked = true;
                if (!result)
                    ec = result.error();
            });
        s->disconnect();

        ioctx.run();
        ioctx.restart();
        CHECK( connectHandlerInvoked );
        CHECK( changes.check(s, {SS::connecting, SS::disconnected}, ioctx) );

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
            s->connect(where, [&](ErrorOr<size_t> result)
            {
                if (!result)
                    ec = result.error();
                connected = !ec;
            });

            ioctx.run();
            CHECK( ec == TransportErrc::success );
            CHECK( connected );
            CHECK( changes.check(s, {SS::connecting, SS::closed}, ioctx) );
        }
    }

    WHEN( "disconnecting during session establishment" )
    {
        std::error_code ec;
        bool connected = false;
        auto s = Session::create(ioctx);
        s->setStateChangeHandler(changes);
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            try
            {
                s->connect(where, yield).value();
                s->join(Realm(testRealm), yield).value();
                connected = true;
            }
            catch (const error::Failure& e)
            {
                ec = e.code();
            }
        });

        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            while (s->state() != SS::establishing)
                suspendCoro(yield);
            s->disconnect();
        });

        ioctx.run();
        ioctx.restart();
        CHECK_FALSE( connected );
        CHECK( ec == SessionErrc::sessionEnded );
        CHECK( changes.check(s, {SS::connecting, SS::closed,
                                 SS::establishing, SS::disconnected}, ioctx) );
    }

    WHEN( "resetting during connect" )
    {
        bool handlerWasInvoked = false;
        auto s = Session::create(ioctx);
        REQUIRE( changes.empty() );
        s->setStateChangeHandler(changes);
        s->connect(where, [&handlerWasInvoked](ErrorOr<size_t>)
        {
            handlerWasInvoked = true;
        });
        s->reset();
        ioctx.run();

        CHECK_FALSE( handlerWasInvoked );
        CHECK( changes.check(s, {SS::connecting, SS::disconnected}, ioctx) );
    }

    WHEN( "resetting during join" )
    {
        bool handlerWasInvoked = false;
        auto s = Session::create(ioctx);
        s->setStateChangeHandler(changes);
        s->connect(where, [&](ErrorOr<size_t>)
        {
            s->join(Realm(testRealm), [&](ErrorOr<SessionInfo>)
            {
                handlerWasInvoked = true;
            });
            s->reset();
        });
        ioctx.run();

        CHECK_FALSE( handlerWasInvoked );
        CHECK( changes.check(s, {SS::connecting, SS::closed,
                                 SS::establishing, SS::disconnected}, ioctx) );
    }

    WHEN( "session goes out of scope during connect" )
    {
        bool handlerWasInvoked = false;

        auto s = Session::create(ioctx);
        s->setStateChangeHandler(changes);
        std::weak_ptr<Session> weakClient(s);

        s->connect(where, [&handlerWasInvoked](ErrorOr<size_t>)
        {
            handlerWasInvoked = true;
        });

        // Reduce session reference count to zero
        s = nullptr;
        REQUIRE( weakClient.expired() );

        ioctx.run();

        CHECK_FALSE( handlerWasInvoked );
        CHECK( changes.are({SS::connecting}) );
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "Using alternate transport and/or serializer", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a TCP connector" )
{
    AsioContext ioctx;
    const auto where = alternateTcp;

    WHEN( "joining and leaving" )
    {
        auto s = Session::create(ioctx);
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            s->connect(where, yield).value();
            CHECK( s->state() == SessionState::closed );

            {
                // Check joining.
                SessionInfo info = s->join(Realm(testRealm), yield).value();
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
                Reason reason = s->leave(yield).value();
                CHECK_FALSE( reason.uri().empty() );
                CHECK( s->state() == SessionState::closed );
            }

            {
                // Check that the same client can rejoin and leave.
                SessionInfo info = s->join(Realm(testRealm), yield).value();
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
                                         yield).value();
                CHECK_FALSE( reason.uri().empty() );
                CHECK( s->state() == SessionState::closed );
            }

            CHECK_NOTHROW( s->disconnect() );
            CHECK( s->state() == SessionState::disconnected );
        });

        ioctx.run();
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "WAMP Pub-Sub", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a TCP connector" )
{
    AsioContext ioctx;
    const auto where = withTcp;

    WHEN( "publishing and subscribing" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(ioctx, where);
            f.join(yield);
            f.subscribe(yield);

            // Check dynamic and static subscriptions.
            f.publisher->publish(Pub("str.num").withArgs("one", 1));
            pid = f.publisher->publish(Pub("str.num").withArgs("two", 2),
                                       yield).value();
            while (f.dynamicPubs.size() < 2)
                suspendCoro(yield);

            REQUIRE( f.dynamicPubs.size() == 2 );
            CHECK( f.dynamicPubs.back() == pid );
            CHECK(( f.dynamicArgs == Array{"two", 2} ));
            REQUIRE( f.staticPubs.size() == 2 );
            CHECK( f.staticPubs.back() == pid );
            CHECK(( f.staticArgs == Array{"two", 2} ));
            CHECK( f.otherPubs.empty() );

            // Check subscription from another client.
            f.publisher->publish(Pub("other"));
            pid = f.publisher->publish(Pub("other"), yield).value();
            while (f.otherPubs.size() < 2)
                suspendCoro(yield);
            CHECK( f.dynamicPubs.size() == 2 );
            CHECK( f.staticPubs.size() == 2 );
            REQUIRE( f.otherPubs.size() == 2 );
            CHECK( f.otherPubs.back() == pid );

            // Unsubscribe the dynamic subscription manually.
            f.subscriber->unsubscribe(f.dynamicSub, yield).value();

            // Check that the dynamic slot no longer fires, and that the
            // static slot still fires.
            pid = f.publisher->publish(Pub("str.num").withArgs("three", 3),
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
            f.publisher->publish(Pub("str.num").withArgs("four", 4),
                                 yield).value();
            pid = f.publisher->publish(Pub("other"), yield).value();
            while (f.otherPubs.size() < 3)
                suspendCoro(yield);
            CHECK( f.dynamicPubs.size() == 2 );
            CHECK( f.staticPubs.size() == 3 );
            REQUIRE( f.otherPubs.size() == 3 );
            CHECK( f.otherPubs.back() == pid );

            // Make the "other" subscriber leave and rejoin the realm.
            f.otherSubscriber->leave(yield).value();
            f.otherSubscriber->join(Realm(testRealm), yield).value();

            // Reestablish the dynamic subscription.
            using namespace std::placeholders;
            f.dynamicSub = f.subscriber->subscribe(
                    Topic("str.num"),
                    std::bind(&PubSubFixture::onDynamicEvent, &f, _1),
                    yield).value();

            // Check that only the dynamic slot still fires.
            f.publisher->publish(Pub("other"), yield).value();
            pid = f.publisher->publish(Pub("str.num").withArgs("five", 5),
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
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            PubSubFixture f(ioctx, where);
            f.join(yield);
            f.staticSub = f.subscriber->subscribe(
                Topic("str.num"),
                simpleEvent<std::string, int>([&](std::string s, int n)
                {
                    f.staticArgs = Array{{s, n}};
                }),
                yield).value();

            f.publisher->publish(Pub("str.num").withArgs("one", 1));

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
GIVEN( "an IO service and a TCP connector" )
{
    AsioContext ioctx;
    const auto where = withTcp;

    WHEN( "unsubscribing multiple times" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
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
            pid = f.publisher->publish(Pub("str.num").withArgs("foo", 42),
                                       yield).value();
            while (f.staticPubs.size() < 1)
                suspendCoro(yield);
            REQUIRE( f.dynamicPubs.size() == 0 );
            REQUIRE( f.staticPubs.size() == 1 );
            CHECK( f.staticPubs.back() == pid );

            // Unsubscribe the static subscription manually.
            f.subscriber->unsubscribe(f.staticSub, yield).value();

            // Unsubscribe the static subscription again manually.
            f.staticSub.unsubscribe();

            // Check that the dynamic and static slots no longer fire.
            // Publish to the "other" subscription so that we know when
            // to stop polling.
            f.publisher->publish(Pub("str.num").withArgs("foo", 42),
                                 yield).value();
            pid = f.publisher->publish(Pub("other"), yield).value();
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
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(ioctx, where);
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
            f.publisher->publish(Pub("str.num").withArgs("foo", 42),
                                 yield).value();
            pid = f.publisher->publish(Pub("other"), yield).value();
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
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(ioctx, where);
            f.join(yield);
            f.subscribe(yield);

            // Make the subscriber client leave the session.
            f.subscriber->leave(yield).value();

            // Unsubscribe the dynamic subscription via RAII.
            REQUIRE_NOTHROW( f.dynamicSub = ScopedSubscription() );

            // Unsubscribe the static subscription manually.
            auto unsubscribed = f.subscriber->unsubscribe(f.staticSub, yield);
            CHECK( unsubscribed == makeUnexpected(SessionErrc::invalidState) );
            REQUIRE_NOTHROW( f.staticSub.unsubscribe() );

            // Check that the dynamic and static slots no longer fire.
            // Publish to the "other" subscription so that we know when
            // to stop polling.
            f.publisher->publish(Pub("str.num").withArgs("foo", 42),
                                 yield).value();
            pid = f.publisher->publish(Pub("other"), yield).value();
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
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(ioctx, where);
            f.join(yield);
            f.subscribe(yield);

            // Make the subscriber client disconnect.
            f.subscriber->disconnect();

            // Unsubscribe the dynamic subscription manually.
            auto unsubscribed = f.subscriber->unsubscribe(f.dynamicSub, yield);
            REQUIRE( !unsubscribed );
            CHECK( unsubscribed == makeUnexpected(SessionErrc::invalidState) );
            REQUIRE_NOTHROW( f.dynamicSub.unsubscribe() );

            // Unsubscribe the static subscription via RAII.
            REQUIRE_NOTHROW( f.staticSub= ScopedSubscription() );

            // Check that the dynamic and static slots no longer fire.
            // Publish to the "other" subscription so that we know when
            // to stop polling.
            f.publisher->publish(Pub("str.num").withArgs("foo", 42),
                                 yield).value();
            pid = f.publisher->publish(Pub("other"), yield).value();
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
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(ioctx, where);
            f.join(yield);
            f.subscribe(yield);

            // Destroy the subscriber
            f.subscriber.reset();

            // Unsubscribe the static subscription via RAII.
            REQUIRE_NOTHROW( f.staticSub = ScopedSubscription() );

            // Check that the dynamic and static slots no longer fire.
            // Publish to the "other" subscription so that we know when
            // to stop polling.
            f.publisher->publish(Pub("str.num").withArgs("foo", 42),
                                 yield).value();
            pid = f.publisher->publish(Pub("other"), yield).value();
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
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
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

                f.publisher->publish(Pub("str.num").withArgs("", 0),
                                     yield).value();
                while (f.dynamicPubs.size() < 1)
                    suspendCoro(yield);
                CHECK( f.dynamicPubs.size() == 1 );
                CHECK( f.staticPubs.size() == 1 );
            }
            // 'sub' goes out of scope here.
            f.publisher->publish(Pub("str.num").withArgs("", 0), yield).value();
            f.publisher->publish(Pub("other"), yield).value();
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

                f.publisher->publish(Pub("str.num").withArgs("", 0),
                                     yield).value();
                while (f.staticPubs.size() < 3)
                    suspendCoro(yield);
                CHECK( f.staticPubs.size() == 3 );
            }
            // 'sub' goes out of scope here.
            f.publisher->publish(Pub("str.num").withArgs("", 0), yield).value();
            f.publisher->publish(Pub("other"), yield).value();
            while (f.otherPubs.size() < 2)
                suspendCoro(yield);
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
    const auto where = withTcp;

    WHEN( "calling remote procedures taking dynamically-typed args" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            RpcFixture f(ioctx, where);
            f.join(yield);
            f.enroll(yield);

            // Check normal RPC
            Error error;
            auto result = f.caller->call(Rpc("dynamic").withArgs("one", 1)
                                         .captureError(error), yield);
            REQUIRE_FALSE(!result);
            CHECK( !error );
            CHECK( error.reason().empty() );
            CHECK( f.dynamicCount == 1 );
            CHECK(( result.value().args() == Array{"one", 1} ));
            result = f.caller->call(Rpc("dynamic").withArgs("two", 2),
                                    yield);
            REQUIRE_FALSE(!result);
            CHECK( f.dynamicCount == 2 );
            CHECK(( result.value().args() == Array{"two", 2} ));

            // Manually unregister the slot.
            f.callee->unregister(f.dynamicReg, yield).value();

            // The router should now report an error when attempting
            // to call the unregistered RPC.
            result = f.caller->call(Rpc("dynamic").withArgs("three", 3), yield);
            CHECK( result == makeUnexpected(SessionErrc::callError) );
            CHECK( result == makeUnexpected(SessionErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure);

            // Calling should work after re-registering the slot.
            using namespace std::placeholders;
            f.dynamicReg = f.callee->enroll(
                Procedure("dynamic"),
                std::bind(&RpcFixture::dynamicRpc, &f, _1),
                yield).value();
            result = f.caller->call(Rpc("dynamic").withArgs("four", 4),
                                    yield);
            REQUIRE_FALSE(!result);
            CHECK( f.dynamicCount == 3 );
            CHECK(( result.value().args() == Array{"four", 4} ));
        });
        ioctx.run();
    }

    WHEN( "calling remote procedures taking statically-typed args" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            RpcFixture f(ioctx, where);
            f.join(yield);
            f.enroll(yield);

            // Check normal RPC
            auto result = f.caller->call(Rpc("static").withArgs("one", 1),
                                         yield);
            REQUIRE_FALSE(!result);
            CHECK( f.staticCount == 1 );
            CHECK(( result.value().args() == Array{"one", 1} ));

            // Extra arguments should be ignored.
            result = f.caller->call(Rpc("static").withArgs("two", 2, true),
                                    yield);
            REQUIRE_FALSE(!result);
            CHECK( f.staticCount == 2 );
            CHECK(( result.value().args() == Array{"two", 2} ));

            // Unregister the slot via RAII.
            f.staticReg = ScopedRegistration();

            // The router should now report an error when attempting
            // to call the unregistered RPC.
            result = f.caller->call(Rpc("static").withArgs("three", 3), yield);
            CHECK( result == makeUnexpected(SessionErrc::callError) );
            CHECK( result == makeUnexpected(SessionErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure );

            // Calling should work after re-registering the slot.
            using namespace std::placeholders;
            f.staticReg = f.callee->enroll(
                Procedure("static"),
                unpackedRpc<std::string, int>(std::bind(&RpcFixture::staticRpc,
                                                        &f, _1, _2, _3)),
                yield).value();
            result = f.caller->call(Rpc("static").withArgs("four", 4), yield);
            REQUIRE_FALSE(!result);
            CHECK( f.staticCount == 3 );
            CHECK(( result.value().args() == Array{"four", 4} ));
        });
        ioctx.run();
    }

    WHEN( "calling simple remote procedures" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            RpcFixture f(ioctx, where);
            f.join(yield);

            f.staticReg = f.callee->enroll(
                    Procedure("static"),
                    simpleRpc<int, std::string, int>([&](std::string, int n)
                    {
                        ++f.staticCount;
                        return n; // Echo back the integer argument
                    }),
                    yield).value();

            // Check normal RPC
            auto result = f.caller->call(Rpc("static").withArgs("one", 1),
                                         yield);
            REQUIRE_FALSE(!result);
            CHECK( f.staticCount == 1 );
            CHECK(( result.value().args() == Array{1} ));

            // Extra arguments should be ignored.
            result = f.caller->call(Rpc("static").withArgs("two", 2, true),
                                    yield);
            REQUIRE_FALSE(!result);
            CHECK( f.staticCount == 2 );
            CHECK(( result.value().args() == Array{2} ));

            // Unregister the slot via RAII.
            f.staticReg = ScopedRegistration();

            // The router should now report an error when attempting
            // to call the unregistered RPC.
            result = f.caller->call(Rpc("static").withArgs("three", 3), yield);
            CHECK( result == makeUnexpected(SessionErrc::callError) );
            CHECK( result == makeUnexpected(SessionErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure );

            // Calling should work after re-registering the slot.
            using namespace std::placeholders;
            f.staticReg = f.callee->enroll(
                    Procedure("static"),
                    simpleRpc<int, std::string, int>([&](std::string, int n)
                    {
                        ++f.staticCount;
                        return n; // Echo back the integer argument
                    }),
                    yield).value();
            result = f.caller->call(Rpc("static").withArgs("four", 4), yield);
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
GIVEN( "an IO service and a TCP connector" )
{
    AsioContext ioctx;
    const auto where = withTcp;

    WHEN( "unregistering after a session is destroyed" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            RpcFixture f(ioctx, where);
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
            auto result = f.caller->call(Rpc("dynamic").withArgs("one", 1),
                                         yield);
            CHECK( result == makeUnexpected(SessionErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure );

            result = f.caller->call(Rpc("dynamic").withArgs("two", 2), yield);
            CHECK( result == makeUnexpected(SessionErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure );
        });
        ioctx.run();
    }

    WHEN( "unregistering after leaving" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            RpcFixture f(ioctx, where);
            f.join(yield);
            f.enroll(yield);

            // Make the callee leave the session.
            f.callee->leave(yield).value();

            // Manually unregister a RPC.
            auto unregistered = f.callee->unregister(f.dynamicReg, yield);
            CHECK( unregistered == makeUnexpected(SessionErrc::invalidState) );
            REQUIRE_NOTHROW( f.dynamicReg.unregister() );

            // Unregister an RPC via RAII.
            REQUIRE_NOTHROW( f.staticReg = ScopedRegistration() );

            // The router should report an error when attempting
            // to call the unregistered RPCs.
            auto result = f.caller->call(Rpc("dynamic").withArgs("one", 1),
                                         yield);
            CHECK( result == makeUnexpected(SessionErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure );

            result = f.caller->call(Rpc("dynamic").withArgs("two", 2), yield);
            CHECK( result == makeUnexpected(SessionErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure );
        });
        ioctx.run();
    }

    WHEN( "unregistering after disconnecting" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            RpcFixture f(ioctx, where);
            f.join(yield);
            f.enroll(yield);

            // Make the callee disconnect.
            f.callee->disconnect();

            // Manually unregister a RPC.
            auto unregistered = f.callee->unregister(f.dynamicReg, yield);
            CHECK( unregistered == makeUnexpected(SessionErrc::invalidState) );
            REQUIRE_NOTHROW( f.dynamicReg.unregister() );

            // Unregister an RPC via RAII.
            REQUIRE_NOTHROW( f.staticReg = ScopedRegistration() );

            // The router should report an error when attempting
            // to call the unregistered RPCs.
            auto result = f.caller->call(Rpc("dynamic").withArgs("one", 1),
                                         yield);
            CHECK( result == makeUnexpected(SessionErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure );

            result = f.caller->call(Rpc("dynamic").withArgs("two", 2), yield);
            CHECK( result == makeUnexpected(SessionErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure );
        });
        ioctx.run();
    }

    WHEN( "unregistering after reset" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            RpcFixture f(ioctx, where);
            f.join(yield);
            f.enroll(yield);

            // Destroy the callee.
            f.callee.reset();

            // Unregister an RPC via RAII.
            REQUIRE_NOTHROW( f.staticReg = ScopedRegistration() );

            // The router should report an error when attempting
            // to call the unregistered RPCs.
            auto result = f.caller->call(Rpc("dynamic").withArgs("one", 1),
                                         yield);
            CHECK( result == makeUnexpected(SessionErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure );

            result = f.caller->call(Rpc("dynamic").withArgs("two", 2), yield);
            CHECK( result == makeUnexpected(SessionErrc::noSuchProcedure) );
            CHECK_THROWS_AS( result.value(), error::Failure );
        });
        ioctx.run();
    }
    WHEN( "moving a ScopedRegistration" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
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

                f.caller->call(Rpc("dynamic"), yield).value();
                CHECK( f.dynamicCount == 1 );
            }
            // 'reg' goes out of scope here.
            auto result = f.caller->call(Rpc("dynamic"), yield);
            CHECK( result == makeUnexpected(SessionErrc::noSuchProcedure) );
            CHECK( f.dynamicCount == 1 );

            // Check move assignment.
            {
                ScopedRegistration reg;
                reg = std::move(f.staticReg);
                CHECK_FALSE( !reg );
                CHECK( reg.id() >= 0 );
                CHECK( !f.staticReg );

                f.caller->call(Rpc("static").withArgs("", 0), yield).value();
                CHECK( f.staticCount == 1 );
            }
            // 'reg' goes out of scope here.
            result = f.caller->call(Rpc("static").withArgs("", 0), yield);
            CHECK( result == makeUnexpected(SessionErrc::noSuchProcedure) );
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
    const auto where = withTcp;
    auto session1 = Session::create(ioctx);
    auto session2 = Session::create(ioctx);

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
                    Rpc("upperify").withArgs(str1), yield).value();
            auto upper2 = session2->call(
                    Rpc("upperify").withArgs(str2), yield).value();
            return upper1[0].to<std::string>() + upper2[0].to<std::string>();
        };

        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            session1->connect(where, yield).value();
            session1->join(Realm(testRealm), yield).value();
            session1->enroll(Procedure("upperify"),
                             unpackedRpc<std::string>(upperify), yield).value();


            session2->connect(where, yield).value();
            session2->join(Realm(testRealm), yield).value();
            session2->enroll(
                Procedure("uppercat"),
                simpleCoroRpc<std::string, std::string, std::string>(uppercat),
                yield).value();

            std::string s1 = "hello ";
            std::string s2 = "world";
            auto result = session1->call(Rpc("uppercat").withArgs(s1, s2),
                                         yield).value();
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
                                               yield).value();
                upperized = result[0].to<std::string>();
            };

        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            callee->connect(where, yield).value();
            callee->join(Realm(testRealm), yield).value();
            callee->enroll(Procedure("upperify"),
                           unpackedRpc<std::string>(upperify), yield).value();

            subscriber->connect(where, yield).value();
            subscriber->join(Realm(testRealm), yield).value();
            subscriber->subscribe(Topic("onEvent"),
                                  simpleCoroEvent<std::string>(onEvent),
                                  yield).value();

            callee->publish(Pub("onEvent").withArgs("Hello"), yield).value();
            while (upperized.empty())
                suspendCoro(yield);
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
                callee->publish(Pub("grapevine").withArgs(upper), yield).value();
                return Result({upper});
            };

        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            callee->connect(where, yield).value();
            callee->join(Realm(testRealm), yield).value();
            callee->enroll(Procedure("shout"),
                           unpackedCoroRpc<std::string>(shout), yield).value();

            subscriber->connect(where, yield).value();
            subscriber->join(Realm(testRealm), yield).value();
            subscriber->subscribe(Topic("grapevine"),
                                  unpackedEvent<std::string>(onEvent),
                                  yield).value();

            subscriber->call(Rpc("shout").withArgs("hello"), yield).value();
            while (upperized.empty())
                suspendCoro(yield);
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
            callee->unregister(reg, yield).value();
        };

        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            callee->connect(where, yield).value();
            callee->join(Realm(testRealm), yield).value();
            reg = callee->enroll(Procedure("oneShot"),
                                 simpleCoroRpc<void>(oneShot), yield).value();

            caller->connect(where, yield).value();
            caller->join(Realm(testRealm), yield).value();

            caller->call(Rpc("oneShot"), yield).value();
            while (callCount == 0)
                suspendCoro(yield);
            CHECK( callCount == 1 );

            auto result = caller->call(Rpc("oneShot"), yield);
            CHECK( result == makeUnexpected(SessionErrc::noSuchProcedure) );

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
            session1->publish(Pub("onShout").withArgs(upper), yield).value();
        };

        auto onShout = [&upperized](Event, std::string str)
        {
            upperized = str;
        };

        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            session1->connect(where, yield).value();
            session1->join(Realm(testRealm), yield).value();
            session1->subscribe(
                        Topic("onTalk"),
                        simpleCoroEvent<std::string>(onTalk), yield).value();

            session2->connect(where, yield).value();
            session2->join(Realm(testRealm), yield).value();
            session2->subscribe(
                        Topic("onShout"),
                        unpackedEvent<std::string>(onShout), yield).value();

            session2->publish(Pub("onTalk").withArgs("hello"), yield).value();
            while (upperized.empty())
                suspendCoro(yield);
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
            subscriber->unsubscribe(sub, yield).value();
        };

        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            publisher->connect(where, yield).value();
            publisher->join(Realm(testRealm), yield).value();

            subscriber->connect(where, yield).value();
            subscriber->join(Realm(testRealm), yield).value();
            sub = subscriber->subscribe(Topic("onEvent"),
                                        unpackedCoroEvent(onEvent),
                                        yield).value();

            // Dummy RPC used to end polling
            int rpcCount = 0;
            subscriber->enroll(Procedure("dummy"),
                [&rpcCount](Invocation) -> Outcome
                {
                   ++rpcCount;
                   return {};
                },
                yield).value();

            publisher->publish(Pub("onEvent"), yield).value();
            while (eventCount == 0)
                suspendCoro(yield);

            // This publish should not have any subscribers
            publisher->publish(Pub("onEvent"), yield).value();

            // Invoke dummy RPC so that we know when to stop
            publisher->call(Rpc("dummy"), yield).value();

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
    using SS = SessionState;
    AsioContext ioctx;
    const auto where = withTcp;
    const auto badWhere = invalidTcp;
    StateChangeListener changes;

    WHEN( "connecting to an invalid port" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            auto s = Session::create(ioctx);
            s->setStateChangeHandler(changes);
            auto index = s->connect(badWhere, yield);
            CHECK( index == makeUnexpected(TransportErrc::failed) );
            CHECK( changes.check(s, {SS::connecting, SS::failed}, yield) );
        });

        ioctx.run();
        CHECK( changes.empty() );
    }

    WHEN( "connecting with multiple transports" )
    {
        const ConnectionWishList wishList = {badWhere, where};

        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            auto s = Session::create(ioctx);
            s->setStateChangeHandler(changes);

            {
                // Connect
                CHECK( s->state() == SessionState::disconnected );
                CHECK( s->connect(wishList, yield).value() == 1 );
                CHECK( changes.check(s, {SS::connecting, SS::closed}, yield) );

                // Join
                SessionInfo info = s->join(Realm(testRealm), yield).value();
                CHECK( changes.check(s, {SS::establishing, SS::established},
                                     yield) );
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
                CHECK( changes.check(s, {SS::disconnected}, yield) );
            }

            {
                // Connect
                CHECK( s->connect(wishList, yield).value() == 1 );
                CHECK( s->state() == SessionState::closed );

                // Join
                SessionInfo info = s->join(Realm(testRealm), yield).value();
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
    const auto where = withTcp;

    WHEN( "registering an already existing procedure" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            RpcFixture f(ioctx, where);
            f.join(yield);
            f.enroll(yield);

            auto handler = [](Invocation)->Outcome {return {};};

            auto reg = f.callee->enroll(Procedure("dynamic"), handler, yield);
            CHECK( reg == makeUnexpected(SessionErrc::registerError) );
            CHECK( reg == makeUnexpected(SessionErrc::procedureAlreadyExists) );
            CHECK_THROWS_AS( reg.value(), error::Failure );
        });
        ioctx.run();
    }

    WHEN( "an RPC returns an error URI" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            int callCount = 0;
            RpcFixture f(ioctx, where);
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
                yield).value();

            {
                Error error;
                auto result = f.caller->call(Rpc("rpc").captureError(error),
                                             yield);
                CHECK( result == makeUnexpected(SessionErrc::notAuthorized) );
                CHECK_THROWS_AS( result.value(), error::Failure );
                CHECK_FALSE( !error );
                CHECK_THAT( error.reason(),
                            Equals("wamp.error.not_authorized") );
                CHECK( error.args() == Array{123} );
                CHECK( error.kwargs() == (Object{{{"foo"},{"bar"}}}) );
            }

            CHECK( callCount == 1 );
        });
        ioctx.run();
    }

    WHEN( "an RPC throws an error URI" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            int callCount = 0;
            RpcFixture f(ioctx, where);
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
                yield).value();

            {
                Error error;
                auto result = f.caller->call(Rpc("rpc").captureError(error),
                                             yield);
                CHECK( result == makeUnexpected(SessionErrc::notAuthorized) );
                CHECK_FALSE( !error );
                CHECK_THAT( error.reason(),
                            Equals("wamp.error.not_authorized") );
                CHECK( error.args() == Array{123} );
                CHECK( error.kwargs() == (Object{{{"foo"},{"bar"}}}) );
            }

            CHECK( callCount == 1 );
        });
        ioctx.run();
    }

    WHEN( "invoking a statically-typed RPC with invalid argument types" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            RpcFixture f(ioctx, where);
            f.join(yield);
            f.enroll(yield);

            // Check type mismatch
            auto result = f.caller->call(Rpc("static").withArgs(42, 42), yield);
            REQUIRE( !result );
            CHECK( result == makeUnexpected(SessionErrc::callError) );
            CHECK( result == makeUnexpected(SessionErrc::invalidArgument) );
            CHECK_THROWS_AS( result.value(), error::Failure );
            CHECK( f.staticCount == 0 );

            // Check insufficient arguments
            result = f.caller->call(Rpc("static").withArgs(42), yield);
            CHECK( result == makeUnexpected(SessionErrc::callError) );
            CHECK( result == makeUnexpected(SessionErrc::invalidArgument) );
            CHECK_THROWS_AS( result.value(), error::Failure );
            CHECK( f.staticCount == 0 );
        });
        ioctx.run();
    }

    WHEN( "receiving a statically-typed event with invalid argument types" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            PublicationId pid = 0;
            PubSubFixture f(ioctx, where);
            f.join(yield);
            f.subscribe(yield);

            // Publications with invalid arguments should be ignored.
            CHECK_NOTHROW( f.publisher->publish(
                               Pub("str.num").withArgs(42, 42), yield ).value() );

            // Publish with valid types so that we know when to stop polling.
            pid = f.publisher->publish(Pub("str.num").withArgs("foo", 42),
                                       yield).value();
            while (f.staticPubs.size() < 1)
                suspendCoro(yield);
            REQUIRE( f.staticPubs.size() == 1 );
            CHECK( f.staticPubs.back() == pid );

            // Publications with extra arguments should be handled,
            // as long as the required arguments have valid types.
            CHECK_NOTHROW( pid = f.publisher->publish(
                    Pub("str.num").withArgs("foo", 42, true), yield).value() );
            while (f.staticPubs.size() < 2)
                suspendCoro(yield);
            REQUIRE( f.staticPubs.size() == 2 );
            CHECK( f.staticPubs.back() == pid );
        });
        ioctx.run();
    }

    WHEN( "invoking an RPC that throws a wamp::error::BadType exceptions" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            RpcFixture f(ioctx, where);
            f.join(yield);

            f.callee->enroll(
                Procedure("bad_conversion"),
                [](Invocation inv)
                {
                    inv.args().front().to<String>();
                    return Result();
                },
                yield).value();

            f.callee->enroll(
                Procedure("bad_conv_coro"),
                simpleCoroRpc<void, Variant>(
                [](Variant v, boost::asio::yield_context yield)
                {
                    v.to<String>();
                }),
                yield).value();

            f.callee->enroll(
                Procedure("bad_access"),
                simpleRpc<void, Variant>( [](Variant v){v.as<String>();} ),
                yield).value();

            f.callee->enroll(
                Procedure("bad_access_coro"),
                unpackedCoroRpc<Variant>(
                [](Invocation inv, Variant v, boost::asio::yield_context yield)
                {
                    v.as<String>();
                    return Result();
                }),
                yield).value();


            // Check bad conversion
            auto result = f.caller->call(Rpc("bad_conversion").withArgs(42),
                                         yield);
            CHECK( result == makeUnexpected(SessionErrc::callError) );
            CHECK( result == makeUnexpected(SessionErrc::invalidArgument) );
            CHECK_THROWS_AS( result.value(), error::Failure );

            // Check bad conversion in coroutine handler
            result = f.caller->call(Rpc("bad_conv_coro").withArgs(42), yield);
            CHECK( result == makeUnexpected(SessionErrc::callError) );
            CHECK( result == makeUnexpected(SessionErrc::invalidArgument) );
            CHECK_THROWS_AS( result.value(), error::Failure );

            // Check bad access
            result = f.caller->call(Rpc("bad_access").withArgs(42), yield);
            CHECK( result == makeUnexpected(SessionErrc::callError) );
            CHECK( result == makeUnexpected(SessionErrc::invalidArgument) );
            CHECK_THROWS_AS( result.value(), error::Failure );

            // Check bad access in couroutine handler
            result = f.caller->call(Rpc("bad_access_coro").withArgs(42), yield);
            CHECK( result == makeUnexpected(SessionErrc::callError) );
            CHECK( result == makeUnexpected(SessionErrc::invalidArgument) );
            CHECK_THROWS_AS( result.value(), error::Failure );
        });
        ioctx.run();
    }

    WHEN( "an event handler throws wamp::error::BadType exceptions" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            unsigned warningCount = 0;
            PubSubFixture f(ioctx, where);
            f.subscriber->setWarningHandler(
                [&warningCount](std::string){++warningCount;}
                );
            f.join(yield);
            f.subscribe(yield);

            f.subscriber->subscribe(
                Topic("bad_conversion"),
                simpleEvent<Variant>([](Variant v) {v.to<String>();}),
                yield).value();

            f.subscriber->subscribe(
                Topic("bad_access"),
                [](Event event) {event.args().front().as<String>();},
                yield).value();

            f.subscriber->subscribe(
                Topic("bad_conversion_coro"),
                simpleCoroEvent<Variant>(
                    [](Variant v, boost::asio::yield_context y)
                    {
                        v.to<String>();
                    }),
                yield).value();

            f.subscriber->subscribe(
                Topic("bad_access_coro"),
                unpackedCoroEvent<Variant>(
                    [](Event ev, Variant v, boost::asio::yield_context y)
                    {
                        v.to<String>();
                    }),
                yield).value();

            f.publisher->publish(Pub("bad_conversion").withArgs(42));
            f.publisher->publish(Pub("bad_access").withArgs(42));
            f.publisher->publish(Pub("bad_conversion_coro").withArgs(42));
            f.publisher->publish(Pub("bad_access_coro").withArgs(42));
            f.publisher->publish(Pub("other"));

            while (f.otherPubs.empty() || warningCount < 2)
                suspendCoro(yield);

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
    const auto where = withTcp;

    WHEN( "joining with an invalid realm URI" )
    {
        using Yield = boost::asio::yield_context;
        checkInvalidUri(
            [](Session& session, Yield yield)
            {
                return session.join(Realm("#bad"), yield);
            },
            false );
    }

    WHEN( "leaving with an invalid reason URI" )
    {
        using Yield = boost::asio::yield_context;
        checkInvalidUri(
            [](Session& session, Yield yield)
            {
                return session.leave(Reason("#bad"), yield);
            } );
    }

    WHEN( "subscribing with an invalid topic URI" )
    {
        using Yield = boost::asio::yield_context;
        checkInvalidUri(
            [](Session& session, Yield yield)
            {
                return session.subscribe(Topic("#bad"), [](Event) {}, yield);
            } );
    }

    WHEN( "publishing with an invalid topic URI" )
    {
        using Yield = boost::asio::yield_context;
        checkInvalidUri(
            [](Session& session, Yield yield)
            {
                return session.publish(Pub("#bad"), yield);
            } );

        AND_WHEN( "publishing with args" )
        {
            checkInvalidUri(
                [](Session& session, Yield yield)
                {
                    return session.publish(Pub("#bad").withArgs(42), yield);
                } );
        }
    }

    WHEN( "enrolling with an invalid procedure URI" )
    {
        using Yield = boost::asio::yield_context;
        checkInvalidUri(
            [](Session& session, Yield yield)
            {
                return session.enroll(Procedure("#bad"),
                                      [](Invocation)->Outcome {return {};},
                                      yield);
            }
        );
    }

    WHEN( "calling with an invalid procedure URI" )
    {
        using Yield = boost::asio::yield_context;
        checkInvalidUri(
            [](Session& session, Yield yield)
            {
                return session.call(Rpc("#bad"), yield);
            } );

        AND_WHEN( "calling with args" )
        {
            checkInvalidUri(
                [](Session& session, Yield yield)
                {
                    return session.call(Rpc("#bad").withArgs(42), yield);
                } );
        }
    }

    WHEN( "joining a non-existing realm" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            auto session = Session::create(ioctx);
            session->connect(where, yield).value();
            auto result = session->join(Realm("nonexistent"), yield);
            CHECK( result == makeUnexpected(SessionErrc::joinError) );
            CHECK( result == makeUnexpected(SessionErrc::noSuchRealm) );
            CHECK_THROWS_AS( result.value(), error::Failure );
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
    const auto where = withTcp;

    WHEN( "constructing a session with an empty connector list" )
    {
        CHECK_THROWS_AS( Session::create(ioctx, ConnectorList{}),
                         error::Logic );
    }

    WHEN( "using invalid operations while disconnected" )
    {
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            auto session = Session::create(ioctx);
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
        auto session = Session::create(ioctx);
        session->connect(where, [](ErrorOr<size_t>){} );

        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            ioctx.stop();
            ioctx.restart();
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
            auto session = Session::create(ioctx);
            CHECK_THROWS( session->connect(invalidTcp, yield).value() );
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
            auto session = Session::create(ioctx);
            session->connect(where, yield).value();
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
        auto session = Session::create(ioctx);
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            session->connect(where, yield).value();
        });

        ioctx.run();

        session->join(Realm(testRealm), [](ErrorOr<SessionInfo>){});

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
            auto session = Session::create(ioctx);
            session->connect(where, yield).value();
            session->join(Realm(testRealm), yield).value();
            REQUIRE( session->state() == SessionState::established );
            checkInvalidConnect(session, yield);
            checkInvalidJoin(session, yield);
            checkInvalidAuthenticate(session, yield);
        });

        ioctx.run();
    }

    WHEN( "using invalid operations while shutting down" )
    {
        auto session = Session::create(ioctx);
        boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
        {
            session->connect(where, yield).value();
            session->join(Realm(testRealm), yield).value();
            ioctx.stop();
        });
        ioctx.run();
        ioctx.restart();

        session->leave([](ErrorOr<Reason>){});

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
    const auto where = withTcp;

    WHEN( "disconnecting during async join" )
    {
        checkDisconnect<SessionInfo>([](Session& session,
                                        boost::asio::yield_context,
                                        bool& completed,
                                        ErrorOr<SessionInfo>& result)
        {
            session.join(Realm(testRealm), [&](ErrorOr<SessionInfo> info)
            {
                completed = true;
                result = info;
            });
        });
    }

    WHEN( "disconnecting during async leave" )
    {
        checkDisconnect<Reason>([](Session& session,
                                   boost::asio::yield_context yield,
                                   bool& completed,
                                   ErrorOr<Reason>& result)
        {
            session.join(Realm(testRealm), yield).value();
            session.leave([&](ErrorOr<Reason> reason)
            {
                completed = true;
                result = reason;
            });
        });
    }

    WHEN( "disconnecting during async subscribe" )
    {
        checkDisconnect<Subscription>(
                    [](Session& session,
                    boost::asio::yield_context yield,
                    bool& completed,
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
        checkDisconnect<bool>([](Session& session,
                                 boost::asio::yield_context yield,
                                 bool& completed,
                                 ErrorOr<bool>& result)
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
        checkDisconnect<bool>([](Session& session,
                                 boost::asio::yield_context yield,
                                 bool& completed,
                                 ErrorOr<bool>& result)
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
        checkDisconnect<PublicationId>([](Session& session,
                                          boost::asio::yield_context yield,
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
        checkDisconnect<PublicationId>([](Session& session,
                                          boost::asio::yield_context yield,
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

    WHEN( "disconnecting during async enroll" )
    {
        checkDisconnect<Registration>(
                    [](Session& session,
                    boost::asio::yield_context yield,
                    bool& completed,
                    ErrorOr<Registration>& result)
        {
            session.join(Realm(testRealm), yield).value();
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
        checkDisconnect<bool>([](Session& session,
                                 boost::asio::yield_context yield,
                                 bool& completed,
                                 ErrorOr<bool>& result)
        {
            session.join(Realm(testRealm), yield).value();
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
                                 boost::asio::yield_context yield,
                                 bool& completed,
                                 ErrorOr<bool>& result)
        {
            session.join(Realm(testRealm), yield).value();
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
        checkDisconnect<Result>([](Session& session,
                                   boost::asio::yield_context yield,
                                   bool& completed,
                                   ErrorOr<Result>& result)
        {
            session.join(Realm(testRealm), yield).value();
            session.call(Rpc("rpc").withArgs("foo"),
                [&](ErrorOr<Result> callResult)
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
            auto s = Session::create(ioctx);
            s->connect(where, yield).value();
            s->join(Realm(testRealm), yield).value();
            s->publish(Pub("topic"),
                       [&](ErrorOr<PublicationId>) {published = true;});
            s->leave(yield).value();
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
    const auto where = withTcp;
    auto session1 = Session::create(ioctx);
    auto session2 = Session::create(ioctx);

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
            callee->connect(where, yield).value();
            callee->join(Realm(testRealm), yield).value();
            callee->enroll(Procedure("echo"), unpackedRpc<std::string>(echo),
                           yield).value();
            callee->enroll(Procedure("trigger"), trigger, yield).value();

            subscriber->connect(where, yield).value();
            subscriber->join(Realm(testRealm), yield).value();
            subscriber->subscribe(Topic("grapevine"),
                                  unpackedEvent<std::string>(onEvent),
                                  yield).value();

            for (int i=0; i<10; ++i)
            {
                // Use async call so that it doesn't block until completion.
                subscriber->call(Rpc("trigger").withArgs("hello"),
                                 [](ErrorOr<Result>) {});

                /*  Try to get callee to send an RPC response while it's still
                    transmitting the large event payload. RawsockTransport
                    should properly enqueue the RPC response while the large
                    event payload is being transmitted. */
                while (eventString.empty())
                    subscriber->call(Rpc("echo").withArgs("hello"), yield).value();

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

//------------------------------------------------------------------------------
SCENARIO( "Using Thread Pools", "[WAMP][Basic]" )
{
GIVEN( "a thread pool execution context" )
{
    boost::asio::io_context ioctx;
    boost::asio::thread_pool pool(4);
    const auto where = withTcp;
    auto session = Session::create(pool);
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
        session->publish(
            threadSafe,
            Pub("topic").withExcludeMe(false).withArgs(n),
            [](ErrorOr<PublicationId> pubId) {pubId.value();});

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

    boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
    {
        session->connect(where, yield).value();
        session->join(testRealm, yield).value();
        session->enroll(Procedure("rpc"), rpc, yield).value();
        session->subscribe(Topic("topic"), onEvent, yield).value();
        for (unsigned i=0; i<numbers.size(); ++i)
        {
            session->call(
                Rpc("rpc").withArgs(numbers[i]),
                [&resultNumbers](ErrorOr<Result> n)
                {
                    resultNumbers.push_back(n.value().args().at(0).to<int>());
                });
        }
        while ((eventCount.load() < numbers.size()) ||
               (resultNumbers.size() < numbers.size()))
        {
            boost::asio::post(yield);
        }
        session->leave(yield).value();
        session->disconnect();

        CHECK( callWatermark > 1 );
        CHECK( eventWatermark > 1 );
        CHECK_THAT( callNumbers, Catch::Matchers::UnorderedEquals(numbers) );
        CHECK_THAT( resultNumbers, Catch::Matchers::UnorderedEquals(numbers) );
        CHECK_THAT( eventNumbers, Catch::Matchers::UnorderedEquals(numbers) );
    });

    ioctx.run();
    pool.join();
}
}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
