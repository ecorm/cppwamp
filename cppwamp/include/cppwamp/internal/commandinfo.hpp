/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CONTROLINFO_HPP
#define CPPWAMP_INTERNAL_CONTROLINFO_HPP

#include "../accesslogging.hpp"
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
    Subscribed(RequestId rid, SubscriptionId sid) : Base(in_place, rid, sid) {}

    explicit Subscribed(Message&& msg) : Base(std::move(msg)) {}

    // This overload needed for genericity
    Subscribed(PassKey, Message&& msg) : Base(std::move(msg)) {}

    SubscriptionId subscriptionId() const
    {
        return message().to<SubscriptionId>(subscriptionIdPos_);
    }

    AccessActionInfo info(Uri topic) const
    {
        return {AccessAction::serverSubscribed, requestId(), std::move(topic)};
    }

private:
    using Base = Command<MessageKind::subscribed>;

    static constexpr unsigned subscriptionIdPos_ = 2;
};

//------------------------------------------------------------------------------
class Unsubscribe : public Command<MessageKind::unsubscribe>
{
public:
    explicit Unsubscribe(SubscriptionId sid) : Base(in_place, 0, sid) {}

    explicit Unsubscribe(Message&& msg) : Base(std::move(msg)) {}

    // This overload needed for genericity
    Unsubscribe(PassKey, Message&& msg) : Base(std::move(msg)) {}

    SubscriptionId subscriptionId() const
    {
        return message().to<SubscriptionId>(subscriptionIdPos_);
    }

    AccessActionInfo info() const
    {
        return {AccessAction::clientUnsubscribe, requestId()};
    }

private:
    using Base = Command<MessageKind::unsubscribe>;

    static constexpr unsigned subscriptionIdPos_ = 2;
};

//------------------------------------------------------------------------------
class Unsubscribed : public Command<MessageKind::unsubscribed>
{
public:
    explicit Unsubscribed(RequestId rid) : Base(in_place, rid) {}

    explicit Unsubscribed(Message&& msg) : Base(std::move(msg)) {}

    // This overload needed for genericity
    Unsubscribed(PassKey, Message&& msg) : Base(std::move(msg)) {}

    AccessActionInfo info(Uri topic) const
    {
        return {AccessAction::serverUnsubscribed, requestId(),
                std::move(topic)};
    }

private:
    using Base = Command<MessageKind::unsubscribed>;
};

//------------------------------------------------------------------------------
class Published : public Command<MessageKind::published>
{
public:
    Published(RequestId rid, PublicationId pid) : Base(in_place, rid, pid) {}

    explicit Published(Message&& msg) : Base(std::move(msg)) {}

    // This overload needed for genericity
    Published(PassKey, Message&& msg) : Base(std::move(msg)) {}

    PublicationId publicationId() const
    {
        return message().to<PublicationId>(publicationIdPos_);
    }

    AccessActionInfo info(Uri topic) const
    {
        return {AccessAction::serverPublished, requestId(), std::move(topic)};
    }

private:
    using Base = Command<MessageKind::published>;

    static constexpr unsigned publicationIdPos_ = 2;
};

//------------------------------------------------------------------------------
class Registered : public Command<MessageKind::registered>
{
public:
    Registered(RequestId rid, RegistrationId sid) : Base(in_place, rid, sid) {}

    explicit Registered(Message&& msg) : Base(std::move(msg)) {}

    // This overload needed for genericity
    Registered(PassKey, Message&& msg) : Base(std::move(msg)) {}

    RegistrationId registrationId() const
    {
        return message().to<RegistrationId>(registrationIdPos_);
    }

    AccessActionInfo info(Uri procedure) const
    {
        return {AccessAction::serverRegistered, requestId(),
                std::move(procedure)};
    }

private:
    using Base = Command<MessageKind::registered>;

    static constexpr unsigned registrationIdPos_ = 2;
};

//------------------------------------------------------------------------------
class Unregister : public Command<MessageKind::unregister>
{
public:
    explicit Unregister(RegistrationId rid) : Base(in_place, 0, rid) {}

    explicit Unregister(Message&& msg) : Base(std::move(msg)) {}

    // This overload needed for genericity
    Unregister(PassKey, Message&& msg) : Base(std::move(msg)) {}

    RegistrationId registrationId() const
    {
        return message().to<RegistrationId>(registrationIdPos_);
    }

    AccessActionInfo info() const
    {
        return {AccessAction::clientUnregister, requestId()};
    }

private:
    using Base = Command<MessageKind::unregister>;

    static constexpr unsigned registrationIdPos_ = 2;
};

//------------------------------------------------------------------------------
class Unregistered : public Command<MessageKind::unregistered>
{
public:
    explicit Unregistered(RequestId rid) : Base(in_place, rid) {}

    explicit Unregistered(Message&& msg) : Base(std::move(msg)) {}

    // This overload needed for genericity
    Unregistered(PassKey, Message&& msg) : Base(std::move(msg)) {}

    AccessActionInfo info(Uri procedure) const
    {
        return {AccessAction::serverUnregistered, requestId(),
                std::move(procedure)};
    }

private:
    using Base = Command<MessageKind::unregistered>;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CONTROLINFO_HPP
