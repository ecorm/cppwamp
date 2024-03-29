/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service provider app using stackless coroutines.
//******************************************************************************

#include <chrono>
#include <ctime>
#include <iostream>
#include <boost/asio/steady_timer.hpp>
#include <boost/variant2.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/tcp.hpp>
#include <cppwamp/unpacker.hpp>
#include <cppwamp/variant.hpp>
#include <boost/asio/yield.hpp>

const std::string realm = "cppwamp.demo.time";
const std::string address = "localhost";
const short port = 54321u;

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
// This variant type is necessary for TimeService::operator() to resume Session
// operations emitting different result types. This makes Boost stackless
// coroutines awkward to use with CppWAMP, but this example demonstrates
// it is still possible.
//------------------------------------------------------------------------------
using Aftermath = boost::variant2::variant<
    boost::variant2::monostate,
    wamp::ErrorOr<size_t>,
    wamp::ErrorOr<wamp::SessionInfo>,
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
    explicit TimeService(wamp::AnyIoExecutor exec, wamp::ConnectionWish where)
        : session_(std::make_shared<wamp::Session>(exec)),
          timer_(std::make_shared<Timer>(std::move(exec))),
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
            yield session_->join(wamp::Realm(realm), *this);
            std::cout << "Joined, SessionId="
                      << boost::variant2::get<2>(aftermath).value().id()
                      << std::endl;
            yield session_->enroll(wamp::Procedure("get_time"),
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
                std::cout << "Tick: " << std::asctime(local) << std::endl;
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
    wamp::ConnectionWish where_;
    std::chrono::steady_clock::time_point deadline_;
};

//------------------------------------------------------------------------------
int main()
{
    wamp::IoContext ioctx;
    auto tcp = wamp::TcpHost(address, port).withFormat(wamp::json);
    TimeService service(ioctx.get_executor(), std::move(tcp));
    service();
    ioctx.run();
    return 0;
}
