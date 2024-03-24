/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_EXAMPLES_DIRECTTIMESERVICE_HPP
#define CPPWAMP_EXAMPLES_DIRECTTIMESERVICE_HPP

#include <cppwamp/directsession.hpp>
#include <cppwamp/router.hpp>
#include <cppwamp/unpacker.hpp>
#include "../common/tmconversion.hpp"

//------------------------------------------------------------------------------
class DirectTimeService : public wamp::RealmObserver
{
public:
    static std::shared_ptr<DirectTimeService>
    create(wamp::AnyIoExecutor exec, wamp::Realm realm)
    {
        return std::shared_ptr<DirectTimeService>(
            new DirectTimeService(std::move(exec), std::move(realm)));
    }

    void start(wamp::DirectRouterLink router)
    {
        realm_.observe(shared_from_this());

        auto self = shared_from_this();
        session_.connect(std::move(router));
        session_.join(
            realm_.uri(),
            [this, self](wamp::ErrorOr<wamp::Welcome> info)
            {
                info.value(); // Throws if join failed
                enroll();
            });
    }

private:
    explicit DirectTimeService(wamp::AnyIoExecutor exec, wamp::Realm realm)
        : realm_(std::move(realm)),
          session_(exec),
          timer_(std::move(exec))
    {}

    static std::tm getTime()
    {
        auto t = std::time(nullptr);
        return *std::localtime(&t);
    }

    void onSubscribe(const wamp::SessionInfo&,
                     const wamp::SubscriptionInfo& sub) override
    {
        if (sub.uri == "time_tick")
            subscriptionCount_ = sub.subscriberCount;
    }

    void onUnsubscribe(const wamp::SessionInfo&,
                       const wamp::SubscriptionInfo& sub) override
    {
        if (sub.uri == "time_tick")
            subscriptionCount_ = sub.subscriberCount;
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
        timer_.async_wait(
            [this, self](boost::system::error_code ec)
            {
                if (ec)
                    throw boost::system::system_error(ec);
                if (subscriptionCount_ != 0)
                    publish();
                kickTimer();
            });
    }

    void publish()
    {
        auto t = std::time(nullptr);
        const std::tm* local = std::localtime(&t);
        session_.publish(wamp::Pub("time_tick").withArgs(*local));
    }

    wamp::Realm realm_;
    wamp::DirectSession session_;
    boost::asio::steady_timer timer_;
    std::chrono::steady_clock::time_point deadline_;
    std::size_t subscriptionCount_ = 0;
};

#endif // CPPWAMP_EXAMPLES_DIRECTTIMESERVICE_HPP
