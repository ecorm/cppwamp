/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// WAMP router executable for running examples.
//******************************************************************************

#include <csignal>
#include <map>
#include <utility>
#include <boost/asio/signal_set.hpp>
#include <cppwamp/authenticators/anonymousauthenticator.hpp>
#include <cppwamp/router.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcp.hpp>
#include <cppwamp/utils/consolelogger.hpp>

//------------------------------------------------------------------------------
struct UserRecord
{
    std::string username;
    std::string password; // Example only, don't store unhashed passwords!
    std::string role;
};

//------------------------------------------------------------------------------
class TicketAuthenticator : public wamp::Authenticator
{
public:
    TicketAuthenticator() = default;

    void upsertUser(UserRecord record)
    {
        auto username = record.username;
        users_[std::move(username)] = std::move(record);
    }

protected:
    void onAuthenticate(wamp::AuthExchange::Ptr ex) override
    {
        auto username = ex->hello().authId().value_or("");
        if (ex->challengeCount() == 0)
        {
            if (username.empty() || users_.count(username) == 0)
                return ex->reject();
            ex->challenge(wamp::Challenge("ticket"));
        }
        else if (ex->challengeCount() == 1)
        {
            const auto kv = users_.find(username);
            if (kv == users_.end())
                ex->reject();

            const auto& record = kv->second;
            if (ex->authentication().signature() != record.password)
                ex->reject();

            ex->welcome({username, record.role, "ticket", "static"});
        }
        else
        {
            ex->reject();
        }
    }

private:
    std::map<std::string, UserRecord> users_;
};

//------------------------------------------------------------------------------
int main()
{
    try
    {
        auto ticketAuth = std::make_shared<TicketAuthenticator>();
        ticketAuth->upsertUser({"alice", "password123", "guest"});

        auto loggerOptions =
            wamp::utils::ConsoleLoggerOptions{}.withOriginLabel("router")
                                               .withColor();
        wamp::utils::ConsoleLogger logger{std::move(loggerOptions)};

        auto routerOptions = wamp::RouterOptions()
            .withLogHandler(logger)
            .withLogLevel(wamp::LogLevel::info)
            .withAccessLogHandler(wamp::AccessLogFilter(logger));

        auto realmOptions = wamp::RealmOptions("cppwamp.examples");

        auto anonymousServerOptions =
            wamp::ServerOptions("tcp12345", wamp::TcpEndpoint{12345},
                                wamp::jsonWithMaxDepth(10))
                .withAuthenticator(wamp::AnonymousAuthenticator::create());

        auto ticketServerOptions =
            wamp::ServerOptions("tcp23456", wamp::TcpEndpoint{23456},
                                wamp::jsonWithMaxDepth(10))
                .withAuthenticator(ticketAuth);

        logger({wamp::LogLevel::info, "CppWAMP example router launched"});
        wamp::IoContext ioctx;

        wamp::Router router{ioctx, routerOptions};
        router.openRealm(realmOptions);
        router.openServer(anonymousServerOptions);
        router.openServer(ticketServerOptions);

        boost::asio::signal_set signals{ioctx, SIGINT, SIGTERM};
        signals.async_wait(
            [&router](const boost::system::error_code& ec, int sig)
            {
                if (ec)
                    return;
                const char* sigName = (sig == SIGINT)  ? "SIGINT" :
                                      (sig == SIGTERM) ? "SIGTERM" : "unknown";
                router.log({wamp::LogLevel::info,
                            std::string("Received ") + sigName + " signal"});
                router.close();
            });

        ioctx.run();
        logger({wamp::LogLevel::info, "CppWAMP example router exit"});
    }
    catch (const std::exception& e)
    {
        std::cerr << "Unhandled exception: " << e.what() << ", terminating."
                  << std::endl;
    }
    catch (...)
    {
        std::cerr << "Unhandled exception: <unknown>, terminating."
                  << std::endl;
    }

    return 0;
}
