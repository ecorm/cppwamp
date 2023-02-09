/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_WAMPMESSAGE_HPP
#define CPPWAMP_INTERNAL_WAMPMESSAGE_HPP

#include <cassert>
#include <limits>
#include <type_traits>
#include <utility>
#include "../erroror.hpp"
#include "../variant.hpp"
#include "../wampdefs.hpp"
#include "messagetraits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// Derived types must not contain member variables due to intentional static
// downcasting (to avoid slicing).
//------------------------------------------------------------------------------
struct WampMessage
{
    using RequestKey = std::pair<WampMsgType, RequestId>;

    static ErrorOr<WampMessage> parse(Array&& fields)
    {
        WampMsgType type = parseMsgType(fields);
        auto unex = makeUnexpectedError(SessionErrc::protocolViolation);
        if (type == WampMsgType::none)
            return unex;

        auto traits = MessageTraits::lookup(type);
        if ( fields.size() < traits.minSize || fields.size() > traits.maxSize )
            return unex;

        assert(fields.size() <=
               std::extent<decltype(traits.fieldTypes)>::value);
        for (size_t i=0; i<fields.size(); ++i)
        {
            if (fields[i].typeId() != traits.fieldTypes[i])
                return unex;
        }

        return WampMessage(type, std::move(fields));
    }

    static WampMsgType parseMsgType(const Array& fields)
    {
        auto result = WampMsgType::none;

        if (!fields.empty())
        {
            const auto& first = fields.front();
            if (first.is<Int>())
            {
                using T = std::underlying_type<WampMsgType>::type;
                static constexpr auto max = std::numeric_limits<T>::max();
                auto n = first.to<Int>();
                if (n >= 0 && n <= max)
                {
                    result = static_cast<WampMsgType>(n);
                    if (!MessageTraits::lookup(result).isValidType())
                        result = WampMsgType::none;
                }
            }
        }
        return result;
    }

    WampMessage() : type_(WampMsgType::none) {}

    WampMessage(WampMsgType type, Array&& messageFields)
        : type_(type), fields_(std::move(messageFields))
    {
        if (fields_.empty())
            fields_.push_back(static_cast<Int>(type));
        else
            fields_.at(0) = static_cast<Int>(type);
    }

    void setType(WampMsgType t)
    {
        type_ = t;
        fields_.at(0) = Int(t);
    }

    void setRequestId(RequestId reqId)
    {
        auto idPos = traits().idPosition;
        if (idPos != 0)
            fields_.at(idPos) = reqId;
    }

    WampMsgType type() const {return type_;}

    const MessageTraits& traits() const {return MessageTraits::lookup(type_);}

    const char* name() const
    {
        // Returns nullptr if invalid message type ID field
        auto t = parseMsgType(fields_);
        return MessageTraits::lookup(t).name;
    }

    const char* nameOr(const char* fallback) const
    {
        auto n = name();
        return n == nullptr ? fallback : n;
    }

    size_t size() const {return fields_.size();}

    const Array& fields() const & {return fields_;}

    Array&& fields() && {return std::move(fields_);}

    Variant& at(size_t index) {return fields_.at(index);}

    const Variant& at(size_t index) const {return fields_.at(index);}

    template <typename T>
    T& as(size_t index) {return fields_.at(index).as<T>();}

    template <typename T>
    const T& as(size_t index) const {return fields_.at(index).as<T>();}

    template <typename T>
    T to(size_t index) const {return fields_.at(index).to<T>();}

    bool hasRequestId() const {return traits().idPosition != 0;}

    RequestId requestId() const
    {
        RequestId id = 0;
        if (type_ != WampMsgType::error)
        {
            auto idPos = traits().idPosition;
            if (idPos != 0)
                id = fields_.at(idPos).to<RequestId>();
        }
        else
            id = fields_.at(2).to<RequestId>();
        return id;
    }

    RequestKey requestKey() const
    {
        auto repliesToType = repliesTo();
        auto reqType = (repliesToType == WampMsgType::none) ? type_
                                                            : repliesToType;
        if (type_ == WampMsgType::error)
            reqType = static_cast<WampMsgType>(fields_.at(1).as<Int>());
        return std::make_pair(reqType, requestId());
    }

    WampMsgType repliesTo() const {return traits().repliesTo;}

    bool isProgressiveResponse() const
    {
        if (type_ != WampMsgType::result || fields_.size() < 3)
            return false;

        const auto& optionsField = fields_.at(2);
        if (!optionsField.is<Object>())
            return false;

        const auto& optionsMap = optionsField.as<Object>();
        auto found = optionsMap.find("progress");
        if (found == optionsMap.end())
            return false;

        return found->second.valueOr<bool>(false);
    }

protected:
    WampMsgType type_;
    mutable Array fields_; // Mutable for lazy-loaded empty payloads
};

