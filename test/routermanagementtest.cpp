/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#if defined(CPPWAMP_TEST_HAS_CORO)

#include <catch2/catch.hpp>
#include <cppwamp/asiodefs.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/spawn.hpp>
#include <cppwamp/tcp.hpp>
#include "routerfixture.hpp"

using namespace wamp;

namespace Matchers = Catch::Matchers;

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

//------------------------------------------------------------------------------
struct TestRealmObserver : public RealmObserver
{
    static std::shared_ptr<TestRealmObserver> create()
    {
        return std::make_shared<TestRealmObserver>();
    }

    void onRealmClosed(Uri u) override
    {
        realmClosedEvents.push_back(std::move(u));
    }

    void onJoin(SessionDetails s) override
    {
        joinEvents.push_back(std::move(s));
    }

    void onLeave(SessionDetails s) override
    {
        leaveEvents.push_back(std::move(s));
    }

    void onRegister(SessionDetails s, RegistrationDetails r) override
    {
        registerEvents.push_back({std::move(s), std::move(r)});
    }

    void onUnregister(SessionDetails s, RegistrationDetails r) override
    {
        unregisterEvents.push_back({std::move(s), std::move(r)});
    }

    void onSubscribe(SessionDetails s, SubscriptionDetails d) override
    {
        subscribeEvents.push_back({std::move(s), std::move(d)});
    }

    void onUnsubscribe(SessionDetails s, SubscriptionDetails d) override
    {
        unsubscribeEvents.push_back({std::move(s), std::move(d)});
    }

    void clear()
    {
        realmClosedEvents.clear();
        joinEvents.clear();
        leaveEvents.clear();
        registerEvents.clear();
        unregisterEvents.clear();
        subscribeEvents.clear();
        unsubscribeEvents.clear();
    }

    std::vector<Uri> realmClosedEvents;
    std::vector<SessionDetails> joinEvents;
    std::vector<SessionDetails> leaveEvents;
    std::vector<std::pair<SessionDetails, RegistrationDetails>> registerEvents;
    std::vector<std::pair<SessionDetails, RegistrationDetails>> unregisterEvents;
    std::vector<std::pair<SessionDetails, SubscriptionDetails>> subscribeEvents;
    std::vector<std::pair<SessionDetails, SubscriptionDetails>> unsubscribeEvents;
};

//------------------------------------------------------------------------------
struct RouterLogLevelGuard
{
    explicit RouterLogLevelGuard(LogLevel level) : level_(level) {}

    ~RouterLogLevelGuard()
    {
        test::RouterFixture::instance().router().setLogLevel(level_);
    }

private:
    LogLevel level_;
};

} // anomymous namespace


//------------------------------------------------------------------------------
TEST_CASE( "Router realm management", "[WAMP][Router]" )
{
    if (!test::RouterFixture::enabled())
        return;

    auto& theRouter = test::RouterFixture::instance().router();
    RouterLogLevelGuard logLevelGuard{theRouter.logLevel()};
    theRouter.setLogLevel(LogLevel::off);
    IoContext ioctx;

    spawn(ioctx, [&](YieldContext yield)
    {
        {
            INFO("Opening already open realm");
            auto realmOrError = theRouter.openRealm({"cppwamp.test"});
            CHECK(realmOrError == makeUnexpectedError(MiscErrc::alreadyExists));
        }

        {
            INFO("Closing non-existing realm");
            bool closed = theRouter.closeRealm("bogus");
            CHECK_FALSE(closed);
        }

        {
            INFO("Accessing non-existing realm");
            auto found = theRouter.realmAt("bogus");
            REQUIRE_FALSE(found.has_value());
            CHECK(found == makeUnexpectedError(WampErrc::noSuchRealm));
        }

        {
            INFO("Opening, accessing, and closing a realm");
            Uri uri{"cppwamp.test2"};
            auto realmOrError = theRouter.openRealm({uri});
            REQUIRE(realmOrError.has_value());

            Realm realm;
            CHECK_FALSE(bool(realm));
            realm = realmOrError.value();
            CHECK(bool(realm));
            CHECK(realm.isOpen());
            CHECK(realm.uri() == uri);

            auto found = theRouter.realmAt(uri);
            REQUIRE(found.has_value());
            CHECK(found->isOpen());
            CHECK(found->uri() == uri);

            auto observer = TestRealmObserver::create();
            realm.observe(observer, ioctx.get_executor());

            bool closed = theRouter.closeRealm(uri);
            CHECK(closed);

            while (realm.isOpen() || observer->realmClosedEvents.empty())
                suspendCoro(yield);
            CHECK_THAT(observer->realmClosedEvents,
                       Matchers::Equals(std::vector<Uri>({uri})));

            realm = {};
            CHECK_FALSE(bool(realm));

            ioctx.stop();
        }
    });

    ioctx.run();
}

//------------------------------------------------------------------------------
TEST_CASE( "Router realm session events", "[WAMP][Router]" )
{
    if (!test::RouterFixture::enabled())
        return;

    auto& theRouter = test::RouterFixture::instance().router();
    RouterLogLevelGuard logLevelGuard{theRouter.logLevel()};
    theRouter.setLogLevel(LogLevel::off);

    IoContext ioctx;
    Session s{ioctx};

    auto observer = TestRealmObserver::create();
    auto realm = theRouter.realmAt(testRealm).value();
    realm.observe(observer, ioctx.get_executor());

    spawn(ioctx, [&](YieldContext yield)
    {
        s.connect(withTcp, yield).value();
        auto welcome = s.join(Petition(testRealm), yield).value();

        while (observer->joinEvents.empty())
            suspendCoro(yield);

        REQUIRE(observer->joinEvents.size() == 1);
        const auto& joined = observer->joinEvents.front();
        CHECK(joined.authInfo.realmUri()  == testRealm);
        CHECK(joined.authInfo.id()        == welcome.authId());
        CHECK(joined.authInfo.method()    == welcome.authMethod());
        CHECK(joined.authInfo.provider()  == welcome.authProvider());
        CHECK(joined.authInfo.role()      == welcome.authRole());
        CHECK(joined.authInfo.sessionId() == welcome.sessionId());
        CHECK(joined.features.supports(ClientFeatures::provided()));

        s.leave(yield).value();

        while (observer->leaveEvents.empty())
            suspendCoro(yield);

        REQUIRE(observer->leaveEvents.size() == 1);
        const auto& left = observer->leaveEvents.front();
        CHECK(left.authInfo.realmUri()  == testRealm);
        CHECK(left.authInfo.id()        == welcome.authId());
        CHECK(left.authInfo.method()    == welcome.authMethod());
        CHECK(left.authInfo.provider()  == welcome.authProvider());
        CHECK(left.authInfo.role()      == welcome.authRole());
        CHECK(left.authInfo.sessionId() == welcome.sessionId());
        CHECK(left.features.supports(ClientFeatures::provided()));

        s.disconnect();
    });

    ioctx.run();
}

#endif // defined(CPPWAMP_TEST_HAS_CORO)
