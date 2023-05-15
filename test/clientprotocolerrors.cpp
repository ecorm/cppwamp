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
    while (server.lastMessageKind() != MessageKind::abort)
        suspendCoro(yield);

    CHECK(session.state() == SessionState::failed);

    const auto& messages = server.messages();
    REQUIRE(!messages.empty());
    auto last = messages.back();
    REQUIRE(last.kind() == MessageKind::abort);

    auto reason = toCommand<Reason>(std::move(last));
    CHECK(reason.errorCode() == WampErrc::protocolViolation);

    REQUIRE(reason.hint().has_value());
    bool hintFound =
        reason.hint().value().find(hintKeyword) != std::string::npos;
    CHECK(hintFound);
}

//------------------------------------------------------------------------------
void testMalformed(IoContext& ioctx, Session& session, MockServer::Ptr server,
                   std::string badWelcome, const std::string& hintKeyword)
{
    server->load({{std::move(badWelcome)}});
    spawn([&ioctx, &session, server, hintKeyword](YieldContext yield)
    {
        session.connect(withTcp, yield).value();
        auto result = session.join(testRealm, yield);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == WampErrc::protocolViolation);
        checkProtocolViolation(session, *server, hintKeyword, yield);
        session.disconnect();
        ioctx.stop();
    });

    ioctx.run();
    ioctx.restart();
}

} // anomymous namespace


//------------------------------------------------------------------------------
TEST_CASE( "WAMP protocol violation detection by client", "[WAMP][Basic]" )
{
    IoContext ioctx;
    Session session{ioctx};
    auto server = internal::MockServer::create(ioctx, testPort);
    server->start();

    {
        INFO("Bad messages");

        struct TestVector
        {
            std::string badMessage;
            std::string hintKeyword;
            std::string info;
        };

        std::vector<TestVector> testVectors =
        {
{"",                        "deserializing",       "Empty message"},
{"[2b,1,{}]",               "deserializing",       "Invalid JSON"},
{"\"2b,1,{}\"",             "not an array",        "Non-array message"},
{"[0,1,{}]",                "invalid type number", "Bad message type number"},
{"[\"WELCOME\",1,{}]",      "field schema",        "Non-integral message type field"},
{"[2]",                     "field schema",        "Missing message fields"},
{"[1,\"cppwamp.test\",{}]", "Role",                "Bad message type for role"},
{"[36,1,1,{}]",             "session state",       "Bad message type for state"}
        };

        for (const auto& vec: testVectors)
        {
            INFO(vec.info);
            testMalformed(ioctx, session, server, vec.badMessage, vec.hintKeyword);
        }
    }

    {
        INFO("Response with no matching request");

        server->load(
        {{
            {"[2,1,{}]"}, // WELCOME
            {"[65,1,1]"}  // REGISTERED
        }});

        spawn([&](YieldContext yield)
        {
            session.connect(withTcp, yield).value();
            session.join(testRealm, yield).value();

            checkProtocolViolation(session, *server, "matching request", yield);
            session.disconnect();
            ioctx.stop();
        });

        ioctx.run();
        ioctx.restart();
    }

    {
        INFO("Reinvoking non-completed RPC");

        server->load(
        {
            {{"[2,1,{}]"}}, // WELCOME
            {
                {"[65,1,1]"}, // REGISTERED
                {"[68,1,1,{},[1]]"}, // INVOCATION
                {"[68,1,1,{},[1]]"}
            }
        });

        CalleeChannel channel;
        auto onStream = [&channel](CalleeChannel ch)
        {
            channel = std::move(ch);
        };

        spawn([&](YieldContext yield)
        {
            session.connect(withTcp, yield).value();
            session.join(testRealm, yield).value();
            session.enroll(Stream{"stream"}, onStream, yield).value();

            checkProtocolViolation(session, *server, "reinvoke", yield);
            session.disconnect();
            ioctx.stop();
        });

        ioctx.run();
        ioctx.restart();
    }

    {
        INFO("Reinvoking a closed stream");

        server->load(
        {
            {{"[2,1,{}]"}}, // WELCOME
            {
                {"[65,1,1]"}, // REGISTERED
                {"[68,1,1,{\"progress\":true},[1]]"}, // INVOCATION
                {"[68,1,1,{\"progress\":false},[1]]"},
                {"[68,1,1,{\"progress\":true},[1]]"},
            }
        });

        CalleeChannel channel;
        auto onStream = [&channel](CalleeChannel ch)
        {
            channel = std::move(ch);
        };

        spawn([&](YieldContext yield)
        {
            session.connect(withTcp, yield).value();
            session.join(testRealm, yield).value();
            session.enroll(Stream{"stream"}, onStream, yield).value();

            checkProtocolViolation(session, *server, "reinvoke", yield);
            session.disconnect();
            ioctx.stop();
        });

        ioctx.run();
        ioctx.restart();
    }

    server->stop();
}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
