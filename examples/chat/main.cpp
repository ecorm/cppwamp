/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include <iostream>
#include <boost/asio/spawn.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/tcp.hpp>
#include <cppwamp/unpacker.hpp>

const std::string realm = "cppwamp.demo.chat";
const std::string address = "localhost";
const short port = 12345;

//------------------------------------------------------------------------------
class ChatService
{
public:
    using Yield = boost::asio::yield_context;

    explicit ChatService(wamp::AsioContext& ioctx)
        : ioctx_(ioctx)
    {}

    void start(wamp::ConnectorList connectors, Yield yield)
    {
        using namespace wamp;
        session_ = Session::create(ioctx_, connectors);

        auto index = session_->connect(yield).value();
        std::cout << "Chat service connected on transport #"
                  << (index + 1) << "\n";

        auto info = session_->join(Realm(realm), yield).value();
        std::cout << "Chat service joined, session ID = " << info.id() << "\n";

        using namespace std::placeholders;

        registration_ = session_->enroll(
            Procedure("say"),
            simpleRpc<void, std::string, std::string>(
                                std::bind(&ChatService::say, this, _1, _2)),
            yield).value();
    }

    void quit(Yield yield)
    {
        registration_.unregister();
        session_->leave(wamp::Reason(), yield).value();
        session_->disconnect();
    }

private:
    void say(std::string user, std::string message)
    {
        // Rebroadcast message to all subscribers
        session_->publish( wamp::Pub("said").withArgs(user, message) );
    }

    wamp::AsioContext& ioctx_;
    wamp::Session::Ptr session_;
    wamp::ScopedRegistration registration_;
};


//------------------------------------------------------------------------------
class ChatClient
{
public:
    using Yield = boost::asio::yield_context;

    ChatClient(wamp::AsioContext& ioctx, std::string user)
        : ioctx_(ioctx),
          user_(std::move(user))
    {}

    void join(wamp::ConnectorList connectors, Yield yield)
    {
        using namespace wamp;
        session_ = Session::create(ioctx_, connectors);

        auto index = session_->connect(yield).value();
        std::cout << user_ << " connected on transport #" << index << "\n";

        auto info = session_->join(Realm(realm), yield).value();
        std::cout << user_ << " joined, session ID = " << info.id() << "\n";

        using namespace std::placeholders;
        subscription_ = session_->subscribe(
                Topic("said"),
                simpleEvent<std::string, std::string>(
                        std::bind(&ChatClient::said, this, _1, _2)),
                yield).value();
    }

    void leave(Yield yield)
    {
        subscription_.unsubscribe();
        session_->leave(wamp::Reason(), yield).value();
        session_.reset();
    }

    void say(const std::string& message, Yield yield)
    {
        std::cout << user_ << " says \"" << message << "\"\n";
        session_->call(wamp::Rpc("say").withArgs(user_, message),
                       yield).value();
    }

private:
    void said(std::string from, std::string message)
    {
        std::cout << user_ << " received message from " << from << ": \""
                  << message << "\"\n";
    }

    wamp::AsioContext& ioctx_;
    std::string user_;
    wamp::Session::Ptr session_;
    wamp::ScopedSubscription subscription_;
};


//------------------------------------------------------------------------------
int main()
{
    wamp::AsioContext ioctx;

    auto tcp = wamp::connector<wamp::Json>( ioctx,
                                            wamp::TcpHost("localhost", 12345) );

    // Normally, the service and client instances would be in separate programs.
    // We run them all here in the same coroutine for demonstration purposes.
    ChatService chat(ioctx);
    ChatClient alice(ioctx, "Alice");
    ChatClient bob(ioctx, "Bob");

    boost::asio::spawn(ioctx, [&](boost::asio::yield_context yield)
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
