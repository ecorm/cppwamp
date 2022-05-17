/*------------------------------------------------------------------------------
                   Copyright Butterfly Energy Systems 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <chrono>
#include <ctime>
#include <iostream>
#include <boost/asio/steady_timer.hpp>
#include <cppwamp/json.hpp>
#include <cppwamp/tcp.hpp>
#include <cppwamp/unpacker.hpp>
#include <cppwamp/variant.hpp>
#include <cppwamp/coro/corosession.hpp>

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
class TimeService : public std::enable_shared_from_this<TimeService>
{
public:
    static std::shared_ptr<TimeService> create(wamp::Session::Ptr session)
    {
        return std::shared_ptr<TimeService>(new TimeService(session));
    }

    void start()
    {
        auto self = shared_from_this();
        session_->connect([this, self](wamp::AsyncResult<size_t> index)
        {
            index.get(); // Throws if connect failed
            join();
        });
    }

private:
    explicit TimeService(wamp::Session::Ptr session)
        : session_(session),
          timer_(session->userExecutor())
    {}

    static std::tm getTime()
    {
        auto t = std::time(nullptr);
        return *std::localtime(&t);
    }

    void join()
    {
        auto self = shared_from_this();
        session_->join(
            wamp::Realm(realm),
            [this, self](wamp::AsyncResult<wamp::SessionInfo> info)
            {
                info.get(); // Throws if join failed
                enroll();
            });
    }

    void enroll()
    {
        auto self = shared_from_this();
        session_->enroll(
            wamp::Procedure("get_time"),
            wamp::basicRpc<std::tm>(&getTime),
            [this, self](wamp::AsyncResult<wamp::Registration> reg)
            {
                reg.get(); // Throws if enroll failed
                deadline_ = std::chrono::steady_clock::now();
                kickTimer();
            });
    }

    void kickTimer()
    {
        deadline_ += std::chrono::seconds(1);
        timer_.expires_at(deadline_);

        auto self = shared_from_this();
        timer_.async_wait([this, self](boost::system::error_code ec)
        {
            if (ec)
                throw boost::system::system_error(ec);
            publish();
            kickTimer();
        });
    }

    void publish()
    {
        auto t = std::time(nullptr);
        const std::tm* local = std::localtime(&t);
        session_->publish(wamp::Pub("time_tick").withArgs(*local));
        std::cout << "Tick: " << std::asctime(local) << "\n";
    }

    wamp::Session::Ptr session_;
    boost::asio::steady_timer timer_;
    std::chrono::steady_clock::time_point deadline_;
};

//------------------------------------------------------------------------------
int main()
{
    using namespace wamp;
    AsioContext ioctx;
    auto tcp = connector<Json>(ioctx, TcpHost(address, port));
    auto session = Session::create(ioctx, tcp);

    auto service = TimeService::create(session);
    service->start();
    ioctx.run();

    return 0;
}
