/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#if defined(CPPWAMP_TEST_HAS_CORO)

#include <jsoncons/json_options.hpp>
#include <cppwamp/codecs/cbor.hpp>
#include "clienttesting.hpp"

using namespace test;
using namespace Catch::Matchers;

namespace
{

const std::string testUdsHost = "./udstest";

#ifdef CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS
const auto alternateTcp = UdsHost(testUdsHost).withFormat(msgpack);
#else
const auto alternateTcp = TcpHost("localhost", validPort).withFormat(msgpack);
#endif

//------------------------------------------------------------------------------
struct IncidentListener
{
    void operator()(Incident i) {list.push_back(i);}

    bool testIfEmptyThenClear()
    {
        bool isEmpty = list.empty();
        if (!isEmpty)
            UNSCOPED_INFO("Last incident: " << list.back().toLogEntry());
        list.clear();
        return isEmpty;
    }

    // Needs to be static due to the functor being stateful and
    // being copied around.
    static std::vector<Incident> list;
};

std::vector<Incident> IncidentListener::list;

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
    IncidentListener incidents;
    s.observeIncidents(incidents);
    RouterFeatures requiredFeatures{Feature::basic, Feature::basic};

    WHEN( "connecting and disconnecting" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            {
                // Connect and disconnect a session
                Session s2(ioctx);
                s2.observeIncidents(incidents);
                CHECK( s2.state() == SS::disconnected );
                s2.connect(
                    where,
                    [](ErrorOr<std::size_t> index)
                    {
                        CHECK(index == 0);
                    });
                CHECK( s2.state() == SS::connecting );

                while (s2.state() == SS::connecting)
                    suspendCoro(yield);
                CHECK( s2.state() == SS::closed );
                CHECK( incidents.testIfEmptyThenClear() );

                CHECK_NOTHROW( s2.disconnect() );
                CHECK( incidents.testIfEmptyThenClear() );
                CHECK( s2.state() == SS::disconnected );

                // Disconnecting again should be harmless
                CHECK_NOTHROW( s2.disconnect() );
                CHECK( s2.state() == SS::disconnected );
                CHECK( incidents.testIfEmptyThenClear() );

                // Check that we can reconnect.
                CHECK( s2.connect(where, yield).value() == 0 );
                CHECK( incidents.testIfEmptyThenClear() );

                // Disconnect by letting session instance go out of scope.
            }

            CHECK( incidents.testIfEmptyThenClear() );
            CHECK( s.state() == SS::disconnected );

            // Check that another client can connect and disconnect.
            s.connect(
                where,
                [](ErrorOr<std::size_t> index)
                {
                    CHECK(index == 0);
                });
            CHECK( s.state() == SS::connecting );

            while (s.state() == SS::connecting)
                suspendCoro(yield);
            CHECK( s.state() == SS::closed );
            CHECK( incidents.testIfEmptyThenClear() );

            CHECK_NOTHROW( s.disconnect() );
            CHECK( incidents.testIfEmptyThenClear() );
            CHECK( s.state() == SS::disconnected );
        });

        ioctx.run();
    }

    WHEN( "disconnecting gracefully" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            Session s(ioctx);
            s.observeIncidents(incidents);
            s.connect(where, yield).value();
            CHECK( incidents.testIfEmptyThenClear() );

            ErrorOr<bool> result;
            bool completed = false;
            s.disconnect(
                [&result, &completed](ErrorOr<bool> done)
                {
                    result = done;
                    completed = true;
                });
            CHECK( s.state() == SS::disconnecting );
            while (!completed)
                suspendCoro(yield);
            CHECK( s.state() == SS::disconnected );
            REQUIRE( result.has_value() );
            CHECK( result.value() == true );
            CHECK( incidents.testIfEmptyThenClear() );

            // Disconnecting again should be harmless
            completed = false;
            s.disconnect(
                [&result, &completed](ErrorOr<bool> done)
                {
                    result = done;
                    completed = true;
                });
            while (!completed)
                suspendCoro(yield);
            CHECK( s.state() == SS::disconnected );
            REQUIRE( result.has_value() );
            CHECK( result.value() == false );
            CHECK( incidents.testIfEmptyThenClear() );

            // Check that we can reconnect.
            CHECK( s.connect(where, yield).value() == 0 );
            CHECK( incidents.testIfEmptyThenClear() );
            s.disconnect();
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
                Welcome welcome;
                s.join(
                    testRealm,
                    [&welcome](ErrorOr<Welcome> w) {welcome = w.value();});
                CHECK(s.state() == SS::establishing);
                
                while (welcome.sessionId() == 0)
                    suspendCoro(yield);
                CHECK( s.state() == SS::established );
                CHECK( incidents.testIfEmptyThenClear() );
                
                CHECK ( welcome.sessionId() <= 9007199254740992ll );
                CHECK( welcome.realm()  == testRealm );
                Object details = welcome.options();
                REQUIRE( details.count("roles") );
                REQUIRE( details["roles"].is<Object>() );
                Object roles = welcome.optionByKey("roles").as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );
                CHECK( welcome.features().supports(requiredFeatures) );
                CHECK( welcome.features().broker().all_of(Feature::basic) );
                CHECK( welcome.features().dealer().all_of(Feature::basic) );

                // Check leaving.
                Goodbye reason;
                s.leave([&reason](ErrorOr<Goodbye> r) {reason = r.value();});
                CHECK(s.state() == SS::shuttingDown);

                while (reason.uri().empty())
                    suspendCoro(yield);
                CHECK( s.state() == SS::closed );
                CHECK( incidents.testIfEmptyThenClear() );
            }

            {
                // Check that the same client can rejoin and leave.
                Welcome welcome = s.join(testRealm, yield).value();
                CHECK( incidents.testIfEmptyThenClear() );
                CHECK( s.state() == SessionState::established );
                CHECK ( welcome.sessionId() != 0 );
                CHECK ( welcome.sessionId() <= 9007199254740992ll );
                CHECK( welcome.realm()  == testRealm );
                Object details = welcome.options();
                REQUIRE( details.count("roles") );
                REQUIRE( details["roles"].is<Object>() );
                Object roles = welcome.optionByKey("roles").as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );
                CHECK( welcome.features().supports(requiredFeatures) );

                // Try leaving with a reason URI this time.
                Goodbye reason = s.leave(Goodbye("wamp.error.system_shutdown"),
                                         yield).value();
                CHECK_FALSE( reason.uri().empty() );
                CHECK( incidents.testIfEmptyThenClear() );
            }

            CHECK_NOTHROW( s.disconnect() );
            CHECK( incidents.testIfEmptyThenClear() );
            CHECK( s.state() == SessionState::disconnected );
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
                CHECK( s.state() == SS::closed );

                // Join
                s.join(testRealm, yield).value();
                CHECK( s.state() == SS::established );

                // Leave
                Goodbye reason = s.leave(yield).value();
                CHECK_FALSE( reason.uri().empty() );
                CHECK( s.state() == SS::closed );

                // Disconnect grafully
                auto done = s.disconnect(yield).value();
                CHECK( done );
                CHECK( s.state() == SessionState::disconnected );
                CHECK( incidents.testIfEmptyThenClear() );
            }

            {
                // Connect
                CHECK( s.connect(where, yield).value() == 0 );
                CHECK( s.state() == SS::closed );

                // Join
                Welcome info = s.join(testRealm, yield).value();
                CHECK( s.state() == SS::established );
                CHECK ( info.sessionId() <= 9007199254740992ll );
                CHECK( info.realm()  == testRealm );
                Object details = info.options();
                REQUIRE( details.count("roles") );
                REQUIRE( details["roles"].is<Object>() );
                Object roles = info.optionByKey("roles").as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );
                CHECK( info.features().supports(requiredFeatures) );

                // Leave
                Goodbye reason = s.leave(yield).value();
                CHECK_FALSE( reason.uri().empty() );
                CHECK( s.state() == SS::closed );

                // Disconnect
                CHECK_NOTHROW( s.disconnect() );
                CHECK( s.state() == SessionState::disconnected );
                CHECK( incidents.testIfEmptyThenClear() );
            }
        });

        ioctx.run();
    }

    WHEN( "disconnecting abruptly uring connect" )
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

        while (s.state() != SS::connecting)
        {
            ioctx.poll();
            ioctx.restart();
        }

        s.disconnect();

        ioctx.run();
        ioctx.restart();
        CHECK( connectHandlerInvoked );
        CHECK( s.state() == SS::disconnected );

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
                s.disconnect();
            });

            ioctx.run();
            CHECK( ec == TransportErrc::success );
            CHECK( connected );
            CHECK( s.state() == SS::disconnected );
        }

        incidents.list.clear();
    }

    WHEN( "attempting to disconnect gracefully during connect" )
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
        while (s.state() != SS::connecting)
        {
            ioctx.poll();
            ioctx.restart();
        }

        ErrorOr<bool> result;
        bool completed = false;
        s.disconnect(
            [&result, &completed](ErrorOr<bool> done)
            {
                result = done;
                completed = true;
            });

        ioctx.run();
        ioctx.restart();
        CHECK( connectHandlerInvoked );
        CHECK( s.state() == SS::disconnected );
        REQUIRE( result.has_value() );
        CHECK( result == false );

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
                s.disconnect();
            });

            ioctx.run();
            CHECK( ec == TransportErrc::success );
            CHECK( connected );
            CHECK( s.state() == SS::disconnected );
        }

        incidents.list.clear();
    }

    WHEN( "disconnecting during join" )
    {
        std::error_code ec;
        bool joined = false;
        spawn(ioctx, [&](YieldContext yield)
        {
            try
            {
                s.connect(where, yield).value();
                s.join(testRealm, yield).value();
                joined = true;
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
        CHECK_FALSE( joined );
        CHECK( ec == MiscErrc::abandoned );
        CHECK( s.state() == SS::disconnected );
        CHECK( incidents.testIfEmptyThenClear() );
    }

    WHEN( "disconnecting gracefully during join" )
    {
        std::error_code ec;
        bool joined = false;
        ErrorOr<bool> disconnected;
        spawn(ioctx, [&](YieldContext yield)
        {
            try
            {
                s.connect(where, yield).value();
                s.join(testRealm, yield).value();
                joined = true;
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
            disconnected = s.disconnect(yield);
        });

        ioctx.run();
        ioctx.restart();
        CHECK_FALSE( joined );
        CHECK( ec == MiscErrc::abandoned );
        CHECK( s.state() == SS::disconnected );
        CHECK( incidents.testIfEmptyThenClear() );
        REQUIRE( disconnected.has_value() );
        CHECK( disconnected.value() == true );
    }

    WHEN( "terminating during connect" )
    {
        bool handlerWasInvoked = false;
        s.connect(where, [&handlerWasInvoked](ErrorOr<size_t>)
        {
            handlerWasInvoked = true;
        });
        while (s.state() != SS::connecting)
        {
            ioctx.poll();
            ioctx.restart();
        }
        s.terminate();
        ioctx.run();

        CHECK_FALSE( handlerWasInvoked );
        CHECK( incidents.testIfEmptyThenClear() );
        CHECK( s.state() == SS::disconnected );
    }

    WHEN( "terminating during join" )
    {
        bool handlerWasInvoked = false;
        s.connect(where, [&](ErrorOr<size_t>)
        {
            s.join(testRealm, [&](ErrorOr<Welcome>)
            {
                handlerWasInvoked = true;
            });
            s.terminate();
        });
        ioctx.run();

        CHECK_FALSE( handlerWasInvoked );
        CHECK( incidents.testIfEmptyThenClear() );
        CHECK( s.state() == SS::disconnected );
    }

    WHEN( "session goes out of scope during connect" )
    {
        bool handlerWasInvoked = false;

        {
            Session client(ioctx);
            client.observeIncidents(incidents);
            client.connect(where, [&handlerWasInvoked](ErrorOr<size_t>)
            {
                handlerWasInvoked = true;
            });
        }
        // Make client go out of scope

        ioctx.run();

        CHECK_FALSE( handlerWasInvoked );
        CHECK( incidents.testIfEmptyThenClear() );
        CHECK( s.state() == SS::disconnected );
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
    RouterFeatures requiredFeatures{Feature::basic, Feature::basic};

    WHEN( "joining and leaving" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            s.connect(where, yield).value();
            CHECK( s.state() == SessionState::closed );

            {
                // Check joining.
                Welcome info = s.join(testRealm, yield).value();
                CHECK( s.state() == SessionState::established );
                CHECK ( info.sessionId() <= 9007199254740992ll );
                CHECK( info.realm()  == testRealm );
                Object details = info.options();
                REQUIRE( details.count("roles") );
                REQUIRE( details["roles"].is<Object>() );
                Object roles = info.optionByKey("roles").as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );
                CHECK( info.features().supports(requiredFeatures) );

                // Check leaving.
                Goodbye reason = s.leave(yield).value();
                CHECK_FALSE( reason.uri().empty() );
                CHECK( s.state() == SessionState::closed );
            }

            {
                // Check that the same client can rejoin and leave.
                Welcome info = s.join(testRealm, yield).value();
                CHECK( s.state() == SessionState::established );
                CHECK ( info.sessionId() <= 9007199254740992ll );
                CHECK( info.realm()  == testRealm );
                Object details = info.options();
                REQUIRE( details.count("roles") );
                REQUIRE( details["roles"].is<Object>() );
                Object roles = info.optionByKey("roles").as<Object>();
                CHECK( roles.count("broker") );
                CHECK( roles.count("dealer") );
                CHECK( info.features().supports(requiredFeatures) );

                // Try leaving with a reason URI this time.
                Goodbye reason = s.leave(Goodbye("wamp.error.system_shutdown"),
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
SCENARIO( "Connecting with codec options", "[WAMP][Basic]" )
{
    IoContext ioctx;
    Session s(ioctx);

    jsoncons::json_options opts;
    opts.float_format(jsoncons::float_chars_format::fixed);
    opts.precision(1);
    opts.inf_to_str("inf");
    const auto where = TcpHost("localhost", validPort)
                           .withFormatOptions(JsonOptions{opts});
    double value = 0;

    auto event = [&value](Event event)
    {
        event.convertTo(value);
    };

    spawn(ioctx, [&](YieldContext yield)
    {
        s.connect(where, yield).value();
        s.join(testRealm, yield).value();
        s.subscribe(Topic("foo"), event, yield).value();

        s.publish(Pub("foo").withArgs(10.14).withExcludeMe(false));
        while (value == 0)
            suspendCoro(yield);
        CHECK_THAT(value, Catch::Matchers::WithinAbs(10.1, 0.01));
        value = 0;

        using Limits = std::numeric_limits<double>;
        s.publish(Pub("foo").withArgs(Limits::infinity()).withExcludeMe(false));
        while (value == 0)
            suspendCoro(yield);
        CHECK(std::isinf(value));

        s.disconnect();
    });

    ioctx.run();
}


//------------------------------------------------------------------------------
SCENARIO( "WAMP Connection Failures", "[WAMP][Basic]" )
{
GIVEN( "a Session, a valid ConnectionWish, and an invalid ConnectionWish" )
{
    using SS = SessionState;
    IoContext ioctx;
    Session s(ioctx);
    IncidentListener incidents;
    s.observeIncidents(incidents);
    const auto where = withTcp;
    auto badWhere = invalidTcp;

    WHEN( "connecting to an invalid port" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            auto index = s.connect(badWhere, yield);
            CHECK( index == makeUnexpected(TransportErrc::failed) );
            CHECK( incidents.testIfEmptyThenClear() );
            CHECK( s.state() == SS::failed );
        });

        ioctx.run();
        CHECK( incidents.testIfEmptyThenClear() );
    }

    WHEN( "connecting with an unsupported serializer" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            auto index = s.connect(TcpHost("localhost", validPort)
                                       .withFormat(cbor), yield);
            CHECK( index == makeUnexpected(TransportErrc::badSerializer) );
            CHECK( incidents.testIfEmptyThenClear() );
            CHECK( s.state() == SS::failed );
        });

        ioctx.run();
        CHECK( incidents.testIfEmptyThenClear() );
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
                CHECK( s.state() == SS::closed );
                REQUIRE_FALSE( incidents.list.empty() );
                const auto& incident = incidents.list.back();
                CHECK( incident.kind() == IncidentKind::trouble );
                CHECK( incident.error() == TransportErrc::failed );
                incidents.list.clear();

                // Join
                Welcome info = s.join(testRealm, yield).value();
                CHECK( incidents.testIfEmptyThenClear() );
                CHECK( s.state() == SS::established );
                CHECK ( info.sessionId() <= 9007199254740992ll );
                CHECK( info.realm()  == testRealm );
                Object details = info.options();
                REQUIRE( details.count("roles") );
                REQUIRE( details["roles"].is<Object>() );
                Object roles = info.optionByKey("roles").as<Object>();

                // Disconnect
                CHECK_NOTHROW( s.disconnect() );
                CHECK( incidents.testIfEmptyThenClear() );
                CHECK( s.state() == SS::disconnected );
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
                return session.join("#bad", yield);
            },
            false );
    }

    WHEN( "leaving with an invalid reason URI" )
    {
        checkInvalidUri(
            [](Session& session, YieldContext yield)
            {
                return session.leave(Goodbye{"#bad"}, yield);
            } );
    }

    WHEN( "joining a non-existing realm" )
    {
        spawn(ioctx, [&](YieldContext yield)
        {
            Session session(ioctx);
            session.connect(where, yield).value();
            auto result = session.join(Hello{"nonexistent"}, yield);
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
            session.join(testRealm, [&](ErrorOr<Welcome> info)
            {
                completed = true;
                result = info;
            });
        });
    }

    WHEN( "disconnecting during async leave" )
    {
        checkDisconnect<Goodbye>([](Session& session, YieldContext yield,
                                    bool& completed, ErrorOr<Goodbye>& result)
        {
            session.join(testRealm, yield).value();
            session.leave([&](ErrorOr<Goodbye> reason)
            {
                completed = true;
                result = reason;
            });
        });
    }
}}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
