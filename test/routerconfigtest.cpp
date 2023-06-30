/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#if defined(CPPWAMP_TEST_HAS_CORO)

#include <boost/asio/steady_timer.hpp>
#include <catch2/catch.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/spawn.hpp>
#include <cppwamp/tcp.hpp>
#include "routerfixture.hpp"

using namespace wamp;

namespace
{

const std::string testRealm = "cppwamp.test-config";
const unsigned short testPort = 12345;
const auto withTcp = TcpHost("localhost", testPort).withFormat(json);

//------------------------------------------------------------------------------
class ScopedRealm
{
public:
    ScopedRealm(Realm realm) : realm_(std::move(realm)) {}

    ~ScopedRealm() {realm_.close();}

private:
    Realm realm_;
};

} // anomymous namespace


//------------------------------------------------------------------------------
TEST_CASE( "Router call timeout forwarding config", "[WAMP][Router]" )
{
    if (!test::RouterFixture::enabled())
        return;

    auto config = RealmConfig{testRealm}.withCallTimeoutForwardingEnabled(true);
    wamp::Router& router = test::RouterFixture::instance().router();
    test::RouterLogLevelGuard logLevelGuard(router.logLevel());
    router.setLogLevel(LogLevel::error);

    IoContext ioctx;
    Session s{ioctx};
    boost::asio::steady_timer timer{ioctx};

    auto rpc = [&timer](Invocation inv) -> Outcome
    {
        auto timeout =
            inv.timeout().value_or(Invocation::CalleeTimeoutDuration{});
        timer.expires_from_now(std::chrono::milliseconds(10));
        timer.async_wait(
            [inv, timeout](boost::system::error_code) mutable
            {
                inv.yield(Result{timeout.count()});
            });
        return deferment;
    };

    ScopedRealm realm{router.openRealm(config).value()};

    spawn(ioctx, [&](YieldContext yield)
    {
        std::chrono::milliseconds timeout{1};
        s.connect(withTcp, yield).value();
        s.join(testRealm, yield).value();
        s.enroll(Procedure{"rpc"}, rpc, yield).value();
        auto result =
            s.call(Rpc{"rpc"}.withDealerTimeout(timeout), yield).value();
        REQUIRE(result.args().size() == 1);
        CHECK(result[0] == timeout.count());
        s.disconnect();
    });

    ioctx.run();
}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
