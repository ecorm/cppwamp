/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#if defined(CPPWAMP_TEST_HAS_CORO)

#include <catch2/catch.hpp>
#include <vector>
#include <cppwamp/cachingauthorizer.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/spawn.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/internal/disclosuremode.hpp>
#include <cppwamp/transports/tcp.hpp>
#include "routerfixture.hpp"

using namespace wamp;
using namespace wamp::internal;

namespace
{

const std::string testRealm = "cppwamp.test-authorizer";
const unsigned short testPort = 12345;
const auto withTcp = TcpHost("localhost", testPort).withFormat(json);

//------------------------------------------------------------------------------
struct TestAuthorizer : public Authorizer
{
    TestAuthorizer()
        : topic("empty"), pub("empty"), proc("empty"), rpc("empty")
    {}

    void authorize(Topic t, AuthorizationRequest a) override
    {
        topic = t;
        info = a.info();
        a.authorize(std::move(t), canSubscribe, cacheEnabled);
    }

    void authorize(Pub p, AuthorizationRequest a) override
    {
        pub = p;
        info = a.info();

        if (discloseMeAllowed || !p.discloseMe())
            return a.authorize(std::move(p), canPublish, cacheEnabled);

        a.authorize(std::move(p),
                    Authorization::denied(WampErrc::discloseMeDisallowed));
    }

    void authorize(Procedure p, AuthorizationRequest a) override
    {
        proc = p;
        info = a.info();
        a.authorize(std::move(p), canRegister, cacheEnabled);
    }

    void authorize(Rpc r, AuthorizationRequest a) override
    {
        rpc = r;
        info = a.info();

        if (discloseMeAllowed || !r.discloseMe())
            return a.authorize(std::move(r), canCall, cacheEnabled);

        a.authorize(std::move(r),
                    Authorization::denied(WampErrc::discloseMeDisallowed));
    }

    void clear(bool enableCache = false)
    {
        canSubscribe = Authorization::granted();
        canPublish = Authorization::granted();
        canRegister = Authorization::granted();
        canCall = Authorization::granted();

        topic = Topic{"empty"};
        pub = Pub{"empty"};
        proc = Procedure{"empty"};
        rpc = Rpc{"empty"};

        info = {};
        cacheEnabled = enableCache;
        discloseMeAllowed = true;
    }

    Authorization canSubscribe;
    Authorization canPublish;
    Authorization canRegister;
    Authorization canCall;
    Topic topic;
    Pub pub;
    Procedure proc;
    Rpc rpc;
    SessionInfo info;
    bool cacheEnabled = false;
    bool discloseMeAllowed = true;
};

} // anomymous namespace


