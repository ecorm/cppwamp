/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <iostream>
#include <cppwamp/session.hpp>
#include <cppwamp/spawn.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcp.hpp>
#include <cppwamp/unpacker.hpp>

const std::string realm = "cppwamp.examples";
const std::string address = "localhost";
const short port = 12345;

//------------------------------------------------------------------------------
class Producer
{
public:
    explicit Producer(wamp::AnyIoExecutor exec)
        : session_(std::move(exec))
    {}

    void start(wamp::ConnectionWishList where, wamp::YieldContext yield)
    {
        using namespace wamp;
        auto index = session_.connect(std::move(where), yield).value();
        std::cout << "Producer connected on transport #"
                  << (index + 1) << std::endl;

        auto info = session_.join(Petition(realm), yield).value();
        std::cout << "Producer joined, session ID = "
                  << info.sessionId() << std::endl;

        using namespace std::placeholders;

        registration_ = session_.enroll(
            Stream("feed").withInvitationExpected(),
            [this](Channel channel) {onStream(std::move(channel));},
            yield).value();
    }

    void quit(wamp::YieldContext yield)
    {
        registration_.unregister();
        session_.leave(wamp::Reason(), yield).value();
        session_.disconnect();
    }

private:
    using Channel = wamp::CalleeChannel;

    void onStream(Channel channel)
    {
        using Chunk = wamp::CalleeOutputChunk;
        std::cout << "Producer received invitation: "
                  << channel.invitation().args() << std::endl;
        channel.respond(Chunk().withArgs("playing")).value();
        channel.send(Chunk().withArgs("one")).value();
        channel.send(Chunk().withArgs("two")).value();
        channel.send(Chunk(true).withArgs("three")).value();
    }

    wamp::Session session_;
    wamp::ScopedRegistration registration_;
};


//------------------------------------------------------------------------------
class Consumer
{
public:
    Consumer(wamp::AnyIoExecutor exec)
        : session_(std::move(exec))
    {}

    void join(wamp::ConnectionWishList where, wamp::YieldContext yield)
    {
        using namespace wamp;

        auto index = session_.connect(std::move(where), yield).value();
        std::cout << "Consumer connected on transport #"
                  << (index + 1) << std::endl;

        auto info = session_.join(Petition(realm), yield).value();
        std::cout << "Consumer joined, session ID = "
                  << info.sessionId() << std::endl;
    }

    void consumeFeed(wamp::YieldContext yield)
    {
        auto mode = wamp::StreamMode::calleeToCaller;
        auto channel = session_.requestStream(
            wamp::StreamRequest("feed", mode).withArgs("play"),
            [this](Channel chan, wamp::ErrorOr<Chunk> chunk)
                {onChunk(std::move(chan), std::move(chunk));},
            yield).value();
        std::cout << "Consumer got RSVP: " << channel.rsvp().args()
                  << " on channel " << channel.id() << std::endl;

        while (!done_)
            boost::asio::post(session_.executor(), yield);
    }

    void leave(wamp::YieldContext yield)
    {
        session_.leave(wamp::Reason(), yield).value();
        session_.disconnect();
    }

private:
    using Channel = wamp::CallerChannel;
    using Chunk = wamp::CallerInputChunk;

    void onChunk(Channel channel, wamp::ErrorOr<Chunk> chunk)
    {
        std::cout << "Consumer got chunk: " << chunk.value().args()
                  << " on channel " << channel.id() << std::endl;
        done_ = chunk->isFinal();
    }

    wamp::Session session_;
    bool done_ = false;
};


//------------------------------------------------------------------------------
int main()
{
    wamp::IoContext ioctx;

    auto tcp = wamp::TcpHost("localhost", 12345).withFormat(wamp::json);

    // Normally, the service and client instances would be in separate programs.
    // We run them all here in the same coroutine for demonstration purposes.
    Producer service(ioctx.get_executor());
    Consumer client(ioctx.get_executor());

    wamp::spawn(ioctx, [&](wamp::YieldContext yield)
    {
        service.start({tcp}, yield);
        client.join({tcp}, yield);
        client.consumeFeed(yield);
        client.leave(yield);
        service.quit(yield);
    });

    ioctx.run();

    return 0;
}
