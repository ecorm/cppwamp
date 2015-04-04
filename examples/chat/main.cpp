/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <iostream>
#include <cppwamp/coroclient.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/legacytcpconnector.hpp>

const std::string realm = "cppwamp.demo.chat";
const std::string address = "localhost";
const short port = 12345;

//------------------------------------------------------------------------------
class ChatService
{
public:
    using Yield = boost::asio::yield_context;

    void start(wamp::ConnectorList connectors, Yield yield)
    {
        client_ = wamp::CoroClient<>::create(connectors);

        auto index = client_->connect(yield);
        std::cout << "Chat service connected on transport #"
                  << (index + 1) << "\n";

        auto sid = client_->join(realm, yield);
        std::cout << "Chat service joined, session ID = " << sid << "\n";

        using namespace std::placeholders;
        registration_ = client_->enroll<std::string, std::string>(
            "say",
            std::bind(&ChatService::say, this, _1, _2, _3),
            yield
        );
    }

    void quit(Yield yield)
    {
        registration_.unregister();
        client_->leave(yield);
        client_->disconnect();
    }

private:
    void say(wamp::Invocation inv, std::string user, std::string message)
    {
        // Rebroadcast message to all subscribers
        client_->publish("said", {user, message});
        inv.yield();
    }

    wamp::CoroClient<>::Ptr client_;
    wamp::Registration registration_;
};


//------------------------------------------------------------------------------
class ChatClient
{
public:
    using Yield = boost::asio::yield_context;

    explicit ChatClient(std::string user) : user_(std::move(user)) {}

    void join(wamp::ConnectorList connectors, Yield yield)
    {
        client_ = wamp::CoroClient<>::create(connectors);

        auto index = client_->connect(yield);
        std::cout << user_ << " connected on transport #" << index << "\n";

        auto sid = client_->join(realm, yield);
        std::cout << user_ << " joined, session ID = " << sid << "\n";

        using namespace std::placeholders;
        subscription_ = client_->subscribe<std::string, std::string>(
                "said",
                std::bind(&ChatClient::said, this, _1, _2, _3),
                yield);
    }

    void leave(Yield yield)
    {
        subscription_.unsubscribe();
        client_->leave(yield);
        client_.reset();
    }

    void say(const std::string& message, Yield yield)
    {
        std::cout << user_ << " says \"" << message << "\"\n";
        client_->call("say", {user_, message}, yield);
    }

private:
    void said(wamp::PublicationId, std::string from, std::string message)
    {
        std::cout << user_ << " received message from " << from << ": \""
                  << message << "\"\n";
    }

    std::string user_;
    wamp::CoroClient<>::Ptr client_;
    wamp::Subscription subscription_;
};


//------------------------------------------------------------------------------
int main()
{
    wamp::AsioService iosvc;
    auto tcp = wamp::legacy::TcpConnector::create(iosvc, "localhost",
                                                  12345, wamp::Json::id());

    // Normally, the service and client instances would be in separate programs.
    // We run them all here in the same coroutine for demonstration purposes.
    ChatService chat;
    ChatClient alice("Alice");
    ChatClient bob("Bob");

    boost::asio::spawn(iosvc, [&](boost::asio::yield_context yield)
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

    iosvc.run();

    return 0;
}
