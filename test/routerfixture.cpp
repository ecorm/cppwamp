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
#include <cppwamp/codecs/cbor.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/codecs/msgpack.hpp>
#include <cppwamp/transports/tcp.hpp>
#include <cppwamp/transports/uds.hpp>
#include <cppwamp/utils/consolelogger.hpp>
#include <cppwamp/utils/filelogger.hpp>

#if defined(CPPWAMP_TEST_HAS_WEB)
#include <cppwamp/transports/websocket.hpp>
#endif

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
        : logHandler_(wamp::utils::ConsoleLogger{loggerOptions()}),
          accessLogHandler_(wamp::utils::FileLogger{accessLogFilename(),
                                                    fileLoggerOptions()}),
          router_(ioctx_, routerOptions(*this)),
          thread_([this](){run();})
    {
        logHandler_.set_executor(ioctx_.get_executor());
    }

    LogLevelGuard supressLogLevel(wamp::LogLevel level)
    {
        LogLevelGuard guard{logLevel_};
        logLevel_ = level;
        return guard;
    }

    LogSnoopGuard snoopLog(wamp::AnyCompletionExecutor exec, LogHandler handler)
    {
        logSnooper_ =
            boost::asio::bind_executor(exec, std::move(handler));
        return LogSnoopGuard{};
    }

    AccessLogSnoopGuard snoopAccessLog(wamp::AnyCompletionExecutor exec,
                                       AccessLogHandler handler)
    {
        accessLogSnooper_ =
            boost::asio::bind_executor(exec, std::move(handler));
        return AccessLogSnoopGuard{};
    }

    void restoreLogLevel(wamp::LogLevel level) {logLevel_ = level;}

    void unsnoopLog() {logSnooper_ = nullptr;}

    void unsnoopAccessLog() {accessLogSnooper_ = nullptr;}

    wamp::Router& router() {return router_;}

    void stop()
    {
        router_.close();
        thread_.join();
    }

private:
    static wamp::utils::ConsoleLoggerOptions loggerOptions()
    {
        return wamp::utils::ConsoleLoggerOptions{}.withColor();
    }

    static std::string accessLogFilename() {return "accesslog.txt";}

    static wamp::utils::FileLoggerOptions fileLoggerOptions()
    {
        return wamp::utils::FileLoggerOptions{}.withTruncate();
    }

    static wamp::RouterOptions routerOptions(Impl& self)
    {
        return wamp::RouterOptions()
            .withLogHandler(
                [&self](wamp::LogEntry e) {self.onLogEntry(std::move(e));})
            .withLogLevel(wamp::LogLevel::info)
            .withAccessLogHandler(
                [&self](wamp::AccessLogEntry e)
                {
                    self.onAccessLogEntry(std::move(e));
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
        return wamp::ServerOptions("uds", wamp::UdsEndpoint{"./udstest"},
                                   wamp::msgpack);
    }

#if defined(CPPWAMP_TEST_HAS_WEB)
    static wamp::ServerOptions websocketOptions()
    {
        return wamp::ServerOptions("websocket", wamp::WebsocketEndpoint{34567},
                                   wamp::cbor);
    }
#endif

    void run()
    {
        try
        {
            remainingInfoLogEntries_ = 5;
            router_.openRealm(wamp::RealmOptions{"cppwamp.test"}
                                  .withMetaApiEnabled());
            router_.openRealm(wamp::RealmOptions{"cppwamp.authtest"});
            router_.openServer(tcpOptions());
            router_.openServer(tcpTicketOptions());
            router_.openServer(udsOptions());
#if defined(CPPWAMP_TEST_HAS_WEB)
            ++remainingInfoLogEntries_;
            router_.openServer(websocketOptions());
#endif
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

    void onLogEntry(wamp::LogEntry e)
    {
        if (remainingInfoLogEntries_ > 0)
        {
            --remainingInfoLogEntries_;
            if (remainingInfoLogEntries_ == 0)
                logLevel_ = wamp::LogLevel::error;
        }

        if (logSnooper_)
            wamp::postAny(ioctx_, logSnooper_, e);
        if (e.severity() >= logLevel_)
            wamp::postAny(ioctx_, logHandler_, std::move(e));
    }

    void onAccessLogEntry(wamp::AccessLogEntry a)
    {
        if (accessLogSnooper_)
            wamp::postAny(ioctx_, accessLogSnooper_, a);
        wamp::postAny(ioctx_, accessLogHandler_, std::move(a));
    }

    wamp::IoContext ioctx_;
    wamp::LogLevel logLevel_ = wamp::LogLevel::info;
    wamp::RouterOptions::LogHandler logHandler_;
    wamp::RouterOptions::LogHandler logSnooper_;
    wamp::RouterOptions::AccessLogHandler accessLogHandler_;
    wamp::RouterOptions::AccessLogHandler accessLogSnooper_;
    wamp::Router router_;
    std::thread thread_;
    unsigned remainingInfoLogEntries_ = 0;
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
    impl_->stop();
    std::cout << "Router stopped" << std::endl;
}

//------------------------------------------------------------------------------
RouterFixture::LogLevelGuard
RouterFixture::supressLogLevel(wamp::LogLevel level)
{
    return impl_->supressLogLevel(level);
}

//------------------------------------------------------------------------------
RouterFixture::LogSnoopGuard
RouterFixture::snoopLog(wamp::AnyCompletionExecutor exec, LogHandler handler)
{
    return impl_->snoopLog(std::move(exec), std::move(handler));
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
void RouterFixture::restoreLogLevel(wamp::LogLevel level)
{
    impl_->restoreLogLevel(level);
}

//------------------------------------------------------------------------------
void RouterFixture::unsnoopLog() {impl_->unsnoopLog();}

//------------------------------------------------------------------------------
void RouterFixture::unsnoopAccessLog() {impl_->unsnoopAccessLog();}

} // namespace test
