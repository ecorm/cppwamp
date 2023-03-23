/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_MESSSAGE_TRAITS_HPP
#define CPPWAMP_MESSSAGE_TRAITS_HPP

#include <cstdint>
#include "../api.hpp"
#include "../wampdefs.hpp"
#include "../variantdefs.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
enum class MessageKind : uint8_t
{
    none         = 0,
    hello        = 1,
    welcome      = 2,
    abort        = 3,
    challenge    = 4,
    authenticate = 5,
    goodbye      = 6,
    error        = 8,
    publish      = 16,
    published    = 17,
    subscribe    = 32,
    subscribed   = 33,
    unsubscribe  = 34,
    unsubscribed = 35,
    event        = 36,
    call         = 48,
    cancel       = 49,
    result       = 50,
    enroll       = 64,
    registered   = 65,
    unregister   = 66,
    unregistered = 67,
    invocation   = 68,
    interrupt    = 69,
    yield        = 70
};

//------------------------------------------------------------------------------
struct CPPWAMP_API MessageTraits
{
    // CPPWAMP_API visibility required by codec component libraries

    static const MessageTraits& lookup(MessageKind kind);

    bool isValidKind() const;

    bool isValidForState(SessionState state) const;

    const char* nameOr(const char* fallback) const;

    const char* name;
    MessageKind repliesTo    : 8;
    size_t requestIdPosition : 8;
    size_t minSize           : 8;
    size_t maxSize           : 8;
    bool isClientRx          : 1;
    bool isRouterRx          : 1;
    bool forEstablishing     : 1;
    bool forAuthenticating   : 1;
    bool forEstablished      : 1;
    bool isRequest           : 1;
    TypeId fieldTypes[7];
};

} // namespace internal

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "messagetraits.ipp"
#endif

#endif // CPPWAMP_MESSSAGE_TRAITS_HPP
