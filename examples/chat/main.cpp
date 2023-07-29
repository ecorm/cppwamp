/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <iostream>
#include <cppwamp/session.hpp>
#include <cppwamp/spawn.hpp>
#include <cppwamp/unpacker.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcp.hpp>

const std::string realm = "cppwamp.examples";
const std::string address = "localhost";
const short port = 12345;

//------------------------------------------------------------------------------
class ChatService
{
public:
    explicit ChatService(wamp::AnyIoExecutor exec)
        : session_(std::move(exec))
    {}

    void start(wamp::ConnectionWishList where, wamp::YieldContext yield)
    {
        using namespace wamp;
        auto index = session_.connect(std::move(where), yield).value();
        std::cout << "Chat service connected on transport #"
                  << (index + 1) << "\n";

        auto welcome = session_.join(Petition(realm), yield).value();
        std::cout << "Chat service joined, session ID = "
                  << welcome.sessionId() << "\n";

        using namespace std::placeholders;

        registration_ = session_.enroll(
            Procedure("say"),
            simpleRpc<void, std::string, std::string>(
                [this](std::string user, std::string message)
                {
                    say(std::move(user), std::move(message));
                }),
            yield).value();
    }

    void quit(wamp::YieldContext yield)
    {
        registration_.unregister();
        session_.leave(wamp::Reason(), yield).value();
        session_.disconnect();
    }

private:
    void say(std::string user, std::string message)
    {
        // Rebroadcast message to all subscribers
        session_.publish( wamp::Pub("said").withArgs(user, message) );
    }

    wamp::Session session_;
    wamp::ScopedRegistration registration_;
};


//------------------------------------------------------------------------------
class ChatClient
{
public:
    ChatClient(wamp::AnyIoExecutor exec, std::string user)
        : session_(std::move(exec)),
          user_(std::move(user))
    {}

    void join(wamp::ConnectionWishList where, wamp::YieldContext yield)
    {
        using namespace wamp;

        auto index = session_.connect(std::move(where), yield).value();
        std::cout << user_ << " connected on transport #" << (index + 1) << "\n";

        auto welcome = session_.join(Petition(realm), yield).value();
        std::cout << user_ << " joined, session ID = "
                  << welcome.sessionId() << "\n";

        using namespace std::placeholders;
        subscription_ = session_.subscribe(
                Topic("said"),
                simpleEvent<std::string, std::string>(
                    [this](std::string user, std::string message)
                    {
                        said(user, message);
                    }),
                yield).value();
    }

    void leave(wamp::YieldContext yield)
    {
        subscription_.unsubscribe();
        session_.leave(wamp::Reason(), yield).value();
        session_.disconnect();
    }

    void say(const std::string& message, wamp::YieldContext yield)
    {
        std::cout << user_ << " says '" << message << "'\n";
        session_.call(wamp::Rpc("say").withArgs(user_, message),
                       yield).value();
    }

private:
    void said(std::string from, std::string message)
    {
        std::cout << user_ << " received message from " << from << ": '"
                  << message << "'\n";
    }

    wamp::Session session_;
    std::string user_;
    wamp::ScopedSubscription subscription_;
};


//------------------------------------------------------------------------------
int main()
{
    wamp::IoContext ioctx;

    auto tcp = wamp::TcpHost("localhost", 12345).withFormat(wamp::json);

    // Normally, the service and client instances would be in separate programs.
    // We run them all here in the same coroutine for demonstration purposes.
    ChatService chat(ioctx.get_executor());
    ChatClient alice(ioctx.get_executor(), "Alice");
    ChatClient bob(ioctx.get_executor(), "Bob");

    wamp::spawn(ioctx, [&](wamp::YieldContext yield)
    {
        chat.start({tcp}, yield);

        alice.join({tcp}, yield);
        alice.say("Hello?", yield);

        bob.join({tcp}, yield);

        alice.say("Is anybody there?", yield);
        bob.say("Yes, I'm here!", yield);

        alice.leave(yield);
        bob.leave(yield);

        chat.quit(yield);
    });

    ioctx.run();

    return 0;
}