//------------------------------------------------------------------------------
template <WampMsgType Kind, unsigned I>
struct MessageWithOptions : public WampMessage
{
    static constexpr WampMsgType kind = Kind;

    static constexpr unsigned optionsPos = I;

    const Object& options() const &
    {
        return fields_.at(optionsPos).template as<Object>();
    }

    Object& options() &
    {
        return fields_.at(optionsPos).template as<Object>();
    }

    Object&& options() &&
    {
        return std::move(fields_.at(optionsPos).template as<Object>());
    }

protected:
    explicit MessageWithOptions(Array&& fields)
        : WampMessage(kind, std::move(fields))
    {}
};

//------------------------------------------------------------------------------
template <WampMsgType Kind, unsigned I, unsigned J>
struct MessageWithPayload : public MessageWithOptions<Kind, I>
{
    static constexpr unsigned argsPos = J;
    static constexpr unsigned kwargsPos = J + 1;

    const Array& args() const &
    {
        if (this->fields_.size() <= argsPos)
            this->fields_.emplace_back(Array{});
        return this->fields_[argsPos].template as<Array>();
    }

    Array& args() &
    {
        if (this->fields_.size() <= argsPos)
            this->fields_.emplace_back(Array{});
        return this->fields_[argsPos].template as<Array>();
    }

    const Object& kwargs() const
    {
        if (this->fields_.size() <= kwargsPos)
        {
            if (this->fields_.size() <= argsPos)
                this->fields_.emplace_back(Array{});
            this->fields_.emplace_back(Object{});
        }
        return this->fields_[kwargsPos].template as<Object>();
    }

    Object& kwargs()
    {
        if (this->fields_.size() <= kwargsPos)
        {
            if (this->fields_.size() <= argsPos)
                this->fields_.emplace_back(Array{});
            this->fields_.emplace_back(Object{});
        }
        return this->fields_[kwargsPos].template as<Object>();
    }

protected:
    explicit MessageWithPayload(Array&& fields)
        : MessageWithOptions<Kind, I>(std::move(fields))
    {}
};

//------------------------------------------------------------------------------
struct HelloMessage : public MessageWithOptions<WampMsgType::hello, 2>
{
    explicit HelloMessage(String realmUri)
        : Base({0, std::move(realmUri), Object{}})
    {}

    const String& realmUri() const & {return fields_.at(1).as<String>();}

    String&& realmUri() && {return std::move(fields_.at(1).as<String>());}

private:
    using Base = MessageWithOptions<WampMsgType::hello, 2>;
};

//------------------------------------------------------------------------------
struct ChallengeMessage : public MessageWithOptions<WampMsgType::challenge, 2>
{
    ChallengeMessage(String authMethod = {}, Object opts = {})
        : Base({0, std::move(authMethod), std::move(opts)})
    {}

    const String& authMethod() const {return fields_.at(1).as<String>();}

private:
    using Base = MessageWithOptions<WampMsgType::challenge, 2>;
};

//------------------------------------------------------------------------------
struct AuthenticateMessage
    : public MessageWithOptions<WampMsgType::authenticate, 2>
{
    explicit AuthenticateMessage(String sig, Object opts = {})
        : Base({0, std::move(sig), std::move(opts)})
    {}

    const String& signature() const {return fields_.at(1).as<String>();}

private:
    using Base = MessageWithOptions<WampMsgType::authenticate, 2>;
};

//------------------------------------------------------------------------------
struct WelcomeMessage : public MessageWithOptions<WampMsgType::welcome, 2>
{
    explicit WelcomeMessage(SessionId sid = 0, Object opts = {})
        : Base({0, sid, std::move(opts)})
    {}

    SessionId sessionId() const {return fields_.at(1).to<SessionId>();}

private:
    using Base = MessageWithOptions<WampMsgType::welcome, 2>;
};

//------------------------------------------------------------------------------
struct AbortMessage : public MessageWithOptions<WampMsgType::abort, 1>
{
    explicit AbortMessage(String reason = "", Object opts = {})
        : Base({0, std::move(opts), std::move(reason)}) {}

    const String& reasonUri() const {return fields_.at(2).as<String>();}

private:
    using Base = MessageWithOptions<WampMsgType::abort, 1>;
};

//------------------------------------------------------------------------------
struct GoodbyeMessage : public MessageWithOptions<WampMsgType::goodbye, 1>
{
    explicit GoodbyeMessage(String reason = "", Object opts = {})
        : Base({0, std::move(opts), std::move(reason)})
    {
        if (reasonUri().empty())
            fields_[2] = String("wamp.error.close_realm");
    }

