/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#if defined(CPPWAMP_TEST_HAS_CORO)

#include <catch2/catch.hpp>
#include <cppwamp/json.hpp>
#include "cppwamp/realmobserver.hpp"
#include <cppwamp/session.hpp>
#include <cppwamp/spawn.hpp>
#include <cppwamp/tcp.hpp>
#include "testrouter.hpp"

using namespace wamp;

namespace
{

const std::string testRealm = "cppwamp.test";
const unsigned short testPort = 12345;
const auto withTcp = TcpHost("localhost", testPort).withFormat(json);

//------------------------------------------------------------------------------
inline void suspendCoro(YieldContext& yield)
{
    auto exec = boost::asio::get_associated_executor(yield);
    boost::asio::post(exec, yield);
}

} // anomymous namespace


//------------------------------------------------------------------------------
TEST_CASE( "WAMP meta events", "[WAMP][Router][thisone]" )
{
    IoContext ioctx;
    Session s1{ioctx};
    Session s2{ioctx};

    s1.observeIncidents(
        [](Incident i) {std::cout << i.toLogEntry() << std::endl;});
    s1.enableTracing();

    SECTION("Session meta events")
    {
        SessionJoinInfo info;
        info.sessionId = 0;

        auto onJoin = [&info](Event event)
        {
            event.convertTo(info);
        };

        spawn(ioctx, [&](YieldContext yield)
        {
            s1.connect(withTcp, yield).value();
            s1.join(Petition(testRealm), yield).value();
            s1.subscribe(Topic{"wamp.session.on_join"}, onJoin, yield).value();

            s2.connect(withTcp, yield).value();
            auto welcome = s2.join(Petition(testRealm), yield).value();

            while (info.sessionId == 0)
                suspendCoro(yield);

            CHECK(info.sessionId == welcome.id());
            s2.disconnect();
            s1.disconnect();
        });

        ioctx.run();
    }
}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
