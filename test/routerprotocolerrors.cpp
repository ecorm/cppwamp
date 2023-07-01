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
#include "routerfixture.hpp"

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

//------------------------------------------------------------------------------
void checkNormalOperation(MockClient::Ptr client,
                          MessageKind lastExpectedMessageKind,
                          YieldContext yield)
{
    while (client->lastMessageKind() != lastExpectedMessageKind)
        suspendCoro(yield);

    const auto& messages = client->messages();
    REQUIRE(!messages.empty());
    auto last = messages.back();
    REQUIRE(last.kind() == lastExpectedMessageKind);
}

//------------------------------------------------------------------------------
template <typename TErrc>
void checkErrorResponse(MockClient::Ptr client, TErrc expectedErrorCode,
                        YieldContext yield)
{
    while (client->lastMessageKind() != MessageKind::error)
        suspendCoro(yield);

    const auto& messages = client->messages();
    REQUIRE(!messages.empty());
    auto last = messages.back();
    REQUIRE(last.kind() == MessageKind::error);
    auto error = toCommand<Error>(std::move(last));
    CHECK(error.errorCode() == expectedErrorCode);
}

} // anomymous namespace


//------------------------------------------------------------------------------
TEST_CASE( "WAMP protocol violation detection by router", "[WAMP][Router]" )
{
    if (!test::RouterFixture::enabled())
        return;

    IoContext ioctx;
    Session session{ioctx};
    auto client = internal::MockClient::create(ioctx, testPort);
    auto client2 = internal::MockClient::create(ioctx, testPort);
    AccessActionInfo lastAction;
    auto guard = test::RouterFixture::instance().snoopAccessLog(
        ioctx.get_executor(),
        [&lastAction](AccessLogEntry e) {lastAction = e.action;});

    auto checkLastAction = [&lastAction](const std::string& hintKeyword,
                                         YieldContext yield)
    {
        while (lastAction.action != AccessAction::serverAbort)
            suspendCoro(yield);
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
                checkLastAction(testVector.hintKeyword, yield);
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
            checkLastAction("reinvoke", yield);
            client->disconnect();
        });

        ioctx.run();
    }

    SECTION("Reinvoking a non-progressive CALL")
    {
        lastAction.action = {};
        client->load(
        {
            {{"[1,\"cppwamp.test\",{}]"}}, // HELLO
            {{"[64,1,{},\"rpc\"]"}},       // REGISTER
            {
                {"[48,2,{},\"rpc\",[1]]"},                  // CALL
                {"[48,2,{\"progress\":true},\"rpc\",[1]]"}, // CALL
            }
        });

        spawn(ioctx, [&](YieldContext yield)
        {
            client->connect(yield);
            checkProtocolViolation(client, "reinvoke", yield);
            checkLastAction("reinvoke", yield);
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
            checkLastAction("non-sequential", yield);
            client->disconnect();
        });

        ioctx.run();
    }

    SECTION("defunct YIELD request ID below INVOCATION watermark")
    {
        lastAction.action = {};
        client->load(
        {
             {{"[1,\"cppwamp.test\",{}]"}}, // HELLO
             {{"[64,1,{},\"rpc\"]"}},       // REGISTER
             {{"[48,2,{},\"rpc\",[1]]"}},   // CALL
             {
                 {"[70,1,{},[1]]"},         // YIELD (ignored)
                 {"[70,2,{},[1]]"},         // YIELD (accepted)
             }
        });

        spawn(ioctx, [&](YieldContext yield)
        {
            client->connect(yield);
            checkNormalOperation(client, MessageKind::result, yield);
            client->disconnect();
        });

        ioctx.run();
    }

    SECTION("inbound ERROR request ID exceeds outbound INVOCATION watermark")
    {
        lastAction.action = {};
        client->load(
        {
             {{"[1,\"cppwamp.test\",{}]"}}, // HELLO
             {{"[64,1,{},\"rpc\"]"}},       // REGISTER
             {{"[48,2,{},\"rpc\",[1]]"}},   // CALL
             {{"[8,68,100,{},\"bad\"]"}}    // ERROR
        });

        spawn(ioctx, [&](YieldContext yield)
        {
            client->connect(yield);
            checkProtocolViolation(client, "non-sequential", yield);
            checkLastAction("non-sequential", yield);
            client->disconnect();
        });

        ioctx.run();
    }

    SECTION("defunct ERROR request ID below INVOCATION watermark")
    {
        lastAction.action = {};
        client->load(
        {
             {{"[1,\"cppwamp.test\",{}]"}}, // HELLO
             {{"[64,1,{},\"rpc\"]"}},       // REGISTER
             {{"[48,2,{},\"rpc\",[1]]"}},   // CALL
             {
                 {"[8,68,1,{},\"bad\"]"},   // ERROR (ignored)
                 {"[8,68,2,{},\"bad\"]"},   // ERROR (accepted)
             }
        });

        spawn(ioctx, [&](YieldContext yield)
        {
            client->connect(yield);
            checkNormalOperation(client, MessageKind::error, yield);
            client->disconnect();
        });

        ioctx.run();
    }

    SECTION("inbound general command request ID exceeds inbound watermark")
    {
        lastAction.action = {};
        client->load(
        {
             {{"[1,\"cppwamp.test\",{}]"}}, // HELLO
             {{"[64,1,{},\"rpc\"]"}},       // REGISTER
             {{"[32,3,{},\"topic\"]"}}      // SUBSCRIBE
        });

        spawn(ioctx, [&](YieldContext yield)
        {
            client->connect(yield);
            checkProtocolViolation(client, "non-sequential", yield);
            checkLastAction("non-sequential", yield);
            client->disconnect();
        });

        ioctx.run();
    }

    SECTION("inbound general command request ID is below inbound watermark")
    {
        lastAction.action = {};
        client->load(
        {
             {{"[1,\"cppwamp.test\",{}]"}}, // HELLO
             {{"[64,1,{},\"rpc\"]"}},       // REGISTER
             {{"[32,1,{},\"topic\"]"}}      // SUBSCRIBE
        });

        spawn(ioctx, [&](YieldContext yield)
        {
            client->connect(yield);
            checkProtocolViolation(client, "non-sequential", yield);
            checkLastAction("non-sequential", yield);
            client->disconnect();
        });

        ioctx.run();
    }

    SECTION("inbound cancel exceeds inbound watermark")
    {
        lastAction.action = {};
        client->load(
        {
             {{"[1,\"cppwamp.test\",{}]"}}, // HELLO
             {{"[64,1,{},\"rpc\"]"}},       // REGISTER
             {{"[48,2,{},\"rpc\",[1]]"}},   // CALL
             {{"[49,3,{}]"}}                // CANCEL
        });

        spawn(ioctx, [&](YieldContext yield)
        {
            client->connect(yield);
            checkProtocolViolation(client, "non-sequential", yield);
            checkLastAction("non-sequential", yield);
            client->disconnect();
        });

        ioctx.run();
    }

    SECTION("defunct CANCEL request ID below INVOCATION watermark")
    {
        lastAction.action = {};
        client->load(
        {
             {{"[1,\"cppwamp.test\",{}]"}}, // HELLO
             {{"[64,1,{},\"rpc\"]"}},       // REGISTER
             {{"[48,2,{},\"rpc\",[1]]"}},   // CALL
             {
                 {"[49,1,{}]"},             // CANCEL (ignored)
                 {"[49,2,{}]"}              // CANCEL (accepted)
             }
        });

        spawn(ioctx, [&](YieldContext yield)
        {
            client->connect(yield);
            checkNormalOperation(client, MessageKind::error, yield);
            client->disconnect();
        });

        ioctx.run();
    }

    SECTION("inbound call exceeds inbound watermark")
    {
        lastAction.action = {};
        client->load(
        {
             {{"[1,\"cppwamp.test\",{}]"}}, // HELLO
             {{"[64,1,{},\"rpc\"]"}},       // REGISTER
             {{"[48,3,{},\"rpc\",[1]]"}}    // CALL
        });

        spawn(ioctx, [&](YieldContext yield)
        {
            client->connect(yield);
            checkProtocolViolation(client, "non-sequential", yield);
            checkLastAction("non-sequential", yield);
            client->disconnect();
        });

        ioctx.run();
    }

    SECTION("defunct CALL request ID below INVOCATION watermark")
    {
        lastAction.action = {};
        client->load(
        {
             {{"[1,\"cppwamp.test\",{}]"}}, // HELLO
             {{"[64,1,{},\"rpc\"]"}},       // REGISTER
             {
                {"[48,2,{},\"rpc\",[2]]"},  // CALL
                {"[48,1,{},\"rpc\",[1]]"}   // CALL (ignored)
             },
             {{"[32,3,{},\"topic\"]"}}      // SUBSCRIBE
        });

        spawn(ioctx, [&](YieldContext yield)
        {
            client->connect(yield);
            checkNormalOperation(client, MessageKind::subscribed, yield);
            client->disconnect();
        });

        ioctx.run();
    }

    SECTION("Unregistering a non-owned registration")
    {
        // TODO: WAMP - Follow up on
        // https://github.com/wamp-proto/wamp-proto/discussions/496
        lastAction.action = {};
        client->load(
        {
            {{"[1,\"cppwamp.test\",{}]"}}, // HELLO
            {{"[64,1,{},\"rpc\"]"}}        // REGISTER
        });

        client2->load(
        {
            {{"[1,\"cppwamp.test\",{}]"}}, // HELLO
            {{"[66,1,1]"}}                 // UNREGISTER
        });

        spawn(ioctx, [&](YieldContext yield)
        {
            client->connect(yield);
            checkNormalOperation(client, MessageKind::registered, yield);
            client2->connect(yield);
            checkErrorResponse(client2, WampErrc::noSuchRegistration, yield);
            client->disconnect();
            client2->disconnect();
        });

        ioctx.run();
    }

    SECTION("Unsubscribing a non-owned subscription")
    {
        // TODO: WAMP - Follow up on
        // https://github.com/wamp-proto/wamp-proto/discussions/496
        lastAction.action = {};
        client->load(
        {
            {{"[1,\"cppwamp.test\",{}]"}}, // HELLO
            {{"[32,1,{},\"topic\"]"}}      // SUBSCRIBE
        });

        client2->load(
        {
            {{"[1,\"cppwamp.test\",{}]"}}, // HELLO
            {{"[34,1,1]"}}                 // UNSUBSCRIBE
        });

        spawn(ioctx, [&](YieldContext yield)
        {
            client->connect(yield);
            checkNormalOperation(client, MessageKind::subscribed, yield);
            client2->connect(yield);
            checkErrorResponse(client2, WampErrc::noSuchSubscription, yield);
            client->disconnect();
            client2->disconnect();
        });

        ioctx.run();
    }
}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
