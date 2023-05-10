/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#if defined(CPPWAMP_TEST_HAS_CORO)

#include <catch2/catch.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/spawn.hpp>
#include <cppwamp/tcp.hpp>
#include "mockserver.hpp"

using namespace wamp;
using internal::MockServer;
using internal::MessageKind;

namespace
{

const std::string testRealm = "cppwamp.test";
const unsigned short testPort = 54321;
const auto withTcp = TcpHost("localhost", testPort).withFormat(json);

//------------------------------------------------------------------------------
inline void suspendCoro(YieldContext& yield)
{
    auto exec = boost::asio::get_associated_executor(yield);
    boost::asio::post(exec, yield);
}

//------------------------------------------------------------------------------
template <typename C>
C toCommand(wamp::internal::Message&& m)
{
    return MockServer::toCommand<C>(std::move(m));
}

//------------------------------------------------------------------------------
void checkProtocolViolation(const Session& session, MockServer& server,
                            const std::string& hintKeyword, YieldContext yield)
{
    CHECK(session.state() == SessionState::failed);
    auto kind = MessageKind::none;
    while (kind != MessageKind::abort)
    {
        suspendCoro(yield);
        const auto& messages = server.messages();
        kind = messages.empty() ? MessageKind::none : messages.back().kind();
    }
    auto last = server.messages().back();
    REQUIRE(last.kind() == MessageKind::abort);
    auto reason = toCommand<Reason>(std::move(last));
    CHECK(reason.errorCode() == WampErrc::protocolViolation);
    REQUIRE(reason.hint().has_value());
    bool hintFound =
        reason.hint().value().find(hintKeyword) != std::string::npos;
    CHECK(hintFound);
}

//------------------------------------------------------------------------------
void testMalformed(Session& session, MockServer& server, std::string badWelcome,
                   const std::string& hintKeyword)
{
    server.load({std::move(badWelcome)});
    server.start();
    spawn([&, hintKeyword](YieldContext yield)
    {
        session.connect(withTcp, yield).value();
        auto result = session.join(testRealm, yield);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == WampErrc::protocolViolation);
        checkProtocolViolation(session, server, hintKeyword, yield);
        server.stop();
    });
}

} // anomymous namespace


//------------------------------------------------------------------------------
TEST_CASE( "WAMP protocol violation detection by client", "[WAMP][Basic]" )
{
    IoContext ioctx;
    auto srv = internal::MockServer::create(ioctx, testPort);
    Session s{ioctx};

    SECTION( "Empty message" )
    {
        testMalformed(s, *srv, "", "deserializing");
        ioctx.run();
    }

    SECTION( "Invalid JSON" )
    {
        testMalformed(s, *srv, "[2b,1,{}]", "deserializing");
        ioctx.run();
    }

    SECTION( "Non-array message" )
    {
        testMalformed(s, *srv, "\"2b,1,{}\"", "not an array");
        ioctx.run();
    }

    SECTION( "Bad message type number" )
    {
        testMalformed(s, *srv, "[0,1,{}]", "invalid type number");
        ioctx.run();
    }

    SECTION( "Non-integral message type field" )
    {
        testMalformed(s, *srv, "[\"WELCOME\",1,{}]", "field schema");
        ioctx.run();
    }

    SECTION( "Missing message fields" )
    {
        testMalformed(s, *srv, "[2]", "field schema");
        ioctx.run();
    }

    SECTION( "Invalid message type for client role" )
    {
        testMalformed(s, *srv, "[1,\"cppwamp.test\",{}]", "Role"); // HELLO
        ioctx.run();
    }

    SECTION( "Invalid message type for session state" )
    {
        testMalformed(s, *srv, "[36,1,1,{}]", "session state"); // EVENT
        ioctx.run();
    }
}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
