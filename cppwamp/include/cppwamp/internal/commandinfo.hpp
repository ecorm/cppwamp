/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CONTROLINFO_HPP
#define CPPWAMP_INTERNAL_CONTROLINFO_HPP

#include "message.hpp"
#include "passkey.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class Subscribed : public Command<MessageKind::subscribed>
{
public:
    Subscribed(RequestId rid, SubscriptionId sid) : Base(rid, sid) {}

    Subscribed(Message&& msg) : Base(std::move(msg)) {}

    SubscriptionId subscriptionId() const
    {
        return message().to<SubscriptionId>(subscriptionIdPos_);
    }

private:
    using Base = Command<MessageKind::subscribed>;

    static constexpr unsigned subscriptionIdPos_ = 2;
};

//------------------------------------------------------------------------------
class Unsubscribe : public Command<MessageKind::unsubscribe>
{
public:
    Unsubscribe(RequestId rid, SubscriptionId sid) : Base(rid, sid) {}

    Unsubscribe(Message&& msg) : Base(std::move(msg)) {}

    SubscriptionId subscriptionId() const
    {
        return message().to<SubscriptionId>(subscriptionIdPos_);
    }

private:
    using Base = Command<MessageKind::unsubscribe>;

    static constexpr unsigned subscriptionIdPos_ = 2;
};

//------------------------------------------------------------------------------
class Unsubscribed : public Command<MessageKind::unsubscribed>
{
public:
    Unsubscribed(RequestId rid) : Base(rid) {}

    Unsubscribed(Message&& msg) : Base(std::move(msg)) {}

private:
    using Base = Command<MessageKind::unsubscribed>;
};

//------------------------------------------------------------------------------
class Published : public Command<MessageKind::published>
{
public:
    Published(RequestId rid, PublicationId pid) : Base(rid, pid) {}

    Published(Message&& msg) : Base(std::move(msg)) {}

    PublicationId publicationId() const
    {
        return message().to<PublicationId>(publicationIdPos_);
    }

private:
    using Base = Command<MessageKind::published>;

    static constexpr unsigned publicationIdPos_ = 2;
};

//------------------------------------------------------------------------------
class Registered : public Command<MessageKind::registered>
{
public:
    Registered(RequestId rid, RegistrationId sid) : Base(rid, sid) {}

    Registered(Message&& msg) : Base(std::move(msg)) {}

    RegistrationId registrationId() const
    {
        return message().to<RegistrationId>(registrationIdPos_);
    }

private:
    using Base = Command<MessageKind::registered>;

    static constexpr unsigned registrationIdPos_ = 2;
};

//------------------------------------------------------------------------------
class Unregister : public Command<MessageKind::unregister>
{
public:
    Unregister(RegistrationId rid) : Base(0, rid) {}

    Unregister(Message&& msg) : Base(std::move(msg)) {}

    RegistrationId registrationId() const
    {
        return message().to<RegistrationId>(registrationIdPos_);
    }

private:
    using Base = Command<MessageKind::unregister>;

    static constexpr unsigned registrationIdPos_ = 2;
};

//------------------------------------------------------------------------------
class Unregistered : public Command<MessageKind::unregistered>
{
public:
    Unregistered(RequestId rid) : Base(rid) {}

    Unregistered(Message&& msg) : Base(std::move(msg)) {}

private:
    using Base = Command<MessageKind::unregistered>;
};

} // namespace internal

//------------------------------------------------------------------------------

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CONTROLINFO_HPP
