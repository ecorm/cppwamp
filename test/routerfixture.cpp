/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "routerfixture.hpp"
#include <fstream>
#include <iostream>
#include <thread>
#include <cppwamp/anyhandler.hpp>
#include <cppwamp/router.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/codecs/msgpack.hpp>
#include <cppwamp/transports/tcp.hpp>
#include <cppwamp/transports/uds.hpp>
#include <cppwamp/utils/consolelogger.hpp>

namespace test
{

//------------------------------------------------------------------------------
class TicketAuthenticator : public wamp::Authenticator
{
public:
    TicketAuthenticator() = default;

protected:
    void onAuthenticate(wamp::AuthExchange::Ptr ex) override
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
        : logger_(wamp::utils::ConsoleLogger{loggerOptions()}),
          router_(ioctx_, routerOptions(*this)),
          thread_([this](){run();})
    {
        logger_.set_executor(ioctx_.get_executor());
    }

    AccessLogSnoopGuard snoopAccessLog(wamp::AnyCompletionExecutor exec,
                                       AccessLogHandler handler)
    {
        accessLogHandler_ =
            boost::asio::bind_executor(exec, std::move(handler));
        return AccessLogSnoopGuard{};
    }

    void unsnoopAccessLog() {accessLogHandler_ = nullptr;}

    wamp::Router& router() {return router_;}

    void stop()
    {
        router_.close();
        thread_.join();
    }

private:
    using AccessLogHandlerWithExecutor =
        wamp::AnyReusableHandler<void (wamp::AccessLogEntry)>;

    static wamp::utils::ConsoleLoggerOptions loggerOptions()
    {
        return wamp::utils::ConsoleLoggerOptions{}.withColor();
    }

    static wamp::RouterOptions routerOptions(Impl& self)
    {
        return wamp::RouterOptions()
            .withLogHandler(self.logger_)
            .withLogLevel(wamp::LogLevel::info)
            .withAccessLogHandler(
                [&self](wamp::AccessLogEntry a)
                {
                    self.onAccessLogEntry(std::move(a));
                });
    }

    static wamp::ServerOptions tcpOptions()
    {
        return wamp::ServerOptions("tcp12345", wamp::TcpEndpoint{12345},
                                   wamp::json);
    }

    static wamp::ServerOptions tcpTicketOptions()
    {
        return wamp::ServerOptions("tcp23456", wamp::TcpEndpoint{23456},
                                   wamp::json)
            .withAuthenticator(std::make_shared<TicketAuthenticator>())
            .withChallengeTimeout(std::chrono::milliseconds(50));
    }

    static wamp::ServerOptions udsOptions()
    {
        return wamp::ServerOptions("uds", wamp::UdsPath{"./udstest"},
                                   wamp::msgpack);
    }

    void run()
    {
        try
        {
            router_.openRealm(wamp::RealmOptions{"cppwamp.test"}
                                  .withMetaApiEnabled());
            router_.openRealm(wamp::RealmOptions{"cppwamp.authtest"});
            router_.openServer(tcpOptions());
            router_.openServer(tcpTicketOptions());
            router_.openServer(udsOptions());
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
            wamp::postAny(ioctx_, accessLogHandler_, std::move(a));
    }

    wamp::IoContext ioctx_;
    wamp::RouterOptions::LogHandler logger_;
    wamp::Router router_;
    std::thread thread_;
    AccessLogHandlerWithExecutor accessLogHandler_;
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
RouterFixture::AccessLogSnoopGuard
RouterFixture::snoopAccessLog(wamp::AnyCompletionExecutor exec,
                              AccessLogHandler handler)
{
    return impl_->snoopAccessLog(std::move(exec), std::move(handler));
}

//------------------------------------------------------------------------------
wamp::Router& RouterFixture::router() {return impl_->router();}

//------------------------------------------------------------------------------
RouterFixture::RouterFixture() {}

//------------------------------------------------------------------------------
void RouterFixture::unsnoopAccessLog() {impl_->unsnoopAccessLog();}

} // namespace test
