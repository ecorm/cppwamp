/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022, 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service provider app using callback handler functions.
//******************************************************************************

#include <chrono>
#include <iostream>
#include <boost/asio/steady_timer.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/unpacker.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tcpclient.hpp>
#include "../common/argsparser.hpp"
#include "../common/tmconversion.hpp"

//------------------------------------------------------------------------------
class TimeService : public std::enable_shared_from_this<TimeService>
{
public:
    static std::shared_ptr<TimeService> create(wamp::AnyIoExecutor exec)
    {
        return std::shared_ptr<TimeService>(new TimeService(std::move(exec)));
    }

    void start(std::string realm, wamp::ConnectionWish where)
    {
        realm_ = std::move(realm);

        auto self = shared_from_this();
        session_.connect(
            std::move(where),
            [this, self](wamp::ErrorOr<size_t> index)
            {
                index.value(); // Throws if connect failed
                join();
            });
    }

private:
    explicit TimeService(wamp::AnyIoExecutor exec)
        : session_(exec),
          timer_(std::move(exec))
    {}

    static std::tm getTime()
    {
        auto t = std::time(nullptr);
        return *std::localtime(&t);
    }

    void join()
    {
        auto self = shared_from_this();
        session_.join(
            realm_,
            [this, self](wamp::ErrorOr<wamp::Welcome> info)
            {
                info.value(); // Throws if join failed
                enroll();
            });
    }

    void enroll()
    {
        auto self = shared_from_this();
        session_.enroll(
            "get_time",
            wamp::simpleRpc<std::tm>(&getTime),
            [this, self](wamp::ErrorOr<wamp::Registration> reg)
            {
                reg.value(); // Throws if enroll failed
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
        session_.publish(wamp::Pub("time_tick").withArgs(*local));
        std::cout << "Tick: " << std::asctime(local) << "\n";
    }

    wamp::Session session_;
    boost::asio::steady_timer timer_;
    std::string realm_;
    std::chrono::steady_clock::time_point deadline_;
};

//------------------------------------------------------------------------------
// Usage: cppwamp-example-asynctimeservice [port [host [realm]]] | help
// Use with cppwamp-example-router and cppwamp-example-asynctimeclient.
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
    auto service = TimeService::create(ioctx.get_executor());
    service->start(realm, wamp::TcpHost(host, port).withFormat(wamp::json));
    ioctx.run();

    return 0;
}
