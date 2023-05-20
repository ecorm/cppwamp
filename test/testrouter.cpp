/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "testrouter.hpp"
#include <fstream>
#include <iostream>
#include <thread>
#include <cppwamp/json.hpp>
#include <cppwamp/msgpack.hpp>
#include <cppwamp/router.hpp>
#include <cppwamp/tcp.hpp>
#include <cppwamp/uds.hpp>
#include <cppwamp/utils/consolelogger.hpp>
#include <cppwamp/utils/logsequencer.hpp>

namespace test
{

//------------------------------------------------------------------------------
class TicketAuthenticator : public wamp::Authenticator
{
public:
    TicketAuthenticator() = default;

protected:
    void authenticate(wamp::AuthExchange::Ptr ex) override
    {
        if (ex->challengeCount() == 0)
        {
            if (ex->hello().authId().value_or("") == "alice")
                ex->challenge(wamp::Challenge("ticket"));
            else
                ex->reject();
        }
        else if (ex->challengeCount() == 1)
        {
            if (ex->authentication().signature() == "password123")
                ex->welcome({"alice", "ticketrole", "ticket", "static"});
            else
                ex->reject();
        }
        else
        {
            ex->reject();
        }
    }
};

//------------------------------------------------------------------------------
struct Router::Impl
{
    using AccessLogHandler = std::function<void (wamp::AccessLogEntry)>;

    Impl()
        : logger_(ioctx_, wamp::utils::ColorConsoleLogger{true}),
          router_(ioctx_, routerConfig(*this)),
          thread_([this](){run();})
    {}

    ~Impl()
    {
        router_.close();
        thread_.join();
    }

    AccessLogGuard attachToAccessLog(AccessLogHandler handler)
    {
        accessLogHandler_ = std::move(handler);
        return AccessLogGuard{};
    }

    void detachFromAccessLog()
    {
        accessLogHandler_ = nullptr;
    }

private:
    using Logger = wamp::utils::LogSequencer;

    static wamp::RouterConfig routerConfig(Impl& self)
    {
        return wamp::RouterConfig()
            .withLogHandler(self.logger_)
            .withLogLevel(wamp::LogLevel::info)
            .withAccessLogHandler(
                [&self](wamp::AccessLogEntry a)
                {
                    self.onAccessLogEntry(std::move(a));
                });
    }

    static wamp::ServerConfig tcpConfig()
    {
        return wamp::ServerConfig("tcp12345", wamp::TcpEndpoint{12345},
                                  wamp::json);
    }

    static wamp::ServerConfig tcpTicketConfig()
    {
        return wamp::ServerConfig("tcp23456", wamp::TcpEndpoint{23456},
                                  wamp::json)
            .withAuthenticator(std::make_shared<TicketAuthenticator>());
    }

    static wamp::ServerConfig udsConfig()
    {
        return wamp::ServerConfig("uds", wamp::UdsPath{"./udstest"},
                                  wamp::msgpack);
    }

    void run()
    {
        try
        {
            router_.openRealm(wamp::RealmConfig{"cppwamp.test"});
            router_.openRealm(wamp::RealmConfig{"cppwamp.authtest"});
            router_.openServer(tcpConfig());
            router_.openServer(tcpTicketConfig());
            router_.openServer(udsConfig());
            ioctx_.run();
        }
        catch (const std::exception& e)
        {
            std::cerr << "Test router exception: " << e.what() << std::endl;
            throw;
        }
        catch (...)
        {
            std::cerr << "Unknown test router exception" << std::endl;
            throw;
        }
    }

    void onAccessLogEntry(wamp::AccessLogEntry a)
    {
        if (accessLogHandler_)
            accessLogHandler_(std::move(a));
    }

    wamp::IoContext ioctx_;
    Logger logger_;
    wamp::Router router_;
    std::thread thread_;
    AccessLogHandler accessLogHandler_;
};

//------------------------------------------------------------------------------
Router& Router::instance()
{
    static Router theRouter;
    return theRouter;
}

//------------------------------------------------------------------------------
void Router::start()
{
    std::cout << "Launching router..." << std::endl;
    impl_ = std::make_shared<Impl>();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "Router started" << std::endl;
}

//------------------------------------------------------------------------------
void Router::stop()
{
    std::cout << "Shutting down router..." << std::endl;
    impl_.reset();
    std::cout << "Router stopped" << std::endl;
}

//------------------------------------------------------------------------------
Router::AccessLogGuard Router::attachToAccessLog(AccessLogHandler handler)
{
    return impl_->attachToAccessLog(std::move(handler));
}

//------------------------------------------------------------------------------
void Router::detachFromAccessLog() {impl_->detachFromAccessLog();}

//------------------------------------------------------------------------------
Router::Router() {}

} // namespace test
