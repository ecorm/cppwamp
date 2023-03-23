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
#include "../errorcodes.hpp"
#include "../erroror.hpp"
#include "../variant.hpp"
#include "../wampdefs.hpp"
#include "messagetraits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct Message
{
    using RequestKey = std::pair<MessageKind, RequestId>;

    static ErrorOr<Message> parse(Array&& fields)
    {
        MessageKind kind = parseMsgType(fields);
        const auto unex = makeUnexpectedError(WampErrc::protocolViolation);
        if (kind == MessageKind::none)
            return unex;

        auto traits = MessageTraits::lookup(kind);
        if ( fields.size() < traits.minSize || fields.size() > traits.maxSize )
            return unex;

        assert(fields.size() <=
               std::extent<decltype(traits.fieldTypes)>::value);
        for (size_t i=0; i<fields.size(); ++i)
        {
            if (fields[i].typeId() != traits.fieldTypes[i])
                return unex;
        }

        return Message(kind, std::move(fields));
    }

    static MessageKind parseMsgType(const Array& fields)
    {
        auto result = MessageKind::none;

        if (!fields.empty())
        {
            const auto& first = fields.front();
            if (first.is<Int>())
            {
                using T = std::underlying_type<MessageKind>::type;
                static constexpr auto max = std::numeric_limits<T>::max();
                auto n = first.to<Int>();
                if (n >= 0 && n <= max)
                {
                    result = static_cast<MessageKind>(n);
                    if (!MessageTraits::lookup(result).isValidKind())
                        result = MessageKind::none;
                }
            }
        }
        return result;
    }

    Message() : kind_(MessageKind::none) {}

    Message(MessageKind kind, Array&& messageFields)
        : kind_(kind), fields_(std::move(messageFields))
    {
        if (fields_.empty())
            fields_.push_back(static_cast<Int>(kind));
        else
            fields_.at(0) = static_cast<Int>(kind);
    }

    void setKind(MessageKind t)
    {
        kind_ = t;
        fields_.at(0) = Int(t);
    }

    void setRequestId(RequestId reqId)
    {
        auto idPos = traits().requestIdPosition;
        assert(idPos != 0);
        fields_.at(idPos) = reqId;
    }

    MessageKind kind() const {return kind_;}

    const MessageTraits& traits() const {return MessageTraits::lookup(kind_);}

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

    bool isRequest() const {return traits().isRequest;}

    bool hasRequestId() const {return traits().requestIdPosition != 0;}

    RequestId requestId() const
    {
        auto idPos = traits().requestIdPosition;
        return (idPos == 0) ? 0 : fields_.at(idPos).to<RequestId>();
    }

    RequestKey requestKey() const
    {
        auto repliesToType = repliesTo();
        auto reqType = (repliesToType == MessageKind::none) ? kind_
                                                            : repliesToType;
        return std::make_pair(reqType, requestId());
    }

    bool isReply() const {return traits().repliesTo != MessageKind::none;}

    MessageKind repliesTo() const
    {
        return kind_ == MessageKind::error ? fields_.at(1).to<MessageKind>()
                                           : traits().repliesTo;
    }

    bool isProgressive() const
    {
        if (kind_ != MessageKind::call && kind_ != MessageKind::result)
            return false;

        if (fields_.size() < 3)
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
    MessageKind kind_;
    mutable Array fields_; // Mutable for lazy-loaded empty payloads
};

//------------------------------------------------------------------------------
template <MessageKind Kind, unsigned I>
struct MessageWithOptions : public Message
{
    static constexpr MessageKind kind = Kind;

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
        : Message(kind, std::move(fields))
    {}
};

//------------------------------------------------------------------------------
template <MessageKind Kind, unsigned I, unsigned J>
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
struct HelloMessage : public MessageWithOptions<MessageKind::hello, 2>
{
    explicit HelloMessage(String realmUri)
        : Base({0, std::move(realmUri), Object{}})
    {}

    const String& uri() const & {return fields_.at(1).as<String>();}

    String&& uri() && {return std::move(fields_.at(1).as<String>());}

private:
    using Base = MessageWithOptions<MessageKind::hello, 2>;
};

//------------------------------------------------------------------------------
struct ChallengeMessage : public MessageWithOptions<MessageKind::challenge, 2>
{
    ChallengeMessage(String authMethod = {}, Object opts = {})
        : Base({0, std::move(authMethod), std::move(opts)})
    {}

    const String& authMethod() const {return fields_.at(1).as<String>();}

private:
    using Base = MessageWithOptions<MessageKind::challenge, 2>;
};

//------------------------------------------------------------------------------
struct AuthenticateMessage
    : public MessageWithOptions<MessageKind::authenticate, 2>
{
    explicit AuthenticateMessage(String sig, Object opts = {})
        : Base({0, std::move(sig), std::move(opts)})
    {}

    const String& signature() const {return fields_.at(1).as<String>();}

private:
    using Base = MessageWithOptions<MessageKind::authenticate, 2>;
};

//------------------------------------------------------------------------------
struct WelcomeMessage : public MessageWithOptions<MessageKind::welcome, 2>
{
    explicit WelcomeMessage(SessionId sid = 0, Object opts = {})
        : Base({0, sid, std::move(opts)})
    {}

    SessionId sessionId() const {return fields_.at(1).to<SessionId>();}

private:
    using Base = MessageWithOptions<MessageKind::welcome, 2>;
};

//------------------------------------------------------------------------------
struct AbortMessage : public MessageWithOptions<MessageKind::abort, 1>
{
    explicit AbortMessage(String reason = "", Object opts = {})
        : Base({0, std::move(opts), std::move(reason)}) {}

    const String& uri() const {return fields_.at(2).as<String>();}

private:
    using Base = MessageWithOptions<MessageKind::abort, 1>;
};

//------------------------------------------------------------------------------
struct GoodbyeMessage : public MessageWithOptions<MessageKind::goodbye, 1>
{
    explicit GoodbyeMessage(String reason = "", Object opts = {})
        : Base({0, std::move(opts), std::move(reason)})
    {}

    explicit GoodbyeMessage(AbortMessage&& msg)
        : Base(std::move(msg).fields())
    {}

    const String& uri() const {return fields_.at(2).as<String>();}

    AbortMessage& transformToAbort()
    {
        setKind(MessageKind::abort);
        auto& base = static_cast<Message&>(*this);
        return static_cast<AbortMessage&>(base);
    }

private:
    using Base = MessageWithOptions<MessageKind::goodbye, 1>;
};

//------------------------------------------------------------------------------
struct ErrorMessage : public MessageWithPayload<MessageKind::error, 3, 5>
{
    explicit ErrorMessage(String reason = "", Object opts = {})
        : Base({0, 0, 0, std::move(opts), std::move(reason)})
    {}

    explicit ErrorMessage(MessageKind reqKind, RequestId reqId, String reason,
                          Object opts = {})
        : Base({0, Int(reqKind), reqId, std::move(opts), std::move(reason)})
    {}

    void setRequestInfo(MessageKind reqKind, RequestId reqId) const
    {
        fields_.at(1) = Int(reqKind);
        fields_.at(2) = reqId;
    }

    MessageKind requestKind() const
    {
        using N = std::underlying_type<MessageKind>::type;
        auto n = fields_.at(1).to<N>();
        return static_cast<MessageKind>(n);
    }

    RequestId requestId() const {return fields_.at(2).to<RequestId>();}

    const String& uri() const {return fields_.at(4).as<String>();}

private:
    using Base = MessageWithPayload<MessageKind::error, 3, 5>;
};

//------------------------------------------------------------------------------
struct PublishMessage : public MessageWithPayload<MessageKind::publish, 2, 4>
{
    explicit PublishMessage(String topic, Object opts = {})
        : Base({0, 0, std::move(opts), std::move(topic)})
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

    const String& uri() const {return fields_.at(3).as<String>();}

private:
    using Base = MessageWithPayload<MessageKind::publish, 2, 4>;
};

//------------------------------------------------------------------------------
struct PublishedMessage : public Message
{
    static constexpr auto kind = MessageKind::published;

    PublishedMessage() : PublishedMessage(0, 0) {}

    PublishedMessage(RequestId r, PublicationId p)
        : Message(kind, {0, r, p})
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

    PublicationId publicationId() const
    {
        return fields_.at(2).to<PublicationId>();
    }
};

//------------------------------------------------------------------------------
struct SubscribeMessage : public MessageWithOptions<MessageKind::subscribe, 2>
{
    explicit SubscribeMessage(String topic)
        : Base({0, 0, Object{}, std::move(topic)})
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

    const String& uri() const & {return fields_.at(3).as<String>();}

    String&& uri() && {return std::move(fields_.at(3).as<String>());}

private:
    using Base = MessageWithOptions<MessageKind::subscribe, 2>;
};

//------------------------------------------------------------------------------
struct SubscribedMessage : public Message
{
    static constexpr auto kind = MessageKind::subscribed;

    SubscribedMessage() : SubscribedMessage(0, 0) {}

    SubscribedMessage(RequestId rid, SubscriptionId sid)
        : Message(kind, {0, rid, sid}) {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

    SubscriptionId subscriptionId() const
    {
        return fields_.at(2).to<SubscriptionId>();
    }
};

//------------------------------------------------------------------------------
struct UnsubscribeMessage : public Message
{
    static constexpr auto kind = MessageKind::unsubscribe;

    explicit UnsubscribeMessage(SubscriptionId subId)
        : Message(kind, {0, 0, subId})
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

    SubscriptionId subscriptionId() const
    {
        return fields_.at(2).to<SubscriptionId>();
    }
};

//------------------------------------------------------------------------------
struct UnsubscribedMessage : public Message
{
    static constexpr auto kind = MessageKind::unsubscribed;

    explicit UnsubscribedMessage(RequestId reqId = 0)
        : Message(kind, {0, reqId}) {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}
};

//------------------------------------------------------------------------------
struct EventMessage : public MessageWithPayload<MessageKind::event, 3, 4>
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
    using Base = MessageWithPayload<MessageKind::event, 3, 4>;
};

//------------------------------------------------------------------------------
struct CallMessage : public MessageWithPayload<MessageKind::call, 2, 4>
{
    explicit CallMessage(String uri = {}, Object opts = {})
        : Base({0, 0, std::move(opts), std::move(uri)})
    {}

    void setUri(String uri) {fields_.at(3) = std::move(uri);}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

    const String& uri() const {return fields_.at(3).as<String>();}

private:
    using Base = MessageWithPayload<MessageKind::call, 2, 4>;
};

//------------------------------------------------------------------------------
struct RegisterMessage : public MessageWithOptions<MessageKind::enroll, 2>
{
    explicit RegisterMessage(String uri, Object opts = {})
        : Base({0, 0, std::move(opts), std::move(uri)})
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

    const String& uri() const & {return fields_.at(3).as<String>();}

    String&& uri() && {return std::move(fields_.at(3).as<String>());}

private:
    using Base = MessageWithOptions<MessageKind::enroll, 2>;
};

//------------------------------------------------------------------------------
struct RegisteredMessage : public Message
{
    static constexpr auto kind = MessageKind::registered;

    RegisteredMessage() : RegisteredMessage(0, 0) {}

    RegisteredMessage(RequestId reqId, RegistrationId regId)
        : Message(kind, {0, reqId, regId})
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

    RegistrationId registrationId() const
    {
        return fields_.at(2).to<RegistrationId>();
    }
};

//------------------------------------------------------------------------------
struct UnregisterMessage : public Message
{
    static constexpr auto kind = MessageKind::unregister;

    explicit UnregisterMessage(RegistrationId regId)
        : Message(kind, {0, 0, regId})
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

    RegistrationId registrationId() const
    {
        return fields_.at(2).to<RegistrationId>();
    }
};

//------------------------------------------------------------------------------
struct UnregisteredMessage : public Message
{
    static constexpr auto kind = MessageKind::unregistered;

    explicit UnregisteredMessage(RequestId r = 0) : Message(kind, {0, r}) {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}
};

//------------------------------------------------------------------------------
struct InvocationMessage :
    public MessageWithPayload<MessageKind::invocation, 3, 4>
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
    using Base =  MessageWithPayload<MessageKind::invocation, 3, 4>;
};