//------------------------------------------------------------------------------
TEST_CASE( "Disclosure computation", "[Disclosure]" )
{
    constexpr bool y = true;
    constexpr bool n = false;

    struct DisclosureVector
    {
        bool producer;
        bool consumer;
        bool disclosed;
    };

    struct TestVector
    {
        Disclosure disclosure;
        std::vector<DisclosureVector> subTestVectors;
    };

    using D = Disclosure;

    const std::vector<TestVector> testVectors =
    {
        //              p, c, d    p, c, d    p, c, d    p, c, d
        {D::preset,   {{n, n, n}, {n, y, n}, {y, n, y}, {y, y, y}}},
        {D::producer, {{n, n, n}, {n, y, n}, {y, n, y}, {y, y, y}}},
        {D::consumer, {{n, n, n}, {n, y, y}, {y, n, n}, {y, y, y}}},
        {D::either,   {{n, n, n}, {n, y, y}, {y, n, y}, {y, y, y}}},
        {D::both,     {{n, n, n}, {n, y, n}, {y, n, n}, {y, y, y}}},
        {D::reveal,   {{n, n, y}, {n, y, y}, {y, n, y}, {y, y, y}}},
        {D::conceal,  {{n, n, n}, {n, y, n}, {y, n, n}, {y, y, n}}}
    };

    SECTION("Without realm preset")
    {
        for (unsigned i=0; i<testVectors.size(); ++i)
        {
            INFO("for i=" << i);
            const auto& vec = testVectors.at(i);
            for (const auto& subVec: vec.subTestVectors)
            {
                INFO("with producer=" << subVec.producer
                     << ", consumer=" << subVec.consumer);
                internal::DisclosureMode mode{vec.disclosure};
                CHECK(mode.compute(subVec.producer, subVec.consumer) ==
                      subVec.disclosed);
            }
        }
    }

    SECTION("With realm preset")
    {
        internal::DisclosureMode presetMode{D::preset};
        internal::DisclosureMode nonPresetMode{D::consumer};
        CHECK(presetMode.compute(false, false, D::reveal) == true);
        CHECK(nonPresetMode.compute(false, true, D::conceal) == true);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "Router dynamic authorizer", "[WAMP][Router]" )
{
    if (!test::RouterFixture::enabled())
        return;

    wamp::Router& router = test::RouterFixture::instance().router();
    IoContext ioctx;
    auto auth = std::make_shared<TestAuthorizer>();
    auto postingAuth = PostingAuthorizer::create(auth, ioctx.get_executor());
    auto config =
        RealmOptions{testRealm}.withMetaApiEnabled()
                               .withAuthorizer(postingAuth)
                               .withCallerDisclosure(Disclosure::reveal)
                               .withPublisherDisclosure(Disclosure::conceal);
    test::ScopedRealm realm{router.openRealm(config).value()};

    Event event;
    auto onEvent = [&event](Event e) {event = std::move(e);};

    Invocation invocation;
    auto onInvocation = [&invocation](Invocation i) -> Outcome
    {
        invocation = std::move(i);
        return Result{};
    };

    spawn(ioctx, [&](YieldContext yield)
    {
        Session s{ioctx};
        s.connect(withTcp, yield).value();
        auto welcome = s.join(testRealm, yield).value();

        {
            INFO("Subscribe authorized");
            auth->clear();
            auto sub = s.subscribe(Topic{"topic1"}, [](Event){}, yield);
            CHECK(auth->topic.uri() == "topic1");
            CHECK(auth->info.sessionId() == welcome.sessionId());
            CHECK(sub.has_value());
        }

        {
            INFO("Subscribe denied");
            auth->clear();
            auth->canSubscribe = Authorization::denied();
            auto sub = s.subscribe(Topic{"topic2"}, [](Event){}, yield);
            CHECK(auth->topic.uri() == "topic2");
            CHECK(auth->info.sessionId() == welcome.sessionId());
            REQUIRE_FALSE(sub.has_value());
            CHECK(sub.error() == WampErrc::authorizationDenied);
        }

        {
            INFO("Subscribe authorization failed");
            auth->clear();
            auth->canSubscribe = Authorization::failed();
            auto sub = s.subscribe(Topic{"topic3"}, [](Event){}, yield);
            CHECK(auth->topic.uri() == "topic3");
            CHECK(auth->info.sessionId() == welcome.sessionId());
            REQUIRE_FALSE(sub.has_value());
            CHECK(sub.error() == WampErrc::authorizationFailed);
        }

        {
            INFO("Subscribe denied with custom error");
            auth->clear();
            auth->canSubscribe = Authorization::denied(WampErrc::invalidUri);
            auto sub = s.subscribe(Topic{"topic4"}, [](Event){}, yield);
            CHECK(auth->topic.uri() == "topic4");
            CHECK(auth->info.sessionId() == welcome.sessionId());
            REQUIRE_FALSE(sub.has_value());
            CHECK(sub.error() == WampErrc::authorizationDenied);
        }

        {
            INFO("Subscribe to meta-topic authorized");
            auth->clear();
            auto sub = s.subscribe(Topic{"wamp.session.on_join"},
                                   [](Event){}, yield);
            CHECK(auth->topic.uri() == "wamp.session.on_join");
            CHECK(auth->info.sessionId() == welcome.sessionId());
            CHECK(sub.has_value());
        }

        {
            INFO("Subscribe to meta-topic denied");
            auth->clear();
            auth->canSubscribe = Authorization::denied();
            auto sub = s.subscribe(Topic{"wamp.session.on_leave"},
                                   [](Event){}, yield);
            CHECK(auth->topic.uri() == "wamp.session.on_leave");
            CHECK(auth->info.sessionId() == welcome.sessionId());
            REQUIRE_FALSE(sub.has_value());
            CHECK(sub.error() == WampErrc::authorizationDenied);
        }

        {
            INFO("Publish authorized");
            auth->clear();
            event = {};
            s.subscribe(Topic{"topic5"}, onEvent, yield).value();
            auto ack = s.publish(Pub{"topic5"}.withArgs(42)
                                              .withExcludeMe(false), yield);
            CHECK(ack.has_value());
            CHECK(auth->pub.uri() == "topic5");
            CHECK(auth->info.sessionId() == welcome.sessionId());

            while (event.args().empty())
                test::suspendCoro(yield);
            CHECK_FALSE(event.publisher().has_value());
        }

        {
            INFO("Publish denied");
            auth->clear();
            auth->canPublish = Authorization::denied();
            auto ack = s.publish(Pub{"topic6"}.withArgs(42), yield);
            REQUIRE_FALSE(ack.has_value());
            CHECK(ack.error() == WampErrc::authorizationDenied);
        }

        {
            INFO("Publish authorized with overriden disclosure rule");
            auth->clear();
            auth->canPublish = Authorization::granted(Disclosure::reveal);
            event = {};
            s.subscribe(Topic{"topic7"}, onEvent, yield).value();
            auto ack = s.publish(Pub{"topic7"}.withArgs(42)
                                              .withExcludeMe(false), yield);
            CHECK(ack.has_value());
            CHECK(auth->pub.uri() == "topic7");
            CHECK(auth->info.sessionId() == welcome.sessionId());

            while (event.args().empty())
                test::suspendCoro(yield);
            CHECK(event.publisher() == welcome.sessionId());
        }

        {
            INFO("Publish disclosure disallowed");
            auth->clear();
            auth->canPublish = Authorization::granted(Disclosure::conceal);
            auth->discloseMeAllowed = false;
            auto ack = s.publish(Pub{"topic8"}.withArgs(42)
                                     .withExcludeMe(false)
                                     .withDiscloseMe(), yield);
            REQUIRE_FALSE(ack.has_value());
            CHECK(ack.error() == WampErrc::discloseMeDisallowed);
        }

        {
            INFO("Register authorized");
            auth->clear();
            auto reg = s.enroll(Procedure{"rpc1"},
                                [](Invocation) -> Outcome{return Result{};},
                                yield);
            CHECK(auth->proc.uri() == "rpc1");
            CHECK(auth->info.sessionId() == welcome.sessionId());
            CHECK(reg.has_value());
        }

        {
            INFO("Register denied");
            auth->clear();
            auth->canRegister = Authorization::denied();
            auto reg = s.enroll(Procedure{"rpc2"},
                                [](Invocation) -> Outcome{return Result{};},
                                yield);
            CHECK(auth->proc.uri() == "rpc2");
            CHECK(auth->info.sessionId() == welcome.sessionId());
            REQUIRE_FALSE(reg.has_value());
            CHECK(reg.error() == WampErrc::authorizationDenied);
        }

        {
            INFO("Call authorized");
            auth->clear();
            invocation = {};
            s.enroll(Procedure{"rpc3"}, onInvocation, yield).value();
            auto result = s.call(Rpc{"rpc3"}.withArgs(42), yield);
            CHECK(result.has_value());
            CHECK(auth->rpc.uri() == "rpc3");
            CHECK(auth->info.sessionId() == welcome.sessionId());

            while (invocation.args().empty())
                test::suspendCoro(yield);
            REQUIRE(invocation.caller().has_value());
            CHECK(invocation.caller().value() == welcome.sessionId());
        }

        {
            INFO("Call denied");
            auth->clear();
            auth->canCall = Authorization::denied();
            s.enroll(Procedure{"rpc4"},
                     [](Invocation) -> Outcome {return Result{};},
                     yield).value();
            auto result = s.call(Rpc{"rpc4"}.withArgs(42), yield);
            REQUIRE_FALSE(result.has_value());
            CHECK(result.error() == WampErrc::authorizationDenied);
            CHECK(auth->rpc.uri() == "rpc4");
            CHECK(auth->info.sessionId() == welcome.sessionId());
        }

        {
            INFO("Call authorized with overriden disclosure rule");
            auth->clear();
            auth->canCall = Authorization::granted(Disclosure::conceal);
            invocation = {};
            s.enroll(Procedure{"rpc5"}, onInvocation, yield).value();
            auto result = s.call(Rpc{"rpc5"}.withArgs(42).withDiscloseMe(),
                                 yield);
            CHECK(result.has_value());
            CHECK(auth->rpc.uri() == "rpc5");
            CHECK(auth->info.sessionId() == welcome.sessionId());

            while (invocation.args().empty())
                test::suspendCoro(yield);
            CHECK_FALSE(invocation.caller().has_value());
        }

        {
            INFO("Call disclosure disallowed");
            auth->clear();
            auth->canCall = Authorization::granted(Disclosure::conceal);
            auth->discloseMeAllowed = false;
            s.enroll(Procedure{"rpc6"}, onInvocation, yield).value();
            auto result = s.call(Rpc{"rpc6"}.withArgs(42).withDiscloseMe(),
                                 yield);
            REQUIRE_FALSE(result.has_value());
            CHECK(result.error() == WampErrc::discloseMeDisallowed);
            CHECK(auth->rpc.uri() == "rpc6");
            CHECK(auth->info.sessionId() == welcome.sessionId());
        }

        {
            INFO("Call denied but procedure doesn't exist");
            auth->clear();
            auth->canCall = Authorization::denied();
            auto result = s.call(Rpc{"rpc7"}.withArgs(42), yield);
            REQUIRE_FALSE(result.has_value());
            CHECK(result.error() == WampErrc::noSuchProcedure);
            CHECK(auth->rpc.uri() == "empty");
            CHECK(auth->info.sessionId() == 0);
        }

        {
            INFO("Call meta-procedure authorized");
            auth->clear();
            invocation = {};
            auto result = s.call(Rpc{"wamp.session.count"}, yield);
            REQUIRE(result.has_value());
            REQUIRE_FALSE(result.value().args().empty());
            CHECK(result.value().args().at(0) == 1);
            CHECK(auth->rpc.uri() == "wamp.session.count");
            CHECK(auth->info.sessionId() == welcome.sessionId());
        }

        {
            INFO("Call meta-procedure denied");
            auth->clear();
            auth->canCall = Authorization::denied();
            auto result = s.call(Rpc{"wamp.session.count"}, yield);
            REQUIRE_FALSE(result.has_value());
            CHECK(result.error() == WampErrc::authorizationDenied);
            CHECK(auth->rpc.uri() == "wamp.session.count");
            CHECK(auth->info.sessionId() == welcome.sessionId());
        }

        s.disconnect();
    });

    ioctx.run();
}

//------------------------------------------------------------------------------
TEST_CASE( "LRU Cache", "[LruCache]" )
{
    LruCache<std::string, std::shared_ptr<int>> cache{3};

    auto n1 = std::make_shared<int>(1);
    auto n2 = std::make_shared<int>(2);
    auto n3 = std::make_shared<int>(3);
    auto n4 = std::make_shared<int>(4);

    SECTION("Empty cache")
    {
        CHECK(cache.empty());
        CHECK(cache.size() == 0);
        CHECK(cache.capacity() == 3);
    }

    SECTION("Lookups, insertions, and clearing")
    {
        CHECK(cache.lookup("a") == nullptr);

        cache.upsert("a", n1);
        CHECK(cache.size() == 1);
        CHECK_FALSE(cache.empty());

        auto* result = cache.lookup("a");
        REQUIRE(result != nullptr);
        CHECK(**result == 1);
        CHECK(n1.use_count() == 2);

        cache.upsert("b", n2);
        CHECK(cache.size() == 2);
        CHECK_FALSE(cache.empty());

        result = cache.lookup("b");
        REQUIRE(result != nullptr);
        CHECK(**result == 2);
        CHECK(n2.use_count() == 2);

        cache.upsert("c", n3);
        CHECK(cache.size() == 3);
        CHECK_FALSE(cache.empty());

        result = cache.lookup("c");
        REQUIRE(result != nullptr);
        CHECK(**result == 3);
        CHECK(n3.use_count() == 2);

        // This next insertion will evict {"a", n1}
        cache.upsert("d", n4);
        CHECK(cache.size() == 3);
        CHECK_FALSE(cache.empty());
        CHECK(n1.use_count() == 1);
        CHECK(cache.lookup("a") == nullptr);

        result = cache.lookup("d");
        REQUIRE(result != nullptr);
        CHECK(**result == 4);
        CHECK(n4.use_count() == 2);

        cache.clear();
        CHECK(cache.empty());
        CHECK(cache.size() == 0);
        CHECK(n1.use_count() == 1);
        CHECK(n2.use_count() == 1);
        CHECK(n3.use_count() == 1);
        CHECK(n4.use_count() == 1);
    }

    SECTION("conditional eviction")
    {
        cache.upsert("a", n1);
        cache.upsert("b", n2);
        cache.upsert("c", n3);

        cache.evictIf(
            [](const std::string& key, std::shared_ptr<int> value) -> bool
            {
                return (key == "b") && (*value == 2);
            });

        CHECK(cache.size() == 2);
        CHECK(n1.use_count() == 2);
        CHECK(n2.use_count() == 1);
        CHECK(n3.use_count() == 2);
    }
}

//------------------------------------------------------------------------------
TEST_CASE( "Router caching dynamic authorizer", "[WAMP][Router]" )
{
    if (!test::RouterFixture::enabled())
        return;

    wamp::Router& router = test::RouterFixture::instance().router();
    IoContext ioctx;
    auto auth = std::make_shared<TestAuthorizer>();
    auto postingAuth = PostingAuthorizer::create(auth, ioctx.get_executor());
    auto cachingAuth = CachingAuthorizer::create(postingAuth, 1000);
    auto options =
        RealmOptions{testRealm}.withMetaApiEnabled()
                               .withAuthorizer(cachingAuth)
                               .withCallerDisclosure(Disclosure::reveal)
                               .withPublisherDisclosure(Disclosure::conceal);
    test::ScopedRealm realm{router.openRealm(options).value()};

    Event event;
    auto onEvent = [&event](Event e) {event = std::move(e);};

    Invocation invocation;
    auto onInvocation = [&invocation](Invocation i) -> Outcome
    {
        invocation = std::move(i);
        return Result{};
    };

    spawn(ioctx, [&](YieldContext yield)
    {
        Session s1{ioctx};
        Session s2{ioctx};
        s1.connect(withTcp, yield).value();
        auto welcome = s1.join(testRealm, yield).value();
        s2.connect(withTcp, yield).value();
        s2.join(testRealm, yield).value();

        {
            INFO("Subscribe authorized");

            // First subscription to topic generates cache entry.
            auth->clear(true);
            auto sub1 = s1.subscribe(Topic{"topic1"}, [](Event){}, yield);
            CHECK(auth->topic.uri() == "topic1"); // Not already cached
            CHECK(auth->info.sessionId() == welcome.sessionId());
            CHECK(sub1.has_value());

            // Make s1 unsubscribe and re-subscribe to same topic.
            // But first add subscription to same topic from another session
            // so that cache entry is not removed while s1 unsubscribes.
            auto sub2 = s2.subscribe(Topic{"topic1"},
                                     [](Event){}, yield).value();
            s1.unsubscribe(*sub1, yield).value();
            auth->clear(true);
            sub1 = s1.subscribe(Topic{"topic1"}, [](Event){}, yield);
            CHECK(auth->topic.uri() == "empty"); // Already cached
            CHECK(auth->info.sessionId() == 0);
            CHECK(sub1.has_value());

            // Removing all subscriptions to topic should remove that topic
            // from the cache.
            s1.unsubscribe(*sub1, yield).value();
            s2.unsubscribe(sub2, yield).value();
            auth->clear(true);
            sub1 = s1.subscribe(Topic{"topic1"}, [](Event){}, yield);
            CHECK(auth->topic.uri() == "topic1"); // Not already cached
            CHECK(auth->info.sessionId() == welcome.sessionId());
            CHECK(sub1.has_value());
        }

        {
            INFO("Subscribe denied");

            // First subscription to topic generates cache entry.
            auth->clear(true);
            auth->canSubscribe = Authorization::denied();
            auto sub = s1.subscribe(Topic{"topic2"}, [](Event){}, yield);
            CHECK(auth->topic.uri() == "topic2");
            CHECK(auth->info.sessionId() == welcome.sessionId());
            REQUIRE_FALSE(sub.has_value());
            CHECK(sub.error() == WampErrc::authorizationDenied);

            // Second subscription attempt should be already cached
            auth->clear(true);
            auth->canSubscribe = Authorization::denied();
            sub = s1.subscribe(Topic{"topic2"}, [](Event){}, yield);
            CHECK(auth->topic.uri() == "empty"); // Already cached
            CHECK(auth->info.sessionId() == 0);
            REQUIRE_FALSE(sub.has_value());
            CHECK(sub.error() == WampErrc::authorizationDenied);
        }

        {
            INFO("Subscribe authorization failed");

            // First subscription to topic generates cache entry.
            auth->clear(true);
            auth->canSubscribe = Authorization::failed();
            auto sub = s1.subscribe(Topic{"topic3"}, [](Event){}, yield);
            CHECK(auth->topic.uri() == "topic3");
            CHECK(auth->info.sessionId() == welcome.sessionId());
            REQUIRE_FALSE(sub.has_value());
            CHECK(sub.error() == WampErrc::authorizationFailed);

            // Second subscription attempt should be already cached
            auth->clear(true);
            auth->canSubscribe = Authorization::failed();
            sub = s1.subscribe(Topic{"topic3"}, [](Event){}, yield);
            CHECK(auth->topic.uri() == "empty"); // Already cached
            CHECK(auth->info.sessionId() == 0);
            REQUIRE_FALSE(sub.has_value());
            CHECK(sub.error() == WampErrc::authorizationFailed);
        }

        {
            INFO("Subscribe to meta-topic authorized");

            // First subscription to topic generates cache entry.
            auth->clear(true);
            auto sub1 = s1.subscribe(Topic{"wamp.session.on_join"},
                                     [](Event){}, yield);
            CHECK(auth->topic.uri() == "wamp.session.on_join");
            CHECK(auth->info.sessionId() == welcome.sessionId());
            CHECK(sub1.has_value());

            // Make s1 unsubscribe and re-subscribe to same meta-topic.
            // But first add subscription to same topic from another session
            // so that cache entry is not removed while s1 unsubscribes.
            auto sub2 = s2.subscribe(Topic{"wamp.session.on_join"},
                                     [](Event){}, yield).value();
            s1.unsubscribe(*sub1, yield).value();
            auth->clear(true);
            sub1 = s1.subscribe(Topic{"wamp.session.on_join"},
                                [](Event){}, yield);
            CHECK(auth->topic.uri() == "empty"); // Already cached
            CHECK(auth->info.sessionId() == 0);
            CHECK(sub1.has_value());

            // Removing all subscriptions to meta-topic should remove that
            // topic from the cache.
            s1.unsubscribe(*sub1, yield).value();
            s2.unsubscribe(sub2, yield).value();
            auth->clear(true);
            sub1 = s1.subscribe(Topic{"wamp.session.on_join"},
                                [](Event){}, yield);
            CHECK(auth->topic.uri() == "wamp.session.on_join"); // Not cached
            CHECK(auth->info.sessionId() == welcome.sessionId());
            CHECK(sub1.has_value());
        }

        {
            INFO("Publish authorized");

            // First publish generates cache entry
            auth->clear(true);
            event = {};
            s1.subscribe(Topic{"topic5"}, onEvent, yield).value();
            auto ack = s1.publish(Pub{"topic5"}.withArgs(42)
                                               .withExcludeMe(false), yield);
            CHECK(ack.has_value());
            CHECK(auth->pub.uri() == "topic5");
            CHECK(auth->info.sessionId() == welcome.sessionId());
            while (event.args().empty())
                test::suspendCoro(yield);
            CHECK_FALSE(event.publisher().has_value());

            // Second publish authorization should already be cached
            auth->clear(true);
            event = {};
            s1.subscribe(Topic{"topic5"}, onEvent, yield).value();
            ack = s1.publish(Pub{"topic5"}.withArgs(43)
                                          .withExcludeMe(false), yield);
            CHECK(ack.has_value());
            CHECK(auth->pub.uri() == "empty"); // Already cached
            CHECK(auth->info.sessionId() == 0);
            while (event.args().empty())
                test::suspendCoro(yield);
            CHECK_FALSE(event.publisher().has_value());
        }

        {
            INFO("Publish authorized with overriden disclosure rule");

            // First publish generates cache entry
            auth->clear(true);
            auth->canPublish = Authorization::granted(Disclosure::reveal);
            event = {};
            s1.subscribe(Topic{"topic7"}, onEvent, yield).value();
            auto ack = s1.publish(Pub{"topic7"}.withArgs(42)
                                               .withExcludeMe(false), yield);
            CHECK(ack.has_value());
            CHECK(auth->pub.uri() == "topic7");
            CHECK(auth->info.sessionId() == welcome.sessionId());

            while (event.args().empty())
                test::suspendCoro(yield);
            CHECK(event.publisher() == welcome.sessionId());

            // Second publish authorization should already be cached
            auth->clear(true);
            auth->canPublish = Authorization::granted(Disclosure::reveal);
            event = {};
            s1.subscribe(Topic{"topic7"}, onEvent, yield).value();
            ack = s1.publish(Pub{"topic7"}.withArgs(42)
                                          .withExcludeMe(false), yield);
            CHECK(ack.has_value());
            CHECK(auth->pub.uri() == "empty"); // Already cached
            CHECK(auth->info.sessionId() == 0);

            while (event.args().empty())
                test::suspendCoro(yield);
            CHECK(event.publisher() == welcome.sessionId());
        }

        {
            INFO("Register authorized");

            // First registration generates cache entry.
            auth->clear(true);
            auto reg = s1.enroll(Procedure{"rpc1"},
                                [](Invocation) -> Outcome{return Result{};},
                                yield);
            CHECK(auth->proc.uri() == "rpc1");
            CHECK(auth->info.sessionId() == welcome.sessionId());
            CHECK(reg.has_value());

            // Second registration authorization should be already cached
            s1.unregister(*reg, yield).value();
            auth->clear(true);
            reg = s1.enroll(Procedure{"rpc1"},
                [](Invocation) -> Outcome{return Result{};},
                yield);
            CHECK(auth->proc.uri() == "empty"); // Already cached
            CHECK(auth->info.sessionId() == 0);
            CHECK(reg.has_value());
        }

        {
            INFO("Call authorized");

            // First call generates cache entry.
            auth->clear(true);
            invocation = {};
            auto reg = s1.enroll(Procedure{"rpc3"}, onInvocation,
                                 yield).value();
            auto result = s1.call(Rpc{"rpc3"}.withArgs(42), yield);
            CHECK(result.has_value());
            CHECK(auth->rpc.uri() == "rpc3");
            CHECK(auth->info.sessionId() == welcome.sessionId());
            while (invocation.args().empty())
                test::suspendCoro(yield);
            REQUIRE(invocation.caller().has_value());
            CHECK(invocation.caller().value() == welcome.sessionId());

            // Second call authorization should be already cached
            auth->clear(true);
            invocation = {};
            result = s1.call(Rpc{"rpc3"}.withArgs(43), yield);
            CHECK(result.has_value());
            CHECK(auth->rpc.uri() == "empty"); // Already cached
            CHECK(auth->info.sessionId() == 0);
            while (invocation.args().empty())
                test::suspendCoro(yield);
            REQUIRE(invocation.caller().has_value());
            CHECK(invocation.caller().value() == welcome.sessionId());

            // Unregistering should clear the URI cache entry
            s1.unregister(reg, yield).value();
            s1.enroll(Procedure{"rpc3"}, onInvocation, yield).value();
            result = s1.call(Rpc{"rpc3"}.withArgs(42), yield);
            CHECK(result.has_value());
            CHECK(auth->rpc.uri() == "rpc3"); // Not cached
            CHECK(auth->info.sessionId() == welcome.sessionId());
            while (invocation.args().empty())
                test::suspendCoro(yield);
            REQUIRE(invocation.caller().has_value());
            CHECK(invocation.caller().value() == welcome.sessionId());
        }

        {
            INFO("Call meta-procedure authorized");

            // First call generates cache entry.
            auth->clear(true);
            invocation = {};
            auto result = s1.call(Rpc{"wamp.session.count"}, yield);
            REQUIRE(result.has_value());
            REQUIRE_FALSE(result.value().args().empty());
            CHECK(result.value().args().at(0) == 2);
            CHECK(auth->rpc.uri() == "wamp.session.count");
            CHECK(auth->info.sessionId() == welcome.sessionId());
\
            // Second call authorization should be already cached
            auth->clear(true);
            invocation = {};
            result = s1.call(Rpc{"wamp.session.count"}, yield);
            REQUIRE(result.has_value());
            REQUIRE_FALSE(result.value().args().empty());
            CHECK(result.value().args().at(0) == 2);
            CHECK(auth->rpc.uri() == "empty"); // Already cached
            CHECK(auth->info.sessionId() == 0);
        }

        {
            INFO("Session leaving");

            // Use every cachable operation
            auth->clear(true);

            // Leave session and check cache was cleared
        }

        s1.disconnect();
    });

    ioctx.run();
}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
