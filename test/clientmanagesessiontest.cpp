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

const std::string testUdsPath = "./.crossbar/udstest";

#ifdef CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS
const auto alternateTcp = UdsPath(testUdsPath).withFormat(msgpack);
#else
const auto alternateTcp = TcpHost("localhost", validPort).withFormat(msgpack);
#endif

//------------------------------------------------------------------------------
struct StateChangeListener
{
    static std::vector<SessionState>& changes()
    {
        static std::vector<SessionState> theChanges;
        return theChanges;
    }

    void operator()(SessionState s, std::error_code)
    {
        changes().push_back(s);
    }

    void clear() {changes().clear();}

    bool empty() const {return changes().empty();}

    bool check(const std::vector<SessionState>& expected, YieldContext yield)
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
        return are(expected);
    };

    bool check(const Session& session,
               const std::vector<SessionState>& expected, YieldContext yield)
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

    bool check(const Session& session,
               const std::vector<SessionState>& expected,
               IoContext& ioctx)
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

    bool checkNow(const Session& session,
                  const std::vector<SessionState>& expected)
    {
        bool isEmpty = expected.empty();
        SessionState last = {};
        if (!isEmpty)
            last = expected.back();

        bool ok = are(expected);

        if (!isEmpty)
            CHECK(session.state() == last);
        ok = ok && (session.state() == last);
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
GIVEN( "a Session and a ConnectionWish" )
{
    using SS = SessionState;
    IoContext ioctx;
    Session s(ioctx);
    const auto where = withTcp;
    StateChangeListener changes;
    s.listenStateChanged(changes);
    RouterFeatures requiredFeatures{BrokerFeatures::basic,
                                    DealerFeatures::basic};

    WHEN( "connecting and disconnecting" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            {
                // Connect and disconnect a session
                Session s2(ioctx);
                s2.listenStateChanged(changes);
                CHECK( s2.state() == SS::disconnected );
                CHECK( changes.empty() );
                CHECK( s2.connect(where, yield).value() == 0 );
                CHECK( changes.check(s2, {SS::connecting, SS::closed}, yield) );
                CHECK_NOTHROW( s2.disconnect() );
                CHECK( changes.check(s2, {SS::disconnected}, yield) );

                // Disconnecting again should be harmless
                CHECK_NOTHROW( s2.disconnect() );
                CHECK( s2.state() == SS::disconnected );
                CHECK( changes.empty() );

                // Check that we can reconnect.
                CHECK( s2.connect(where, yield).value() == 0 );
                CHECK( changes.check(s2, {SS::connecting, SS::closed}, yield) );

                // Disconnect by letting session instance go out of scope.
            }

            CHECK( changes.check({SS::disconnected}, yield) );

            // Check that another client can connect and disconnect.
            CHECK( s.state() == SS::disconnected );
            CHECK( changes.empty() );
            CHECK( s.connect(where, yield).value() == 0 );
            CHECK( changes.check(s, {SS::connecting, SS::closed}, yield) );
            CHECK_NOTHROW( s.disconnect() );
            CHECK( changes.check(s, {SS::disconnected}, yield) );
        });

        ioctx.run();
    }

    WHEN( "joining and leaving" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            s.connect(where, yield).value();
            CHECK( s.state() == SessionState::closed );

            {
                // Check joining.
                Welcome info = s.join(Realm(testRealm), yield).value();
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
                CHECK( info.features().supports(requiredFeatures) );
                CHECK( info.features().broker().all_of(BrokerFeatures::basic) );
                CHECK( info.features().dealer().all_of(DealerFeatures::basic) );

                // Check leaving.
                Reason reason = s.leave(yield).value();
                CHECK_FALSE( reason.uri().empty() );
                CHECK( changes.check(s, {SS::shuttingDown, SS::closed}, yield) );
            }

            {
                // Check that the same client can rejoin and leave.
                Welcome info = s.join(Realm(testRealm), yield).value();
                CHECK( changes.check(s, {SS::establishing, SS::established},
                                     yield) );
                CHECK( s.state() == SessionState::established );
                CHECK ( info.id() <= 9007199254740992ll );
                CHECK( info.realm()  == testRealm );
                Object details = info.options();
                REQUIRE( details.count("roles") );
                REQUIRE( details["roles"].is<Object>() );
                Object roles = info.optionByKey("roles").as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );
                CHECK( info.features().supports(requiredFeatures) );

                // Try leaving with a reason URI this time.
                Reason reason = s.leave(Reason("wamp.error.system_shutdown"),
                                         yield).value();
                CHECK_FALSE( reason.uri().empty() );
                CHECK( changes.check(s, {SS::shuttingDown, SS::closed}, yield) );
            }

            CHECK_NOTHROW( s.disconnect() );
            CHECK( changes.check(s, {SS::disconnected}, yield) );
        });

        ioctx.run();
    }

    WHEN( "connecting, joining, leaving, and disconnecting" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            {
                // Connect
                CHECK( s.state() == SessionState::disconnected );
                CHECK( s.connect(where, yield).value() == 0 );
                CHECK( changes.check(s, {SS::connecting, SS::closed}, yield) );

                // Join
                s.join(Realm(testRealm), yield).value();
                CHECK( changes.check(s, {SS::establishing, SS::established},
                                     yield) );

                // Leave
                Reason reason = s.leave(yield).value();
                CHECK_FALSE( reason.uri().empty() );
                CHECK( changes.check(s, {SS::shuttingDown, SS::closed}, yield) );

                // Disconnect
                CHECK_NOTHROW( s.disconnect() );
                CHECK( changes.check(s, {SS::disconnected}, yield) );
            }

            {
                // Connect
                CHECK( s.connect(where, yield).value() == 0 );
                CHECK( changes.check(s, {SS::connecting, SS::closed}, yield) );

                // Join
                Welcome info = s.join(Realm(testRealm), yield).value();
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
                CHECK( info.features().supports(requiredFeatures) );

                // Leave
                Reason reason = s.leave(yield).value();
                CHECK_FALSE( reason.uri().empty() );
                CHECK( changes.check(s, {SS::shuttingDown, SS::closed}, yield) );

                // Disconnect
                CHECK_NOTHROW( s.disconnect() );
                CHECK( changes.check(s, {SS::disconnected}, yield) );
            }
        });

        ioctx.run();
    }

    WHEN( "disconnecting during connect" )
    {
        std::error_code ec;
        bool connectHandlerInvoked = false;
        s.connect(
            ConnectionWishList{invalidTcp, where},
            [&](ErrorOr<size_t> result)
            {
                connectHandlerInvoked = true;
                if (!result)
                    ec = result.error();
            });
        s.disconnect();

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
            s.disconnect();
            ec.clear();
            bool connected = false;
            s.connect(where, [&](ErrorOr<size_t> result)
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
        spawn(ioctx, [&](YieldContext yield)
        {
            try
            {
                s.connect(where, yield).value();
                s.join(Realm(testRealm), yield).value();
                connected = true;
            }
            catch (const error::Failure& e)
            {
                ec = e.code();
            }
        });

        spawn(ioctx, [&](YieldContext yield)
        {
            while (s.state() != SS::establishing)
                suspendCoro(yield);
            s.disconnect();
        });

        ioctx.run();
        ioctx.restart();
        CHECK_FALSE( connected );
        CHECK( ec == Errc::abandoned );
        CHECK( changes.check(s, {SS::connecting, SS::closed,
                                 SS::establishing, SS::disconnected}, ioctx) );
    }

    WHEN( "terminating during connect" )
    {
        bool handlerWasInvoked = false;
        s.connect(where, [&handlerWasInvoked](ErrorOr<size_t>)
        {
            handlerWasInvoked = true;
        });
        s.terminate();
        ioctx.run();

        CHECK_FALSE( handlerWasInvoked );
        CHECK( changes.are({SS::connecting}) );
        CHECK( s.state() == SS::disconnected );
    }

    WHEN( "terminating during join" )
    {
        bool handlerWasInvoked = false;
        s.connect(where, [&](ErrorOr<size_t>)
        {
            s.join(Realm(testRealm), [&](ErrorOr<Welcome>)
            {
                handlerWasInvoked = true;
            });
            s.terminate();
        });
        ioctx.run();

        CHECK_FALSE( handlerWasInvoked );
        CHECK( changes.are({SS::connecting, SS::closed, SS::establishing}) );
        CHECK( s.state() == SS::disconnected );
    }

    WHEN( "session goes out of scope during connect" )
    {
        bool handlerWasInvoked = false;

        {
            Session client(ioctx);
            client.listenStateChanged(changes);
            client.connect(where, [&handlerWasInvoked](ErrorOr<size_t>)
            {
                handlerWasInvoked = true;
            });
        }
        // Make client go out of scope

        ioctx.run();

        CHECK_FALSE( handlerWasInvoked );
        CHECK( changes.are({SS::connecting, SS::disconnected}) );
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "Using alternate transport and/or serializer", "[WAMP][Basic]" )
{
GIVEN( "a Session and an alternate ConnectionWish" )
{
    IoContext ioctx;
    Session s(ioctx);
    const auto where = alternateTcp;
    RouterFeatures requiredFeatures{BrokerFeatures::basic,
                                    DealerFeatures::basic};

    WHEN( "joining and leaving" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            s.connect(where, yield).value();
            CHECK( s.state() == SessionState::closed );

            {
                // Check joining.
                Welcome info = s.join(Realm(testRealm), yield).value();
                CHECK( s.state() == SessionState::established );
                CHECK ( info.id() <= 9007199254740992ll );
                CHECK( info.realm()  == testRealm );
                Object details = info.options();
                REQUIRE( details.count("roles") );
                REQUIRE( details["roles"].is<Object>() );
                Object roles = info.optionByKey("roles").as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );
                CHECK( info.features().supports(requiredFeatures) );

                // Check leaving.
                Reason reason = s.leave(yield).value();
                CHECK_FALSE( reason.uri().empty() );
                CHECK( s.state() == SessionState::closed );
            }

            {
                // Check that the same client can rejoin and leave.
                Welcome info = s.join(Realm(testRealm), yield).value();
                CHECK( s.state() == SessionState::established );
                CHECK ( info.id() <= 9007199254740992ll );
                CHECK( info.realm()  == testRealm );
                Object details = info.options();
                REQUIRE( details.count("roles") );
                REQUIRE( details["roles"].is<Object>() );
                Object roles = info.optionByKey("roles").as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );
                CHECK( info.features().supports(requiredFeatures) );

                // Try leaving with a reason URI this time.
                Reason reason = s.leave(Reason("wamp.error.system_shutdown"),
                                         yield).value();
                CHECK_FALSE( reason.uri().empty() );
                CHECK( s.state() == SessionState::closed );
            }

            CHECK_NOTHROW( s.disconnect() );
            CHECK( s.state() == SessionState::disconnected );
        });

        ioctx.run();
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "WAMP Connection Failures", "[WAMP][Basic]" )
{
GIVEN( "a Session, a valid ConnectionWish, and an invalid ConnectionWish" )
{
    using SS = SessionState;
    IoContext ioctx;
    Session s(ioctx);
    StateChangeListener changes;
    s.listenStateChanged(changes);
    const auto where = withTcp;
    const auto badWhere = invalidTcp;

    WHEN( "connecting to an invalid port" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            auto index = s.connect(badWhere, yield);
            CHECK( index == makeUnexpected(TransportErrc::failed) );
            CHECK( changes.check(s, {SS::connecting, SS::failed}, yield) );
        });

        ioctx.run();
        CHECK( changes.empty() );
    }

    WHEN( "connecting with multiple transports" )
    {
        const ConnectionWishList wishList = {badWhere, where};

        spawn(ioctx, [&](YieldContext yield)
        {
            for (int i=0; i<2; ++i)
            {
                // Connect
                CHECK( s.state() == SessionState::disconnected );
                CHECK( s.connect(wishList, yield).value() == 1 );
                CHECK( changes.check(s, {SS::connecting, SS::closed}, yield) );

                // Join
                Welcome info = s.join(Realm(testRealm), yield).value();
                CHECK( changes.check(s, {SS::establishing, SS::established},
                                     yield) );
                CHECK ( info.id() <= 9007199254740992ll );
                CHECK( info.realm()  == testRealm );
                Object details = info.options();
                REQUIRE( details.count("roles") );
                REQUIRE( details["roles"].is<Object>() );
                Object roles = info.optionByKey("roles").as<Object>();

                // Disconnect
                CHECK_NOTHROW( s.disconnect() );
                CHECK( changes.check(s, {SS::disconnected}, yield) );
            }
        });

        ioctx.run();
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "Invalid WAMP URIs", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a ConnectionWish" )
{
    IoContext ioctx;
    const auto where = withTcp;

    WHEN( "joining with an invalid realm URI" )
    {
        checkInvalidUri(
            [](Session& session, YieldContext yield)
            {
                return session.join(Realm("#bad"), yield);
            },
            false );
    }

    WHEN( "leaving with an invalid reason URI" )
    {
        checkInvalidUri(
            [](Session& session, YieldContext yield)
            {
                return session.leave(Reason("#bad"), yield);
            } );
    }

    WHEN( "joining a non-existing realm" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            Session session(ioctx);
            session.connect(where, yield).value();
            auto result = session.join(Realm("nonexistent"), yield);
            CHECK( result == makeUnexpected(WampErrc::noSuchRealm) );
            CHECK_THROWS_AS( result.value(), error::Failure );
        });

        ioctx.run();
    }
}}


//------------------------------------------------------------------------------
SCENARIO( "WAMP Disconnect/Leave During Async Session Ops", "[WAMP][Basic]" )
{
GIVEN( "an IO service and a ConnectionWish" )
{
    IoContext ioctx;
    const auto where = withTcp;

    WHEN( "disconnecting during async join" )
    {
        checkDisconnect<Welcome>([](Session& session, YieldContext,
                                    bool& completed, ErrorOr<Welcome>& result)
        {
            session.join(Realm(testRealm), [&](ErrorOr<Welcome> info)
            {
                completed = true;
                result = info;
            });
        });
    }

    WHEN( "disconnecting during async leave" )
    {
        checkDisconnect<Reason>([](Session& session, YieldContext yield,
                                   bool& completed, ErrorOr<Reason>& result)
        {
            session.join(Realm(testRealm), yield).value();
            session.leave([&](ErrorOr<Reason> reason)
            {
                completed = true;
                result = reason;
            });
        });
    }
}}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
