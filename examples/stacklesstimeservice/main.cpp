/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022, 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service provider app using stackless coroutines.
//******************************************************************************

#include <chrono>
#include <iostream>
#include <boost/asio/steady_timer.hpp>
#include <boost/variant2.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcpclient.hpp>
#include <cppwamp/unpacker.hpp>
#include <boost/asio/yield.hpp>
#include "../common/argsparser.hpp"
#include "../common/tmconversion.hpp"

//------------------------------------------------------------------------------
// This variant type is necessary for TimeService::operator() to resume Session
// operations emitting different result types. This makes Boost stackless
// coroutines awkward to use with CppWAMP, but this example demonstrates that
// it is still possible.
//------------------------------------------------------------------------------
using Aftermath = boost::variant2::variant<
    boost::variant2::monostate,
    wamp::ErrorOr<size_t>,
    wamp::ErrorOr<wamp::Welcome>,
    wamp::ErrorOr<wamp::Registration>,
    boost::system::error_code,
    wamp::ErrorOr<wamp::PublicationId>>;

//------------------------------------------------------------------------------
// Visitor that checks the success of any of the result types passed to the
// coroutine.
//------------------------------------------------------------------------------
struct AftermathChecker
{
    void operator()(boost::variant2::monostate) const {}

    template <typename T>
    void operator()(const wamp::ErrorOr<T>& result) const
    {
        result.value();
    }

    void operator()(boost::system::error_code ec) const
    {
        if (ec)
            throw boost::system::system_error(ec);
    }
};

//------------------------------------------------------------------------------
class TimeService : boost::asio::coroutine
{
public:
    explicit TimeService(wamp::AnyIoExecutor exec, std::string realm,
                         wamp::ConnectionWish where)
        : session_(std::make_shared<wamp::Session>(exec)),
          timer_(std::make_shared<Timer>(std::move(exec))),
          realm_(std::move(realm)),
          where_(std::move(where))
    {}

    void operator()(Aftermath aftermath = {})
    {
        boost::variant2::visit(AftermathChecker{}, aftermath);

        std::time_t time;
        const std::tm* local = nullptr;

        reenter (this)
        {
            yield session_->connect(where_, *this);
            std::cout << "Connected via "
                      << boost::variant2::get<1>(aftermath).value() << std::endl;
            yield session_->join(realm_, *this);
            std::cout << "Joined, SessionId="
                      << boost::variant2::get<2>(aftermath).value().sessionId()
                      << std::endl;
            yield session_->enroll("get_time",
                                   wamp::simpleRpc<std::tm>(&getTime),
                                   *this);
            std::cout << "Registered 'get_time', RegistrationId="
                      << boost::variant2::get<3>(aftermath).value().id()
                      << std::endl;

            // The deadline must be a member variable due to TimeService
            // getting copied around between ticks.
            deadline_ = std::chrono::steady_clock::now();
            while (true)
            {
                deadline_ += std::chrono::seconds(1);
                timer_->expires_at(deadline_);
                yield timer_->async_wait(*this);

                time = std::time(nullptr);
                local = std::localtime(&time);
                std::cout << "Tick: " << std::asctime(local);
                yield session_->publish(
                    wamp::Pub("time_tick").withArgs(*local),
                    *this);
            }
        }
    }

private:
    using Timer = boost::asio::steady_timer;

    static std::tm getTime()
    {
        auto t = std::time(nullptr);
        return *std::localtime(&t);
    }

    // The session and timer objects must be stored as shared pointers
    // due to TimeService getting copied around.
    std::shared_ptr<wamp::Session> session_;
    std::shared_ptr<Timer> timer_;
    std::string realm_;
    wamp::ConnectionWish where_;
    std::chrono::steady_clock::time_point deadline_;
};

//------------------------------------------------------------------------------
// Usage: cppwamp-example-stacklesstimeservice [port [host [realm]]] | help
// Use with cppwamp-example-router and cppwamp-example-stacklesstimeclient.
//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    ArgsParser args{{{"port", "12345"},
                     {"host", "localhost"},
                     {"realm", "cppwamp.examples"}}};

    std::string port, host, realm;
    if (!args.parse(argc, argv, port, host, realm))
        return 0;

    wamp::IoContext ioctx;
    auto tcp = wamp::TcpHost(host, port).withFormat(wamp::json);
    TimeService service(ioctx.get_executor(), std::move(realm), std::move(tcp));
    service();
    ioctx.run();
    return 0;
}
