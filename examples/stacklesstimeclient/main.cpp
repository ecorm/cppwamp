/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022, 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service consumer app using stackless coroutines.
//******************************************************************************

#include <iostream>
#include <boost/variant2.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/unpacker.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcpclient.hpp>
#include <boost/asio/yield.hpp>
#include "../common/argsparser.hpp"
#include "../common/tmconversion.hpp"

//------------------------------------------------------------------------------
void onTimeTick(std::tm time)
{
    std::cout << "The current time is: " << std::asctime(&time) << std::endl;
}

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
    wamp::ErrorOr<wamp::Result>,
    wamp::ErrorOr<wamp::Subscription>>;

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
};

//------------------------------------------------------------------------------
class TimeClient : boost::asio::coroutine
{
public:
    explicit TimeClient(wamp::AnyIoExecutor exec, std::string realm,
                        wamp::ConnectionWish where)
        : session_(std::make_shared<wamp::Session>(std::move(exec))),
          realm_(std::move(realm)),
          where_(std::move(where))
    {}

    void operator()(Aftermath aftermath = {})
    {
        boost::variant2::visit(AftermathChecker{}, aftermath);
        std::tm time;

        reenter (this)
        {
            yield session_->connect(where_, *this);
            std::cout << "Connected via "
                      << boost::variant2::get<1>(aftermath).value() << std::endl;
            yield session_->join(realm_, *this);
            std::cout << "Joined, SessionId="
                      << boost::variant2::get<2>(aftermath).value().sessionId()
                      << std::endl;

            yield session_->call(wamp::Rpc("get_time"), *this);
            time = boost::variant2::get<3>(aftermath).value()[0].to<std::tm>();
            std::cout << "The current time is: " << std::asctime(&time)
                      << std::endl;

            yield session_->subscribe("time_tick",
                                      wamp::simpleEvent<std::tm>(&onTimeTick),
                                      *this);
        }
    }

private:
    // The session object must be stored as a shared pointer due to
    // TimeClient getting copied around.
    std::shared_ptr<wamp::Session> session_;
    std::string realm_;
    wamp::ConnectionWish where_;
};

//------------------------------------------------------------------------------
// Usage: cppwamp-example-stacklesstimeclient [port [host [realm]]] | help
// Use with cppwamp-example-router and cppwamp-example-stacklesstimeservice.
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
    TimeClient client(ioctx.get_executor(), std::move(realm), std::move(tcp));
    client();
    ioctx.run();
    return 0;
}
