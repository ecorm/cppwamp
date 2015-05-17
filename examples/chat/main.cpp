/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <iostream>
#include <cppwamp/corosession.hpp>
#include <cppwamp/json.hpp>
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

    void start(wamp::ConnectorList connectors, Yield yield)
    {
        using namespace wamp;
        session_ = CoroSession<>::create(connectors);

        auto index = session_->connect(yield);
        std::cout << "Chat service connected on transport #"
                  << (index + 1) << "\n";

        auto info = session_->join(Realm(realm), yield);
        std::cout << "Chat service joined, session ID = " << info.id() << "\n";

        using namespace std::placeholders;

        registration_ = session_->enroll(
            Procedure("say"),
            unpackedRpc<std::string, std::string>(std::bind(&ChatService::say,
                                                            this, _1, _2, _3)),
            yield
        );
    }

    void quit(Yield yield)
    {
        registration_.unregister();
        session_->leave(wamp::Reason(), yield);
        session_->disconnect();
    }

private:
    wamp::Outcome say(wamp::Invocation, std::string user, std::string message)
    {
        // Rebroadcast message to all subscribers
        session_->publish( wamp::Pub("said").withArgs({user, message}) );
        return {};
    }

    wamp::CoroSession<>::Ptr session_;
    wamp::ScopedRegistration registration_;
};


//------------------------------------------------------------------------------
class ChatClient
{
public:
    using Yield = boost::asio::yield_context;

    explicit ChatClient(std::string user) : user_(std::move(user)) {}

    void join(wamp::ConnectorList connectors, Yield yield)
    {
        using namespace wamp;
        session_ = CoroSession<>::create(connectors);

        auto index = session_->connect(yield);
        std::cout << user_ << " connected on transport #" << index << "\n";

        auto info = session_->join(Realm(realm), yield);
        std::cout << user_ << " joined, session ID = " << info.id() << "\n";

        using namespace std::placeholders;
        subscription_ = session_->subscribe(
                Topic("said"),
                unpackedEvent<std::string, std::string>(
                                std::bind(&ChatClient::said, this, _1, _2, _3)),
                yield);
    }

    void leave(Yield yield)
    {
        subscription_.unsubscribe();
        session_->leave(wamp::Reason(), yield);
        session_.reset();
    }

    void say(const std::string& message, Yield yield)
    {
        std::cout << user_ << " says \"" << message << "\"\n";
        session_->call(wamp::Rpc("say").withArgs({user_, message}), yield);
    }

private:
    void said(wamp::Event, std::string from, std::string message)
    {
        std::cout << user_ << " received message from " << from << ": \""
                  << message << "\"\n";
    }

    std::string user_;
    wamp::CoroSession<>::Ptr session_;
    wamp::ScopedSubscription subscription_;
};


//------------------------------------------------------------------------------
int main()
{
    wamp::AsioService iosvc;

#ifdef CPPWAMP_USE_LEGACY_CONNECTORS
    auto tcp = wamp::legacyConnector<wamp::Json>( iosvc,
            wamp::TcpHost("localhost", 12345) );
#else
    auto tcp = wamp::connector<wamp::Json>( iosvc,
                                            wamp::TcpHost("localhost", 12345) );
#endif

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
