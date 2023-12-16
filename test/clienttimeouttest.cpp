/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#if defined(CPPWAMP_TEST_HAS_CORO)

#include <cppwamp/internal/rawsocklistener.hpp>
#include "clienttesting.hpp"
#include "mockwampserver.hpp"

using namespace test;
using namespace wamp::internal;
using namespace Catch::Matchers;
using internal::MockWampServer;

namespace
{

//------------------------------------------------------------------------------
template <typename C>
C toCommand(wamp::internal::Message&& m)
{
    return MockWampServer::toCommand<C>(std::move(m));
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
    using ListenerType = TcpListener;
    using SS = SessionState;

    IoContext ioctx;
    auto exec = ioctx.get_executor();
    auto strand = boost::asio::make_strand(exec);
    Session s(ioctx);
    IncidentListener incidents;
    s.observeIncidents(incidents);
    const auto where = withTcp;
    auto badWhere = invalidTcp;

    const auto tcpEndpoint = TcpEndpoint{invalidPort};
    auto lstn = std::make_shared<ListenerType>(
        exec, strand, tcpEndpoint, CodecIdSet{KnownCodecIds::json()});
    Transporting::Ptr transport;
    lstn->observe(
        [&](ListenResult result)
        {
            REQUIRE(result.ok());
            transport = lstn->take();
        });
    lstn->establish();

    auto check = [&](Timeout timeout)
    {
        {
            INFO("intermediate connection timeout");
            const ConnectionWishList wishList = {badWhere.withTimeout(timeout),
                                                 where};

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
                    CHECK(incident.kind() == IncidentKind::trouble);
                    CHECK(incident.error() == TransportErrc::timeout);
                    incidents.list.clear();

                    // Join
                    Welcome info = s.join(testRealm, yield).value();
                    CHECK( incidents.empty() );
                    CHECK( s.state() == SS::established );

                    // Disconnect
                    CHECK_NOTHROW( s.disconnect() );
                    CHECK( incidents.empty() );
                    CHECK( s.state() == SS::disconnected );
                }
            });

            ioctx.run();
            ioctx.restart();
        }

        {
            INFO("final connection timeout")
            const ConnectionWishList wishList = {badWhere.withTimeout(timeout)};

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
    };

    SECTION("explicit timeout")
    {
        check(std::chrono::milliseconds(50));
    }

    SECTION("fallback timeout")
    {
        s.setFallbackTimeout(std::chrono::milliseconds(50));
        check(unspecifiedTimeout);
    }
}

//------------------------------------------------------------------------------
TEST_CASE("WAMP Client Command Timeouts", "[WAMP][Basic]")
{
    using SS = SessionState;
    IoContext ioctx;
    Session s{ioctx};
    auto server = MockWampServer::create(ioctx.get_executor(), invalidPort);
    server->start();

    auto check = [&](Timeout timeout)
    {
        spawn([&](YieldContext yield)
        {
            INFO("join");
            {
                s.connect(invalidTcp, yield).value();
                auto welcome = s.join(Hello{testRealm}.withTimeout(timeout),
                                      yield);
                REQUIRE_FALSE(welcome.has_value());
                CHECK(welcome.error() == WampErrc::timeout);
                CHECK(s.state() == SS::failed);
                s.disconnect();
            }

            INFO("leave");
            {
                server->load({{{"[2,1,{}]"}}} /* WELCOME */);
                s.connect(invalidTcp, yield).value();
                s.join(testRealm, yield).value();
                auto goodbye = s.leave(Goodbye{}, timeout, yield);
                REQUIRE_FALSE(goodbye.has_value());
                CHECK(goodbye.error() == WampErrc::timeout);
                CHECK(s.state() == SS::failed);
                s.disconnect();
            }

            INFO("subscribe");
            {
                server->load({{{"[2,1,{}]"}}} /* WELCOME */);
                s.connect(invalidTcp, yield).value();
                s.join(testRealm, yield).value();
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
                s.join(testRealm, yield).value();
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
                s.join(testRealm, yield).value();
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
                s.join(testRealm, yield).value();
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
                s.join(testRealm, yield).value();
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
                s.join(testRealm, yield).value();
                auto reg = s.enroll(
                    "foo",
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
    };

    SECTION("explicit timeout")
    {
        check(std::chrono::milliseconds(20));
    }

    SECTION("fallback timeout")
    {
        s.setFallbackTimeout(std::chrono::milliseconds(20));
        check(unspecifiedTimeout);
    }
}

//------------------------------------------------------------------------------
TEST_CASE("WAMP Client Idle Timeouts", "[WAMP][Basic]")
{
    IoContext ioctx;
    std::vector<Incident> incidents;
    Session s1{ioctx};
    Session s2{ioctx};
    s1.setIdleTimeout(std::chrono::milliseconds(80));
    s1.observeIncidents([&incidents](Incident i) {incidents.push_back(i);});
    boost::asio::steady_timer timer{ioctx};

    auto wait = [&timer](unsigned ms, YieldContext yield)
    {
        timer.expires_after(std::chrono::milliseconds{ms});
        timer.async_wait(yield);
    };

    spawn([&](YieldContext yield)
    {
        // Idle timeout does not apply while not established
        s1.connect(withTcp, yield).value();
        wait(100, yield);

        // Idle timeout countdown starts as soon as the session is established
        s1.join(Hello{testRealm}, yield).value();
        wait(100, yield);
        REQUIRE(!incidents.empty());
        CHECK(incidents.front().kind() == IncidentKind::idleTimeout);
        incidents.clear();

        // There should be no subsequent timeouts while there is no activity
        // to restart the idle timeout countdown
        wait(100, yield);
        CHECK(incidents.empty());

        s2.connect(withTcp, yield).value();
        s2.join(Hello{testRealm}, yield).value();

        // Keep triggering activity before the idle timeout deadline
        auto sub = s1.subscribe("topic", [](Event) {}, yield).value();
        wait(60, yield);
        s1.publish(Pub{"topic"}, yield).value();
        wait(60, yield);
        s1.publish(Pub{"topic"});
        wait(60, yield);
        s2.publish(Pub{"topic"});
        wait(60, yield);
        sub.unsubscribe();
        wait(60, yield);
        auto reg = s1.enroll("rpc", [](Invocation) {return Result{};},
                             yield).value();
        wait(60, yield);
        s1.call(Rpc{"rpc"}, yield).value();
        wait(60, yield);
        s2.call(Rpc{"rpc"}, yield).value();
        wait(60, yield);
        reg.unregister();
        wait(60, yield);
        CHECK(incidents.empty());

        // Allow the idle timeout deadline to lapse
        wait(40, yield);
        REQUIRE(!incidents.empty());
        CHECK(incidents.front().kind() == IncidentKind::idleTimeout);
        incidents.clear();

        // Clearing the idle timeout cancels the current pending timeout
        s1.publish(Pub{"topic"}, yield).value();
        wait(60, yield);
        s1.setIdleTimeout(neverTimeout);
        wait(60, yield);
        CHECK(incidents.empty());

        // Setting the idle timeout triggers a new idle timeout countdown
        s1.setIdleTimeout(std::chrono::milliseconds(100));
        wait(120, yield);
        REQUIRE(!incidents.empty());
        CHECK(incidents.front().kind() == IncidentKind::idleTimeout);

        s1.disconnect();
        s2.disconnect();
        ioctx.stop();
    });

    ioctx.run();
}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
