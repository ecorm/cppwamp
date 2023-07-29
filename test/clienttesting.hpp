/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <algorithm>
#include <atomic>
#include <cctype>
#include <mutex>
#include <thread>
#include <boost/asio/thread_pool.hpp>
#include <catch2/catch.hpp>
#include <cppwamp/config.hpp>
#include <cppwamp/corounpacker.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/msgpack.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/spawn.hpp>
#include <cppwamp/unpacker.hpp>
#include <cppwamp/transports/tcp.hpp>

#ifdef CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS
    #include <cppwamp/transports/uds.hpp>
#endif

using namespace wamp;

namespace test
{

const std::string testRealm = "cppwamp.test";
const unsigned short validPort = 12345;
const unsigned short invalidPort = 54321;
const auto withTcp = TcpHost("localhost", validPort).withFormat(json);
const auto invalidTcp = TcpHost("localhost", invalidPort).withFormat(json);

//------------------------------------------------------------------------------
inline void suspendCoro(YieldContext yield)
{
    auto exec = boost::asio::get_associated_executor(yield);
    boost::asio::post(exec, yield);
}

//------------------------------------------------------------------------------
template <typename TDelegate>
void checkInvalidUri(TDelegate&& delegate, bool joined = true)
{
    IoContext ioctx;
    Session session(ioctx);
    spawn(ioctx, [&](YieldContext yield)
    {
        session.connect(withTcp, yield).value();
        if (joined)
            session.join(Petition(testRealm), yield).value();
        auto result = delegate(session, yield);
        REQUIRE( !result );
        CHECK( result.error() );
        if (session.state() == SessionState::established)
            CHECK( result.error() == WampErrc::invalidUri );
        CHECK_THROWS_AS( result.value(), error::Failure );
        session.disconnect();
    });

    ioctx.run();
}

//------------------------------------------------------------------------------
template <typename TResult, typename TDelegate>
void checkDisconnect(TDelegate&& delegate)
{
    bool completed = false;
    ErrorOr<TResult> result;
    IoContext ioctx;
    Session session(ioctx);
    spawn(ioctx, [&](YieldContext yield)
    {
        session.connect(withTcp, yield).value();
        delegate(session, yield, completed, result);
        session.disconnect();
        CHECK( session.state() == SessionState::disconnected );
    });

    ioctx.run();
    CHECK( completed );
    CHECK( result == makeUnexpected(MiscErrc::abandoned) );
    CHECK_THROWS_AS( result.value(), error::Failure );
}

//------------------------------------------------------------------------------
struct PubSubFixture
{
    using PubVec = std::vector<PublicationId>;

    PubSubFixture(IoContext& ioctx, ConnectionWish wish)
        : ioctx(ioctx),
          where(std::move(wish)),
          publisher(ioctx),
          subscriber(ioctx),
          otherSubscriber(ioctx)
    {}

    void join(YieldContext yield)
    {
        publisher.connect(where, yield).value();
        publisher.join(Petition(testRealm), yield).value();
        subscriber.connect(where, yield).value();
        subscriber.join(Petition(testRealm), yield).value();
        otherSubscriber.connect(where, yield).value();
        otherSubscriber.join(Petition(testRealm), yield).value();
    }

    void subscribe(YieldContext yield)
    {
        using namespace std::placeholders;
        dynamicSub = subscriber.subscribe(
            Topic("str.num"),
            [this](Event ev) {onDynamicEvent(std::move(ev));},
            yield).value();

        staticSub = subscriber.subscribe(
            Topic("str.num"),
            unpackedEvent<std::string, int>(
                [this](Event ev, std::string s, int n)
                {
                    onStaticEvent(std::move(ev), std::move(s), n);
                }),
            yield).value();

        otherSub = otherSubscriber.subscribe(
            Topic("other"),
            [this](Event ev) {onOtherEvent(std::move(ev));},
            yield).value();
    }

    void onDynamicEvent(Event event)
    {
        INFO( "in onDynamicEvent" );
        CHECK( event.publicationId() <= 9007199254740992ll );
        CHECK( event.executor() == ioctx.get_executor() );
        dynamicArgs = event.args();
        dynamicPubs.push_back(event.publicationId());
    }

    void onStaticEvent(Event event, std::string str, int num)
    {
        INFO( "in onStaticEvent" );
        CHECK( event.publicationId() <= 9007199254740992ll );
        CHECK( event.executor() == ioctx.get_executor() );
        staticArgs = Array{{str, num}};
        staticPubs.push_back(event.publicationId());
    }

    void onOtherEvent(Event event)
    {
        INFO( "in onOtherEvent" );
        CHECK( event.publicationId() <= 9007199254740992ll );
        CHECK( event.executor() == ioctx.get_executor() );
        otherPubs.push_back(event.publicationId());
    }

    IoContext& ioctx;
    ConnectionWish where;

    Session publisher;
    Session subscriber;
    Session otherSubscriber;

    ScopedSubscription dynamicSub;
    ScopedSubscription staticSub;
    ScopedSubscription otherSub;

    PubVec dynamicPubs;
    PubVec staticPubs;
    PubVec otherPubs;

    Array dynamicArgs;
    Array staticArgs;
};

} // namespace test
