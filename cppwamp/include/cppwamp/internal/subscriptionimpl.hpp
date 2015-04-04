/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_SUBSCRIPTIONIMPL_HPP
#define CPPWAMP_INTERNAL_SUBSCRIPTIONIMPL_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include "../args.hpp"
#include "subscriber.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class SubscriptionBase
{
public:
    using Id                 = uint64_t;
    using PublicationId      = uint64_t;
    using SubscriberPtr      = Subscriber::WeakPtr;
    using Ptr                = std::shared_ptr<SubscriptionBase>;
    using WeakPtr            = std::weak_ptr<SubscriptionBase>;
    using UnsubscribeHandler = Subscriber::UnsubscribeHandler;

    virtual ~SubscriptionBase() {unsubscribe();}

    const std::string& topic() const {return topic_;}

    Id id() const {return id_;}

    void setId(Id id) {id_ = id;}

    virtual void invoke(PublicationId pubId, const Args& args) = 0;

    void unsubscribe()
    {
        if (!subscriber_.expired())
            subscriber_.lock()->unsubscribe(this);
    }

    void unsubscribe(UnsubscribeHandler handler)
    {
        if (!subscriber_.expired())
            subscriber_.lock()->unsubscribe(this, std::move(handler));
    }

protected:
    SubscriptionBase(SubscriberPtr subscriber, std::string&& procedure)
        : subscriber_(subscriber), topic_(std::move(procedure)), id_(0)
    {}

private:
    SubscriberPtr subscriber_;
    std::string topic_;
    Id id_;
};

//------------------------------------------------------------------------------
template <typename... TParams>
class SubscriptionImpl : public SubscriptionBase
{
public:
    using Slot = std::function<void (PublicationId, TParams...)>;

    static Ptr create(SubscriberPtr subscriber, std::string topic, Slot slot)
    {
        using std::move;
        return Ptr(new SubscriptionImpl(subscriber, move(topic), move(slot)));
    }

    virtual void invoke(PublicationId pubId, const Args& args) override
    {
        wamp::Unmarshall<TParams...>::apply(slot_, args.list, pubId);
    }

private:
    SubscriptionImpl(SubscriberPtr subscriber, std::string&& topic, Slot&& slot)
        : SubscriptionBase(subscriber, std::move(topic)), slot_(std::move(slot))
    {}

    Slot slot_;
};

//------------------------------------------------------------------------------
template <>
class SubscriptionImpl<Args> : public SubscriptionBase
{
public:
    using Slot = std::function<void (PublicationId, Args)>;

    static Ptr create(SubscriberPtr subscriber, std::string topic, Slot slot)
    {
        using std::move;
        return Ptr(new SubscriptionImpl(subscriber, move(topic), move(slot)));
    }

    virtual void invoke(PublicationId pubId, const Args& args) override
    {
        slot_(pubId, args);
    }

private:
    SubscriptionImpl(SubscriberPtr subscriber, std::string&& topic, Slot&& slot)
        : SubscriptionBase(subscriber, std::move(topic)), slot_(std::move(slot))
    {}

    Slot slot_;
};

//------------------------------------------------------------------------------
template <>
class SubscriptionImpl<void> : public SubscriptionBase
{
public:
    using Slot = std::function<void (PublicationId)>;

    static Ptr create(SubscriberPtr subscriber, std::string topic, Slot slot)
    {
        using std::move;
        return Ptr(new SubscriptionImpl(subscriber, move(topic), move(slot)));
    }

    virtual void invoke(PublicationId pubId, const Args&) override
    {
        slot_(pubId);
    }

private:
    SubscriptionImpl(SubscriberPtr subscriber, std::string&& topic, Slot&& slot)
        : SubscriptionBase(subscriber, std::move(topic)), slot_(std::move(slot))
    {}

    Slot slot_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_SUBSCRIPTIONIMPL_HPP
