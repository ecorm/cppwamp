/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <iostream>
#include <cppwamp/json.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/spawn.hpp>
#include <cppwamp/tcp.hpp>
#include <cppwamp/unpacker.hpp>

const std::string realm = "cppwamp.demo.streaming";
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
                  << (index + 1) << "\n";

        auto info = session_.join(Realm(realm), yield).value();
        std::cout << "Producer joined, session ID = "
                  << info.id() << "\n";

        using namespace std::placeholders;

        registration_ = session_.enroll(
            Stream("feed"),
            [this](ChannelPtr channel) {onStream(channel);},
            yield).value();
    }

    void quit(wamp::YieldContext yield)
    {
        registration_.unregister();
        session_.leave(wamp::Reason(), yield).value();
        session_.disconnect();
    }

private:
    using ChannelPtr = wamp::CalleeChannel::Ptr;

    void onStream(ChannelPtr channel)
    {
        std::cout << "Producer received invitation: "
                  << channel->invitation().args() << "\n";
        channel->accept(wamp::CalleeOutputChunk().withArgs("playing"));
        channel->send(wamp::CalleeOutputChunk().withArgs("one"));
        channel->send(wamp::CalleeOutputChunk().withArgs("two"));
        channel->send(wamp::CalleeOutputChunk(true).withArgs("three"));
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
                  << (index + 1) << "\n";

        auto info = session_.join(Realm(realm), yield).value();
        std::cout << "Consumer joined, session ID = " << info.id() << "\n";
    }

    void consumeFeed(wamp::YieldContext yield)
    {
        auto mode = wamp::StreamMode::calleeToCaller;
        auto channelOrError = session_.invite(
            wamp::Invitation("feed", mode).withArgs("play"),
            [this](ChannelPtr channel,
                   wamp::ErrorOr<wamp::CallerInputChunk> chunk)
                {onChunk(channel, std::move(chunk));},
            yield);
        auto channel = channelOrError.value();
        std::cout << "Consumer got RSVP: " << channel->rsvp().args() << "\n";
    }

    void leave(wamp::YieldContext yield)
    {
        session_.leave(wamp::Reason(), yield).value();
        session_.disconnect();
    }

private:
    using ChannelPtr = wamp::CallerChannel::Ptr;

    void onChunk(ChannelPtr chanel, wamp::ErrorOr<wamp::CallerInputChunk> chunk)
    {
        std::cout << "Consumer got chunk: " << chunk.value().args() << "\n";
    }

    wamp::Session session_;
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