    const String& reasonUri() const {return fields_.at(2).as<String>();}

private:
    using Base = MessageWithOptions<WampMsgType::goodbye, 1>;
};

//------------------------------------------------------------------------------
struct ErrorMessage : public MessageWithPayload<WampMsgType::error, 3, 5>
{
    explicit ErrorMessage(String reason = "", Object opts = {})
        : Base({0, 0, 0, std::move(opts), std::move(reason)})
    {}

    explicit ErrorMessage(WampMsgType reqType, RequestId reqId, String reason,
                          Object opts = {})
        : Base({0, Int(reqType), reqId, std::move(opts), std::move(reason)})
    {}

    void setRequestInfo(WampMsgType reqType, RequestId reqId) const
    {
        fields_.at(1) = Int(reqType);
        fields_.at(2) = reqId;
    }

    WampMsgType requestType() const
    {
        using N = std::underlying_type<WampMsgType>::type;
        auto n = fields_.at(1).to<N>();
        return static_cast<WampMsgType>(n);
    }

    RequestId requestId() const {return fields_.at(2).to<RequestId>();}

    const String& reasonUri() const {return fields_.at(4).as<String>();}

private:
    using Base = MessageWithPayload<WampMsgType::error, 3, 5>;
};

//------------------------------------------------------------------------------
struct PublishMessage : public MessageWithPayload<WampMsgType::publish, 2, 4>
{
    explicit PublishMessage(String topic, Object opts = {})
        : Base({0, 0, std::move(opts), std::move(topic)})
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

    const String& topicUri() const {return fields_.at(3).as<String>();}

private:
    using Base = MessageWithPayload<WampMsgType::publish, 2, 4>;
};

//------------------------------------------------------------------------------
struct PublishedMessage : public WampMessage
{
    static constexpr auto kind = WampMsgType::published;

    PublishedMessage() : PublishedMessage(0, 0) {}

    PublishedMessage(RequestId r, PublicationId p)
        : WampMessage(kind, {0, r, p})
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

    PublicationId publicationId() const
    {
        return fields_.at(2).to<PublicationId>();
    }
};

//------------------------------------------------------------------------------
struct SubscribeMessage : public MessageWithOptions<WampMsgType::subscribe, 2>
{
    explicit SubscribeMessage(String topic)
        : Base({0, 0, Object{}, std::move(topic)})
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

    const String& topicUri() const & {return fields_.at(3).as<String>();}

    String&& topicUri() && {return std::move(fields_.at(3).as<String>());}

private:
    using Base = MessageWithOptions<WampMsgType::subscribe, 2>;
};

//------------------------------------------------------------------------------
struct SubscribedMessage : public WampMessage
{
    static constexpr auto kind = WampMsgType::subscribed;

    SubscribedMessage() : SubscribedMessage(0, 0) {}

    SubscribedMessage(RequestId rid, SubscriptionId sid)
        : WampMessage(kind, {0, rid, sid}) {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

    SubscriptionId subscriptionId() const
    {
        return fields_.at(2).to<SubscriptionId>();
    }
};

//------------------------------------------------------------------------------
struct UnsubscribeMessage : public WampMessage
{
    static constexpr auto kind = WampMsgType::unsubscribe;

    explicit UnsubscribeMessage(SubscriptionId subId)
        : WampMessage(kind, {0, 0, subId})
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

    SubscriptionId subscriptionId() const
    {
        return fields_.at(2).to<SubscriptionId>();
    }
};

//------------------------------------------------------------------------------
struct UnsubscribedMessage : public WampMessage
{
    static constexpr auto kind = WampMsgType::unsubscribed;

    explicit UnsubscribedMessage(RequestId reqId = 0)
        : WampMessage(kind, {0, reqId}) {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}
};

//------------------------------------------------------------------------------
struct EventMessage : public MessageWithPayload<WampMsgType::event, 3, 4>
{
    EventMessage() : Base({0, 0, 0, Object{}}) {}

    EventMessage(PublicationId pubId, Object opts = {})
        : Base({0, null, pubId, std::move(opts)})
    {}

    EventMessage(Array&& publicationFields, SubscriptionId sid,
                 PublicationId pid, Object opts = {})
        : Base(std::move(publicationFields))
    {
        fields_.at(1) = sid;
        fields_.at(2) = pid;
        fields_.at(3) = std::move(opts);
    }

    void setSubscriptionId(SubscriptionId subId)
    {
        fields_.at(0) = subId;
    }

    SubscriptionId subscriptionId() const
    {
        return fields_.at(1).to<SubscriptionId>();
    }