//------------------------------------------------------------------------------
struct YieldMessage : public MessageWithPayload<MessageKind::yield, 2, 3>
{
    explicit YieldMessage(RequestId reqId = nullId(), Object opts = {})
        : Base({0, reqId, std::move(opts)})
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

private:
    using Base =  MessageWithPayload<MessageKind::yield, 2, 3>;
};

//------------------------------------------------------------------------------
struct ResultMessage : public MessageWithPayload<MessageKind::result, 2, 3>
{
    explicit ResultMessage(Object opts = {}) : Base({0, 0, std::move(opts)}) {}

    explicit ResultMessage(YieldMessage&& msg)
        : Base(std::move(msg).fields())
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

    YieldMessage& transformToYield()
    {
        setKind(MessageKind::yield);
        auto& base = static_cast<Message&>(*this);
        return static_cast<YieldMessage&>(base);
    }

private:
    using Base = MessageWithPayload<MessageKind::result, 2, 3>;
};

//------------------------------------------------------------------------------
struct CancelMessage : public MessageWithOptions<MessageKind::cancel, 2>
{
    explicit CancelMessage(RequestId reqId, Object opts = {})
        : Base({0, reqId, std::move(opts)})
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

private:
    using Base = MessageWithOptions<MessageKind::cancel, 2>;
};

//------------------------------------------------------------------------------
struct InterruptMessage : public MessageWithOptions<MessageKind::interrupt, 2>
{
    explicit InterruptMessage(RequestId reqId = 0, Object opts = {})
        : Base({0, reqId, std::move(opts)})
    {}

    RequestId requestId() const {return fields_.at(1).to<RequestId>();}

private:
    using Base = MessageWithOptions<MessageKind::interrupt, 2>;
};

//------------------------------------------------------------------------------
template <typename TDerived>
TDerived& messageCast(Message& msg)
{
    assert(msg.kind() == TDerived::kind);
    return static_cast<TDerived&>(msg);
}

//------------------------------------------------------------------------------
template <typename TDerived>
const TDerived& messageCast(const Message& msg)
{
    assert(msg.kind() == TDerived::kind);
    return static_cast<const TDerived&>(msg);
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WAMPMESSAGE_HPP
