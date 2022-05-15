/*------------------------------------------------------------------------------
              Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_WAMPMESSAGE_HPP
#define CPPWAMP_INTERNAL_WAMPMESSAGE_HPP

#include <utility>
#include "../error.hpp"
#include "../variant.hpp"
#include "../wampdefs.hpp"
#include "messagetraits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct WampMessage
{
    using RequestKey = std::pair<WampMsgType, RequestId>;

    static WampMessage parse(Array&& fields, std::error_code& ec)
    {
        ProtocolErrc errc = ProtocolErrc::success;
        WampMsgType type = WampMsgType::none;
        if (fields.empty() || !fields.at(0).is<Int>())
            errc = ProtocolErrc::badSchema;
        else
        {
            type = static_cast<WampMsgType>(fields.at(0).as<Int>());
            auto traits = MessageTraits::lookup(type);
            if ( fields.size() < traits.minSize ||
                 fields.size() > traits.maxSize )
            {
                errc = ProtocolErrc::badSchema;
            }
            else
            {
                assert(fields.size() <=
                       std::extent<decltype(traits.fieldTypes)>::value);
                for (size_t i=0; i<fields.size(); ++i)
                    if (fields.at(i).typeId() != traits.fieldTypes[i])
                    {
                        errc = ProtocolErrc::badSchema;
                        break;
                    }
            }
        }
        ec = make_error_code(errc);
        return WampMessage(type, std::move(fields));
    }

    WampMessage() : type(WampMsgType::none) {}

    WampMessage(WampMsgType type, Array messageFields)
        : type(type), fields(std::move(messageFields))
    {
        if (fields.empty())
            fields.push_back(static_cast<Int>(type));
        else
            fields.at(0) = static_cast<Int>(type);
    }

    const MessageTraits& traits() const {return MessageTraits::lookup(type);}

    size_t size() const {return fields.size();}

    Variant& at(size_t index) {return fields.at(index);}

    const Variant& at(size_t index) const {return fields.at(index);}

    template <typename T>
    T& as(size_t index) {return fields.at(index).as<T>();}

    template <typename T>
    const T& as(size_t index) const {return fields.at(index).as<T>();}

    template <typename T>
    T to(size_t index) const {return fields.at(index).to<T>();}

    RequestId requestId() const
    {
        RequestId id = 0;
        if (type != WampMsgType::error)
        {
            auto idPos = traits().idPosition;
            if (idPos != 0)
                id = fields.at(idPos).to<RequestId>();
        }
        else
            id = fields.at(2).to<RequestId>();
        return id;
    }

    RequestKey requestKey() const
    {
        WampMsgType reqType = (type != WampMsgType::error) ? type :
                static_cast<WampMsgType>(fields.at(1).as<Int>());
        return std::make_pair(reqType, requestId());
    }

    WampMsgType repliesTo() const {return traits().repliesTo;}

    WampMsgType type;
    Array fields;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WAMPMESSAGE_HPP
