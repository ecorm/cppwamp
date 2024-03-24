/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022, 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_EXAMPLES_CALLBACKTIMECLIENT_HPP
#define CPPWAMP_EXAMPLES_CALLBACKTIMECLIENT_HPP

#include <iostream>
#include <cppwamp/session.hpp>
#include <cppwamp/unpacker.hpp>
#include "tmconversion.hpp"

//------------------------------------------------------------------------------
class TimeClient : public std::enable_shared_from_this<TimeClient>
{
public:
    static std::shared_ptr<TimeClient> create(wamp::AnyIoExecutor exec)
    {
        return std::shared_ptr<TimeClient>(new TimeClient(std::move(exec)));
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
    TimeClient(wamp::AnyIoExecutor exec)
        : session_(std::move(exec))
    {}

    static void onTimeTick(std::tm time)
    {
        std::cout << "The current time is: " << std::asctime(&time);
    }

    void join()
    {
        auto self = shared_from_this();
        session_.join(
            realm_,
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
                std::cout << "The current time is: " << std::asctime(&time);
                subscribe();
            });
    }

    void subscribe()
    {
        session_.subscribe(
            "time_tick",
            wamp::simpleEvent<std::tm>(&TimeClient::onTimeTick),
            [](wamp::ErrorOr<wamp::Subscription> sub)
            {
                sub.value(); // Throws if subscribe failed
            });
    }

    wamp::Session session_;
    std::string realm_;
};

#endif // CPPWAMP_EXAMPLES_CALLBACKTIMECLIENT_HPP
