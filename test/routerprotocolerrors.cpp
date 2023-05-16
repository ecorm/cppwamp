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
#include "mockclient.hpp"
#include "testrouter.hpp"

using namespace wamp;
using internal::MockClient;
using internal::MessageKind;

namespace
{

const std::string testRealm = "cppwamp.test";
const unsigned short testPort = 12345;

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
    return MockClient::toCommand<C>(std::move(m));
}

//------------------------------------------------------------------------------
void checkProtocolViolation(MockClient::Ptr client,
                            const std::string& hintKeyword, YieldContext yield)
{
    while (client->lastMessageKind() != MessageKind::abort)
        suspendCoro(yield);

    const auto& messages = client->messages();
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

} // anomymous namespace


//------------------------------------------------------------------------------
TEST_CASE( "WAMP protocol violation detection by router", "[WAMP][Router]" )
{
    IoContext ioctx;
    Session session{ioctx};
    auto client = internal::MockClient::create(ioctx, testPort);
    AccessActionInfo lastAction;
    auto guard = test::Router::instance().attachToAccessLog(
        [&lastAction](AccessLogEntry e) {lastAction = e.action;});

    auto checkLastAction = [&lastAction](const std::string& hintKeyword)
    {
        REQUIRE(lastAction.action == AccessAction::serverAbort);
        auto found = lastAction.options.find("message");
        REQUIRE(found != lastAction.options.end());
        REQUIRE(found->second.is<String>());
        const auto& hint = found->second.as<String>();
        bool foundHintKeyword = hint.find(hintKeyword) != hint.npos;
        CHECK(foundHintKeyword);
    };

    SECTION("Bad message")
    {
        struct TestVector
        {
            std::string json;
            std::string hintKeyword;
            std::string info;
        };

        const std::vector<TestVector> testVectors =
        {
            {"",                  "deserializing", "Empty message"},
            {"[1b,1,{}]",         "deserializing", "Invalid JSON"},
            {"\"1,1,{}\"",        "not an array",  "Non-array message"},
            {"[0,1,{}]",          "type number",   "Bad message type number"},
            {"[\"HELLO\",1,{}]",  "field schema",  "Non-integral message type field"},
            {"[1]",               "field schema",  "Missing message fields"},
            {"[2,1,{}]",          "Role",          "Bad message type for role"},
            {"[64,1,{},\"rpc\"]", "session state", "Bad message type for state"}
        };

        spawn(ioctx, [&](YieldContext yield)
        {
            for (const auto& testVector: testVectors)
            {
                INFO(testVector.info);
                lastAction.action = {};
                client->load({{testVector.json}});
                client->connect(yield);
                checkProtocolViolation(client, testVector.hintKeyword, yield);
                checkLastAction(testVector.hintKeyword);
                client->disconnect();
            }
        });

        ioctx.run();
    }

    SECTION("Reinvoking a closed RPC")
    {
        lastAction.action = {};
        client->load(
        {
             {{"[1,\"cppwamp.test\",{}]"}}, // HELLO
             {{"[64,1,{},\"rpc\"]"}},       // REGISTER
             {
                 {"[48,2,{\"progress\":true},\"rpc\",[1]]"},  // CALL
                 {"[48,2,{\"progress\":false},\"rpc\",[1]]"}, // CALL
                 {"[48,2,{\"progress\":true},\"rpc\",[1]]"},  // CALL
             }
        });

        spawn(ioctx, [&](YieldContext yield)
        {
            client->connect(yield);
            checkProtocolViolation(client, "reinvoke", yield);
            checkLastAction("reinvoke");
            client->disconnect();
        });

        ioctx.run();
    }

    SECTION("inbound YIELD request ID exceeds outbound INVOCATION watermark")
    {
        lastAction.action = {};
        client->load(
        {
             {{"[1,\"cppwamp.test\",{}]"}}, // HELLO
             {{"[64,1,{},\"rpc\"]"}},       // REGISTER
             {{"[48,2,{},\"rpc\",[1]]"}},   // CALL
             {{"[70,100,{},[1]]"}},         // YIELD
        });

        spawn(ioctx, [&](YieldContext yield)
        {
            client->connect(yield);
            checkProtocolViolation(client, "non-sequential", yield);
            checkLastAction("non-sequential");
            client->disconnect();
        });

        ioctx.run();
    }
}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
