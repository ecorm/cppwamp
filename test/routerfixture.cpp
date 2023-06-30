/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "routerfixture.hpp"
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
std::shared_ptr<RouterFixture> RouterFixture::theRouter_;
bool RouterFixture::enabled_ = false;

//------------------------------------------------------------------------------
struct RouterFixture::Impl
{
    using AccessLogHandler = std::function<void (wamp::AccessLogEntry)>;

    Impl()
        : logger_(ioctx_, wamp::utils::ColorConsoleLogger{true}),
          router_(ioctx_, routerConfig(*this)),
          thread_([this](){run();})
    {}

    AccessLogGuard attachToAccessLog(AccessLogHandler handler)
    {
        accessLogHandler_ = std::move(handler);
        return AccessLogGuard{};
    }

    void detachFromAccessLog() {accessLogHandler_ = nullptr;}

    wamp::Router& router() {return router_;}

    void stop()
    {
        router_.close();
        thread_.join();
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
            router_.openRealm(wamp::RealmConfig{"cppwamp.test"}
                                  .withMetaApiEnabled());
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
RouterFixture& RouterFixture::instance()
{
    enabled_ = true;
    if (!theRouter_)
        theRouter_.reset(new RouterFixture);
    return *theRouter_;
}

//------------------------------------------------------------------------------
void RouterFixture::cleanUp()
{
    if (theRouter_)
        theRouter_.reset();
}

//------------------------------------------------------------------------------
bool RouterFixture::enabled() {return enabled_;}

//------------------------------------------------------------------------------
void RouterFixture::start()
{
    std::cout << "Launching router..." << std::endl;
    impl_ = std::make_shared<Impl>();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "Router started" << std::endl;
}

//------------------------------------------------------------------------------
void RouterFixture::stop()
{
    std::cout << "Shutting down router..." << std::endl;
    impl_->router().setLogLevel(wamp::LogLevel::error);
    impl_->stop();
    std::cout << "Router stopped" << std::endl;
}

//------------------------------------------------------------------------------
RouterFixture::AccessLogGuard RouterFixture::attachToAccessLog(AccessLogHandler handler)
{
    return impl_->attachToAccessLog(std::move(handler));
}

//------------------------------------------------------------------------------
void RouterFixture::detachFromAccessLog() {impl_->detachFromAccessLog();}

//------------------------------------------------------------------------------
wamp::Router& RouterFixture::router() {return impl_->router();}

//------------------------------------------------------------------------------
RouterFixture::RouterFixture() {}

} // namespace test
