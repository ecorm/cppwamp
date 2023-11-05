/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#if defined(CPPWAMP_TEST_HAS_CORO)

#include <catch2/catch.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/spawn.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcpclient.hpp>
#include "clienttesting.hpp"
#include "mockwampserver.hpp"

using namespace wamp;
using internal::MockWampServer;
using internal::MessageKind;

namespace
{

const std::string testRealm = "cppwamp.test";
const unsigned short testPort = 54321;
const auto withTcp = TcpHost("localhost", testPort).withFormat(json);

//------------------------------------------------------------------------------
template <typename C>
C toCommand(wamp::internal::Message&& m)
{
    return MockWampServer::toCommand<C>(std::move(m));
}

//------------------------------------------------------------------------------
void checkProtocolViolation(const Session& session, MockWampServer& server,
                            const std::string& hintKeyword, YieldContext yield)
{
    while (server.lastMessageKind() != MessageKind::abort)
        test::suspendCoro(yield);

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
void checkInvocationError(const Session& session, MockWampServer& server,
                          const std::string& hintKeyword, YieldContext yield)
{
    while (server.lastMessageKind() != MessageKind::error)
        test::suspendCoro(yield);

    CHECK(session.state() == SessionState::established);

    const auto& messages = server.messages();
    REQUIRE(!messages.empty());
    auto last = messages.back();
    REQUIRE(last.kind() == MessageKind::error);

    auto error = toCommand<Error>(std::move(last));
    CHECK(error.errorCode() == WampErrc::optionNotAllowed);

    REQUIRE(!error.args().empty());
    REQUIRE(error.args().front().is<String>());
    const auto& hint = error.args().front().as<String>();
    bool hintFound = hint.find(hintKeyword) != std::string::npos;
    CHECK(hintFound);
}

//------------------------------------------------------------------------------
void testMalformed(IoContext& ioctx, Session& session,
                   MockWampServer::Ptr server, std::string badWelcome,
                   const std::string& hintKeyword)
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
TEST_CASE( "WAMP protocol violation detection by client", "[WAMP][Advanced]" )
{
    IoContext ioctx;
    Session session{ioctx};
    auto server = internal::MockWampServer::create(ioctx.get_executor(),
                                                   testPort);
    server->start();

    {
        INFO("Bad messages");

        struct TestVector
        {
            std::string badMessage;
            std::string hintKeyword;
            std::string info;
        };

        const std::vector<TestVector> testVectors =
        {
            {"",                        "deserializing", "Empty message"},
            {"[2b,1,{}]",               "deserializing", "Invalid JSON"},
            {"\"2,1,{}\"",              "not an array",  "Non-array message"},
            {"[0,1,{}]",                "type number",   "Bad message type number"},
            {"[\"WELCOME\",1,{}]",      "field schema",  "Non-integral message type field"},
            {"[2]",                     "field schema",  "Missing message fields"},
            {"[1,\"cppwamp.test\",{}]", "Role",          "Bad message type for role"},
            {"[36,1,1,{}]",             "session state", "Bad message type for state"}
        };

        for (const auto& vec: testVectors)
        {
            INFO(vec.info);
            testMalformed(ioctx, session, server, vec.badMessage,
                          vec.hintKeyword);
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

#ifdef CPPWAMP_STRICT_INVOCATION_ID_CHECKS
    {
        INFO("Non-sequential INVOCATION request ID");

        server->load(
        {
            {{"[2,1,{}]"}}, // WELCOME
            {
                {"[65,1,1]"}, // REGISTERED
                {"[68,1,1,{},[1]]"}, // INVOCATION
                {"[68,3,1,{},[1]]"}  // INVOCATION
            }
        });

        auto onRpc = [](Invocation) -> Outcome {return deferment;};

        spawn([&](YieldContext yield)
        {
            session.connect(withTcp, yield).value();
            session.join(testRealm, yield).value();
            session.enroll(Procedure{"rpc"}, onRpc, yield).value();

            checkProtocolViolation(session, *server, "non-sequential", yield);
            session.disconnect();
            ioctx.stop();
        });

        ioctx.run();
        ioctx.restart();
    }
#endif

    {
        INFO("Progressive invocation on RPC not registered as stream");

        server->load(
        {
            {{"[2,1,{}]"}}, // WELCOME
            {
                {"[65,1,1]"}, // REGISTERED
                {"[68,1,1,{\"progress\":true},[1]]"} // INVOCATION
            }
        });

        auto onRpc = [](Invocation) -> Outcome {return deferment;};

        spawn([&](YieldContext yield)
        {
            session.connect(withTcp, yield).value();
            session.join(testRealm, yield).value();
            session.enroll(Procedure{"rpc"}, onRpc, yield).value();

            checkInvocationError(session, *server, "registered as a stream",
                                 yield);
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

        auto onRpc = [](Invocation) -> Outcome {return deferment;};

        spawn([&](YieldContext yield)
        {
            session.connect(withTcp, yield).value();
            session.join(testRealm, yield).value();
            session.enroll(Procedure{"rpc"}, onRpc, yield).value();

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
