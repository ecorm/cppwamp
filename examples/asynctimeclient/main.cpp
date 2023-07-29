/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service consumer app using callback handler functions.
//******************************************************************************

#include <ctime>
#include <iostream>
#include <cppwamp/json.hpp>
#include <cppwamp/transports/tcp.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/unpacker.hpp>
#include <cppwamp/variant.hpp>

const std::string realm = "cppwamp.examples";
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
class TimeClient : public std::enable_shared_from_this<TimeClient>
{
public:
    static std::shared_ptr<TimeClient> create(wamp::AnyIoExecutor exec)
    {
        return std::shared_ptr<TimeClient>(new TimeClient(std::move(exec)));
    }

    void start(wamp::ConnectionWish where)
    {
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
    TimeClient(wamp::AnyIoExecutor exec)
        : session_(std::move(exec))
    {}

    static void onTimeTick(std::tm time)
    {
        std::cout << "The current time is: " << std::asctime(&time) << "\n";
    }

    void join()
    {
        auto self = shared_from_this();
        session_.join(
            wamp::Petition(realm),
            [this, self](wamp::ErrorOr<wamp::Welcome> info)
            {
                info.value(); // Throws if join failed
                getTime();
            });
    }

    void getTime()
    {
        auto self = shared_from_this();
        session_.call(
            wamp::Rpc("get_time"),
            [this, self](wamp::ErrorOr<wamp::Result> result)
            {
                // result.value() throws if the call failed
                auto time = result.value()[0].to<std::tm>();
                std::cout << "The current time is: " << std::asctime(&time) << "\n";
                subscribe();
            });
    }

    void subscribe()
    {
        session_.subscribe(
            wamp::Topic("time_tick"),
            wamp::simpleEvent<std::tm>(&TimeClient::onTimeTick),
            [](wamp::ErrorOr<wamp::Subscription> sub)
            {
                sub.value(); // Throws if subscribe failed
            });
    }

    wamp::Session session_;
};


//------------------------------------------------------------------------------
int main()
{
    wamp::IoContext ioctx;
    auto client = TimeClient::create(ioctx.get_executor());
    client->start(wamp::TcpHost(address, port).withFormat(wamp::json));
    ioctx.run();
    return 0;
}