    PublicationId publicationId() const
    {
        return fields_.at(2).to<PublicationId>();
    }

private:
    using Base = MessageWithPayload<WampMsgType::event, 3, 4>;
};

//------------------------------------------------------------------------------
struct CallMessage : public MessageWithPayload<WampMsgType::call, 2, 4>
{
    explicit CallMessage(String uri, Object opts = {})
        : Base({0, 0, std::move(opts), std::move(uri)})
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

    const String& procedureUri() const {return fields_.at(3).as<String>();}

private:
    using Base = MessageWithPayload<WampMsgType::call, 2, 4>;
};

//------------------------------------------------------------------------------
struct RegisterMessage : public MessageWithOptions<WampMsgType::enroll, 2>
{
    explicit RegisterMessage(String uri, Object opts = {})
        : Base({0, 0, std::move(opts), std::move(uri)})
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

    const String& procedureUri() const & {return fields_.at(3).as<String>();}

    String&& procedureUri() && {return std::move(fields_.at(3).as<String>());}

private:
    using Base = MessageWithOptions<WampMsgType::enroll, 2>;
};

//------------------------------------------------------------------------------
struct RegisteredMessage : public WampMessage
{
    static constexpr auto kind = WampMsgType::registered;

    RegisteredMessage() : RegisteredMessage(0, 0) {}

    RegisteredMessage(RequestId reqId, RegistrationId regId)
        : WampMessage(kind, {0, reqId, regId})
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

    RegistrationId registrationId() const
    {
        return fields_.at(2).to<RegistrationId>();
    }
};

//------------------------------------------------------------------------------
struct UnregisterMessage : public WampMessage
{
    static constexpr auto kind = WampMsgType::unregister;

    explicit UnregisterMessage(RegistrationId regId)
        : WampMessage(kind, {0, 0, regId})
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

    RegistrationId registrationId() const
    {
        return fields_.at(2).to<RegistrationId>();
    }
};

//------------------------------------------------------------------------------
struct UnregisteredMessage : public WampMessage
{
    static constexpr auto kind = WampMsgType::unregistered;

    explicit UnregisteredMessage(RequestId r = 0) : WampMessage(kind, {0, r}) {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}
};

//------------------------------------------------------------------------------
struct InvocationMessage :
    public MessageWithPayload<WampMsgType::invocation, 3, 4>
{
    InvocationMessage() : Base({0, 0, 0, Object{}}) {}

    InvocationMessage(Array&& callFields, RegistrationId regId,
                      Object opts = {})
        : Base(std::move(callFields))
    {
        fields_.at(2) = regId;
        fields_.at(3) = std::move(opts);
    }

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

    RegistrationId registrationId() const
    {
        return fields_.at(2).to<RegistrationId>();
    }

private:
    using Base =  MessageWithPayload<WampMsgType::invocation, 3, 4>;
};

//------------------------------------------------------------------------------
struct YieldMessage : public MessageWithPayload<WampMsgType::yield, 2, 3>
{
    explicit YieldMessage(RequestId reqId, Object opts = {})
        : Base({0, reqId, std::move(opts)})
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

private:
    using Base =  MessageWithPayload<WampMsgType::yield, 2, 3>;
};

//------------------------------------------------------------------------------
struct ResultMessage : public MessageWithPayload<WampMsgType::result, 2, 3>
{
    explicit ResultMessage(Object opts = {}) : Base({0, 0, std::move(opts)}) {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

    YieldMessage& transformToYield()
    {
        setType(WampMsgType::yield);
        auto& base = static_cast<WampMessage&>(*this);
        return static_cast<YieldMessage&>(base);
    }

private:
    using Base = MessageWithPayload<WampMsgType::result, 2, 3>;
};

//------------------------------------------------------------------------------
struct CancelMessage : public MessageWithOptions<WampMsgType::cancel, 2>
{
    explicit CancelMessage(RequestId reqId, Object opts = {})
        : Base({0, reqId, std::move(opts)})
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

private:
    using Base = MessageWithOptions<WampMsgType::cancel, 2>;
};

//------------------------------------------------------------------------------
struct InterruptMessage : public MessageWithOptions<WampMsgType::interrupt, 2>
{
    explicit InterruptMessage(RequestId reqId = 0, Object opts = {})
        : Base({0, reqId, std::move(opts)})
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

private:
    using Base = MessageWithOptions<WampMsgType::interrupt, 2>;
};

//------------------------------------------------------------------------------
template <typename TDerived>
TDerived& messageCast(WampMessage& msg)
{
    assert(msg.type() == TDerived::kind);
    return static_cast<TDerived&>(msg);
}

//------------------------------------------------------------------------------
template <typename TDerived>
const TDerived& messageCast(const WampMessage& msg)
{
    assert(msg.type() == TDerived::kind);
    return static_cast<const TDerived&>(msg);
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WAMPMESSAGE_HPP
