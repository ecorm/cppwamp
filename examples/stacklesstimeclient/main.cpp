/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service consumer app using stackless coroutines.
//******************************************************************************

#include <ctime>
#include <iostream>
#include <boost/variant2.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/tcp.hpp>
#include <cppwamp/unpacker.hpp>
#include <cppwamp/variant.hpp>
#include <boost/asio/yield.hpp>

const std::string realm = "cppwamp.demo.time";
const std::string address = "localhost";
const short port = 12345u;

//------------------------------------------------------------------------------
namespace wamp
{
    // Convert a std::tm to/from an object variant.
    template <typename TConverter>
    void convert(TConverter& conv, std::tm& t)
    {
        conv ("sec",   t.tm_sec)
             ("min",   t.tm_min)
             ("hour",  t.tm_hour)
             ("mday",  t.tm_mday)
             ("mon",   t.tm_mon)
             ("year",  t.tm_year)
             ("wday",  t.tm_wday)
             ("yday",  t.tm_yday)
             ("isdst", t.tm_isdst);
    }
}

//------------------------------------------------------------------------------
void onTimeTick(std::tm time)
{
    std::cout << "The current time is: " << std::asctime(&time) << std::endl;
}

//------------------------------------------------------------------------------
// This variant type is necessary for TimeService::operator() to resume Session
// operations emitting different result types. This makes Boost stackless
// coroutines awkward to use with CppWAMP, but this example demonstrates
// it is still possible.
//------------------------------------------------------------------------------
using Aftermath = boost::variant2::variant<
    boost::variant2::monostate,
    wamp::ErrorOr<size_t>,
    wamp::ErrorOr<wamp::SessionInfo>,
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
    explicit TimeClient(wamp::AnyIoExecutor exec, wamp::ConnectionWish where)
        : session_(std::make_shared<wamp::Session>(std::move(exec))),
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
            yield session_->join(wamp::Realm(realm), *this);
            std::cout << "Joined, SessionId="
                      << boost::variant2::get<2>(aftermath).value().id()
                      << std::endl;

            yield session_->call(wamp::Rpc("get_time"), *this);
            time = boost::variant2::get<3>(aftermath).value()[0].to<std::tm>();
            std::cout << "The current time is: " << std::asctime(&time)
                      << std::endl;

            yield session_->subscribe(wamp::Topic("time_tick"),
                                      wamp::simpleEvent<std::tm>(&onTimeTick),
                                      *this);
        }
    }

private:
    // The session object must be stored as a shared pointer due to
    // TimeClient getting copied around.
    std::shared_ptr<wamp::Session> session_;
    wamp::ConnectionWish where_;
};

//------------------------------------------------------------------------------
int main()
{
    wamp::IoContext ioctx;
    auto tcp = wamp::TcpHost(address, port).withFormat(wamp::json);
    TimeClient client(ioctx.get_executor(), std::move(tcp));
    client();
    ioctx.run();
    return 0;
}
