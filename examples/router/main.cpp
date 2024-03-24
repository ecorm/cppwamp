/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// WAMP router executable for running examples.
//******************************************************************************

#include <map>
#include <utility>
#include <cppwamp/authenticators/anonymousauthenticator.hpp>
#include <cppwamp/router.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcpserver.hpp>
#include "../common/argsparser.hpp"
#include "../common/examplerouter.hpp"

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
// Usage: cppwamp-example-router [anonymous_port [ticket_port [realm]]] | help
//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    try
    {
        ArgsParser args{{{"anonymous_port", "12345"},
                         {"ticket_port",    "23456"},
                         {"realm",          "cppwamp.examples"}}};

        uint_least16_t anonymousPort = 0;
        uint_least16_t ticketPort = 0;
        std::string realm;
        if (!args.parse(argc, argv, anonymousPort, ticketPort, realm))
            return 0;

        auto ticketAuth = std::make_shared<TicketAuthenticator>();
        ticketAuth->upsertUser({"alice", "password123", "guest"});

        auto loggerOptions =
            wamp::utils::ConsoleLoggerOptions{}.withOriginLabel("router")
                                               .withColor();
        wamp::utils::ConsoleLogger logger{std::move(loggerOptions)};

        auto anonymousServer =
            wamp::ServerOptions("tcp" + std::to_string(anonymousPort),
                                wamp::TcpEndpoint{anonymousPort},
                                wamp::jsonWithMaxDepth(10))
                .withAuthenticator(wamp::AnonymousAuthenticator::create());

        auto ticketServer =
            wamp::ServerOptions("tcp" + std::to_string(ticketPort),
                                wamp::TcpEndpoint{ticketPort},
                                wamp::jsonWithMaxDepth(10))
                .withAuthenticator(ticketAuth)
                .withChallengeTimeout(std::chrono::milliseconds(1000));

        wamp::IoContext ioctx;

        wamp::Router router = initRouter(
            ioctx,
            {realm},
            {std::move(anonymousServer), std::move(ticketServer)},
            logger);

        runRouter(ioctx, router, logger);
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
