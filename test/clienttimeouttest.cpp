/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#if defined(CPPWAMP_TEST_HAS_CORO)

#include <cppwamp/internal/rawsocklistener.hpp>
#include "clienttesting.hpp"
#include "mockserver.hpp"

using namespace test;
using namespace Catch::Matchers;
using internal::MockServer;

namespace
{

//------------------------------------------------------------------------------
template <typename C>
C toCommand(wamp::internal::Message&& m)
{
    return MockServer::toCommand<C>(std::move(m));
}

//------------------------------------------------------------------------------
struct IncidentListener
{
    void operator()(Incident i) {list.push_back(i);}

    bool empty()
    {
        bool isEmpty = list.empty();
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
TEST_CASE("WAMP Client Connection Timeouts", "[WAMP][Basic]")
{
    struct MockListenerConfig : internal::DefaultRawsockServerOptions
    {
        static bool mockUnresponsiveness() {return true;}
    };

    using MockListener = internal::RawsockListener<internal::TcpAcceptor,
                                                   MockListenerConfig>;
    using SS = SessionState;

    IoContext ioctx;
    auto strand = boost::asio::make_strand(ioctx);
    Session s(ioctx);
    IncidentListener incidents;
    s.observeIncidents(incidents);
    const auto where = withTcp;
    auto badWhere = invalidTcp;

    const auto tcpEndpoint = TcpEndpoint{invalidPort};
    auto lstn = MockListener::create(strand, tcpEndpoint,
                                     {KnownCodecIds::json()});
    lstn->establish( [](ErrorOr<Transporting::Ptr>) {} );

    SECTION("intermediate connection timeout")
    {
        const ConnectionWishList wishList =
            {badWhere.withTimeout(std::chrono::milliseconds(50)), where};

        spawn(ioctx, [&](YieldContext yield)
        {
            for (int i=0; i<2; ++i)
            {
                // Connect
                CHECK( s.state() == SessionState::disconnected );
                CHECK( s.connect(wishList, yield).value() == 1 );
                CHECK( incidents.empty() );
                CHECK( s.state() == SS::closed );

                // Join
                Welcome info = s.join(Petition(testRealm), yield).value();
                CHECK( incidents.empty() );
                CHECK( s.state() == SS::established );

                // Disconnect
                CHECK_NOTHROW( s.disconnect() );
                CHECK( incidents.empty() );
                CHECK( s.state() == SS::disconnected );
            }
        });

        ioctx.run();
    }

    SECTION("final connection timeout")
    {
        const ConnectionWishList wishList =
            {badWhere.withTimeout(std::chrono::milliseconds(20))};

        spawn(ioctx, [&](YieldContext yield)
        {
            for (int i=0; i<2; ++i)
            {
                // Connect
                CHECK( s.state() == SessionState::disconnected );
                auto index = s.connect(wishList, yield);
                REQUIRE_FALSE(index.has_value());
                CHECK( index.error() == TransportErrc::timeout );
                CHECK( incidents.empty() );
                CHECK( s.state() == SS::failed );

                // Disconnect
                CHECK_NOTHROW( s.disconnect() );
                CHECK( incidents.empty() );
                CHECK( s.state() == SS::disconnected );
            }
        });

        ioctx.run();
    }
}

//------------------------------------------------------------------------------
TEST_CASE("WAMP Client Command Timeouts", "[WAMP][Basic]")
{
    using SS = SessionState;
    const std::chrono::milliseconds timeout{20};
    IoContext ioctx;
    Session s{ioctx};
    // TODO
//    s.observeIncidents([](Incident i) {std::cout << i.toLogEntry() << std::endl;});
//    s.enableTracing();
    auto server = internal::MockServer::create(ioctx, invalidPort);
    server->start();

    spawn([&](YieldContext yield)
    {
        INFO("join");
        {
            s.connect(invalidTcp, yield).value();
            auto welcome = s.join(Petition{testRealm}.withTimeout(timeout),
                                  yield);
            REQUIRE_FALSE(welcome.has_value());
            CHECK(welcome.error() == WampErrc::timeout);
            CHECK(s.state() == SS::failed);
            s.disconnect();
        }

        INFO("subscribe");
        {
            server->load({{{"[2,1,{}]"}}} /* WELCOME */);
            s.connect(invalidTcp, yield).value();
            s.join(Petition{testRealm}, yield).value();
            auto sub = s.subscribe(Topic{"foo"}.withTimeout(timeout),
                                   [](Event) {}, yield);
            REQUIRE_FALSE(sub.has_value());
            CHECK(sub.error() == WampErrc::timeout);
            CHECK(s.state() == SS::failed);
            s.disconnect();
        }

        INFO("unsubscribe");
        {
            server->load({
                {{"[2,1,{}]"}}, // WELCOME
                {{"[33,1,1]"}}  // SUBSCRIBED
            });

            s.connect(invalidTcp, yield).value();
            s.join(Petition{testRealm}, yield).value();
            auto sub = s.subscribe(Topic{"foo"}.withTimeout(timeout),
                                   [](Event) {}, yield).value();
            auto done = s.unsubscribe(sub, timeout, yield);
            REQUIRE_FALSE(done.has_value());
            CHECK(done.error() == WampErrc::timeout);
            CHECK(s.state() == SS::failed);
            s.disconnect();
        }

        INFO("acked publish");
        {
            server->load({{{"[2,1,{}]"}}} /* WELCOME */);
            s.connect(invalidTcp, yield).value();
            s.join(Petition{testRealm}, yield).value();
            auto pubId = s.publish(Pub{"foo"}.withArgs(42).withTimeout(timeout),
                                   yield);
            REQUIRE_FALSE(pubId.has_value());
            CHECK(pubId.error() == WampErrc::timeout);
            CHECK(s.state() == SS::failed);
            s.disconnect();
        }

        INFO("timeout ignored for unacknowledged publish");
        {
            server->load({{{"[2,1,{}]"}}} /* WELCOME */);
            s.connect(invalidTcp, yield).value();
            s.join(Petition{testRealm}, yield).value();
            s.publish(Pub{"foo"}.withArgs(42).withTimeout(timeout));

            boost::asio::steady_timer timer{ioctx};
            timer.expires_after(2*timeout);
            timer.async_wait(yield);
            CHECK(s.state() == SS::established);

            s.disconnect();
        }

        INFO("register");
        {
            server->load({{{"[2,1,{}]"}}} /* WELCOME */);
            s.connect(invalidTcp, yield).value();
            s.join(Petition{testRealm}, yield).value();
            auto reg = s.enroll(Procedure{"foo"}.withTimeout(timeout),
                                [](Invocation) -> Outcome {return Result{};},
                                yield);
            REQUIRE_FALSE(reg.has_value());
            CHECK(reg.error() == WampErrc::timeout);
            CHECK(s.state() == SS::failed);
            s.disconnect();
        }

        INFO("unregister");
        {
            server->load({
                {{"[2,1,{}]"}}, // WELCOME
                {{"[65,1,1]"}}  // REGISTERED
            });

            s.connect(invalidTcp, yield).value();
            s.join(Petition{testRealm}, yield).value();
            auto reg = s.enroll(Procedure{"foo"},
                                [](Invocation) -> Outcome {return Result{};},
                                yield).value();
            auto done = s.unregister(reg, timeout, yield);
            REQUIRE_FALSE(done.has_value());
            CHECK(done.error() == WampErrc::timeout);
            CHECK(s.state() == SS::failed);
            s.disconnect();
        }

        ioctx.stop();
    });

    ioctx.run();
}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
