/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP router.
//******************************************************************************

// TODO: Standalone router to run examples

#include <cppwamp/directsession.hpp>
#include <cppwamp/router.hpp>
#include <cppwamp/spawn.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcp.hpp>
#include <cppwamp/utils/consolelogger.hpp>

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
            ex->challenge(wamp::Challenge("ticket").withChallenge("quest"),
                          std::string("memento"));
        }
        else if (ex->challengeCount() == 1)
        {
            if (ex->authentication().signature() == "grail")
                ex->welcome({"admin", "authrole", "ticket", "static"});
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
int main()
{
    auto loggerOptions =
        wamp::utils::ConsoleLoggerOptions{}.withOriginLabel("router")
                                           .withColor();
    wamp::utils::ConsoleLogger logger{std::move(loggerOptions)};

    auto authenticator = std::make_shared<TicketAuthenticator>();

    auto routerOptions = wamp::RouterOptions()
        .withLogHandler(logger)
        .withLogLevel(wamp::LogLevel::debug)
        .withAccessLogHandler(wamp::AccessLogFilter(logger));

    auto realmOptions = wamp::RealmOptions("cppwamp.examples");

    auto serverOptions =
        wamp::ServerOptions("tcp12345", wamp::TcpEndpoint{12345},
                            wamp::jsonWithMaxDepth(10))
            .withAuthenticator(authenticator);

    auto echo = [](wamp::Invocation inv) -> wamp::Outcome
    {
        return wamp::Result{inv.args().at(0).as<wamp::String>()};
    };

    logger({wamp::LogLevel::info, "CppWAMP Example Router launched"});
    wamp::IoContext ioctx;

    wamp::Router router{ioctx, routerOptions};
    router.openRealm(realmOptions);
    router.openServer(serverOptions);

    wamp::DirectSession session{ioctx};
    wamp::spawn(ioctx, [&](wamp::YieldContext yield)
    {
        session.connect(router);
        session.join(wamp::Petition{"cppwamp.examples"}.withAuthId("insider"),
                     yield).value();
        session.enroll(wamp::Procedure("local_echo"), echo, yield).value();
    });

    ioctx.run();
    logger({wamp::LogLevel::info, "CppWAMP Example Router exit"});
    return 0;
}
