/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP router.
//******************************************************************************

#include <cppwamp/directsession.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/tcp.hpp>
#include <cppwamp/router.hpp>
#include <cppwamp/spawn.hpp>
#include <cppwamp/utils/consolelogger.hpp>

//------------------------------------------------------------------------------
class TicketAuthenticator : public wamp::Authenticator
{
public:
    TicketAuthenticator(wamp::utils::ColorConsoleLogger& logger)
        : logger_(logger)
    {}

protected:
    void authenticate(wamp::AuthExchange::Ptr ex) override
    {
        logger_({
                wamp::LogLevel::debug,
                "main onAuthenticate: authid=" +
                    ex->realm().authId().value_or("anonymous")});
        if (ex->challengeCount() == 0)
        {
            logger_({wamp::LogLevel::debug, "ex->challenge()"});
            ex->challenge(wamp::Challenge("ticket").withChallenge("quest"),
                          std::string("memento"));
        }
        else if (ex->challengeCount() == 1)
        {
            logger_({wamp::LogLevel::debug,
                    "note = " +
                        wamp::any_cast<const std::string&>(ex->note())});
            if (ex->authentication().signature() == "grail")
                ex->welcome({"admin", "authrole", "ticket", "static"});
            else
                ex->reject();
        }
        else
        {
            logger_({wamp::LogLevel::debug, "ex->reject()"});
            ex->reject();
        }
    }

private:
    wamp::utils::ColorConsoleLogger& logger_;
};


//------------------------------------------------------------------------------
int main()
{
    wamp::utils::ColorConsoleLogger logger{"router"};

    auto authenticator = std::make_shared<TicketAuthenticator>(logger);

    auto config = wamp::RouterConfig()
        .withLogHandler(logger)
        .withLogLevel(wamp::LogLevel::trace)
        .withAccessLogHandler(wamp::AccessLogFilter(logger));

    auto realmConfig = wamp::RealmConfig("cppwamp.examples");

    auto serverConfig =
        wamp::ServerConfig("tcp12345", wamp::TcpEndpoint{12345}, wamp::json)
            .withAuthenticator(authenticator);

    auto echo = [](wamp::Invocation inv) -> wamp::Outcome
    {
        return wamp::Result{inv.args().at(0).as<wamp::String>()};
    };

    logger({wamp::LogLevel::info, "CppWAMP Example Router launched"});
    wamp::IoContext ioctx;

    wamp::Router router{ioctx, config};
    router.openRealm(realmConfig);
    router.openServer(serverConfig);

    wamp::DirectSession session{ioctx};
    wamp::spawn(ioctx, [&](wamp::YieldContext yield)
    {
        session.connect(router);
        session.join(wamp::Realm{"cppwamp.examples"}, yield).value();
        session.enroll(wamp::Procedure("local_echo"), echo, yield).value();
    });

    ioctx.run();
    logger({wamp::LogLevel::info, "CppWAMP Example Router exit"});
    return 0;
}
