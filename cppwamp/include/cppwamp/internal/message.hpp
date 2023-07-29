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
#include "../tagtypes.hpp"
#include "../traits.hpp"
#include "../variant.hpp"
#include "../wampdefs.hpp"
#include "messagetraits.hpp"
#include "passkey.hpp"

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
        const MessageKind kind = parseMsgType(fields);
        const auto unex = makeUnexpectedError(WampErrc::protocolViolation);
        if (kind == MessageKind::none)
            return unex;

        auto traits = MessageTraits::lookup(kind);
        if ( fields.size() < traits.minSize || fields.size() > traits.maxSize )
            return unex;

        assert(fields.size() <= MessageTraits::maxFieldCount);
        for (size_t i=0; i<fields.size(); ++i)
        {
            if (fields.at(i).kind() != traits.fieldKinds.at(i))
                return unex;
        }

        Message msg{std::move(fields)};
        msg.setKind(kind);
        return msg;
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

    template <typename... Ts>
    explicit Message(MessageKind kind, Ts&&... fields)
        : kind_(kind),
          fields_(Array{static_cast<Int>(kind), std::forward<Ts>(fields)...})
    {}

    explicit Message(Array&& array)
        : kind_(MessageKind::none),
          fields_(std::move(array))
    {}

    void setKind(MessageKind t)
    {
        kind_ = t;
        fields_.at(0) = static_cast<Int>(t);
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
        const char* n = name();
        return n == nullptr ? fallback : n;
    }

    const Array& fields() const {return fields_;}

    Array& fields() {return fields_;}

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

    bool isProgress() const
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

private:
    MessageKind kind_;
    mutable Array fields_; // Mutable for lazy-loaded empty payloads
};

//------------------------------------------------------------------------------
template <MessageKind K>
class RequestCommand
{
protected:
    RequestCommand() = default;

    template <typename... Ts>
    explicit RequestCommand(MessageKind kind, Ts&&... fields)
        : message_(kind, std::forward<Ts>(fields)...),
          requestId_(message_.template to<RequestId>(requestIdPos_))
    {}

    explicit RequestCommand(Message&& msg)
        : message_(std::move(msg)),
          requestId_(message_.template to<RequestId>(requestIdPos_))
    {}

    explicit RequestCommand(Array&& array)
        : message_(std::move(array)),
          requestId_(message_.template to<RequestId>(requestIdPos_))
    {}

    const Message& message() const {return message_;}

    Message& message() {return message_;}

    RequestId requestId() const
    {
        return requestId_;
    }

    void setRequestId(RequestId rid)
    {
        requestId_ = rid;
        message_.at(requestIdPos_) = rid;
    }

private:
    static constexpr unsigned requestIdPos_ =
        MessageKindTraits<K>::requestIdPos;

    static constexpr unsigned requestKindPos_ = 1;

    Message message_;
    RequestId requestId_ = nullId();

public: // Internal use only
    RequestId requestId(PassKey) const {return requestId_;}

    std::pair<MessageKind, RequestId> requestKey(PassKey) const
    {
        return {message_.kind(), requestId()};
    }

    void setRequestId(PassKey, RequestId rid) {setRequestId(rid);}

    void setRequestKindToCall(PassKey)
    {
        message_.at(requestKindPos_) = static_cast<unsigned>(MessageKind::call);
    }
};

//------------------------------------------------------------------------------
class NonRequestCommand
{
protected:
    NonRequestCommand() = default;

    template <typename... Ts>
    explicit NonRequestCommand(MessageKind kind, Ts&&... fields)
        : message_(kind, std::forward<Ts>(fields)...)
    {}

    explicit NonRequestCommand(Message&& msg) : message_(std::move(msg)) {}

    explicit NonRequestCommand(Array&& array) : message_(std::move(array)) {}

    const Message& message() const {return message_;}

    Message& message() {return message_;}

private:
    Message message_;

public: // Internal use only
    std::pair<MessageKind, RequestId> requestKey(PassKey) const
    {
        return {message_.kind(), nullId()};
    }
};

template <MessageKind K>
using CommandBase = Conditional<MessageKindTraits<K>::requestIdPos != 0,
                                RequestCommand<K>,
                                NonRequestCommand>;

//------------------------------------------------------------------------------
template <MessageKind K>
class Command : public CommandBase<K>
{
protected:
    Command() = default;

    template <typename... Ts>
    explicit Command(in_place_t, Ts&&... fields)
        : Base(K, std::forward<Ts>(fields)...)
    {}

    explicit Command(internal::Message&& msg) : Base(std::move(msg)) {}

    template <MessageKind M, CPPWAMP_NEEDS(M != K) = 0>
    explicit Command(Command<M>&& command)
        : Base(std::move(command.message().fields()))
    {}

    using CommandBase<K>::message;

private:
    using Base = CommandBase<K>;

    template <MessageKind> friend class Command;

public: // Internal use only
    static constexpr MessageKind messageKind(PassKey) {return K;}

    static constexpr bool isRequest(PassKey)
    {
        return MessageKindTraits<K>::isRequest();
    }

    static constexpr bool hasRequestId(PassKey)
    {
        return MessageKindTraits<K>::requestIdPos != 0;
    }

    static constexpr bool hasUri(PassKey)
    {
        return MessageKindTraits<K>::uriPos != 0;
    }

    internal::Message& message(PassKey) {return this->message();}

    const internal::Message& message(PassKey) const {return this->message();}
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WAMPMESSAGE_HPP
