/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "messagetraits.hpp"
#include <type_traits>
#include "../api.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE const MessageTraits& MessageTraits::lookup(WampMsgType type)
{
    using W = WampMsgType;
    constexpr TypeId n = TypeId::null;
    constexpr TypeId i = TypeId::integer;
    constexpr TypeId s = TypeId::string;
    constexpr TypeId a = TypeId::array;
    constexpr TypeId o = TypeId::object;

    static const MessageTraits traits[] =
    {
//                      forEstablished ------------------------+
//                      forChallenging ---------------------+  |
//                     forEstablishing ------------------+  |  |
//                          isRouterRx ---------------+  |  |  |
//                          isClientRx ------------+  |  |  |  |
//                             maxSize ---------+  |  |  |  |  |
//                             minSize ------+  |  |  |  |  |  |
//                          idPosition ---+  |  |  |  |  |  |  |
//                                        |  |  |  |  |  |  |  |
// id     message         repliesTo       |  |  |  |  |  |  |  |
/*  0 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/*  1 */ {"HELLO",        W::none,        0, 3, 3, 0, 1, 1, 0, 0, {i,s,o,n,n,n,n}},
/*  2 */ {"WELCOME",      W::none,        0, 3, 3, 1, 0, 1, 1, 0, {i,i,o,n,n,n,n}},
/*  3 */ {"ABORT",        W::none,        0, 3, 3, 1, 0, 1, 1, 1, {i,o,s,n,n,n,n}},
/*  4 */ {"CHALLENGE",    W::none,        0, 3, 3, 1, 0, 1, 1, 0, {i,s,o,n,n,n,n}},
/*  5 */ {"AUTHENTICATE", W::challenge,   0, 3, 3, 0, 1, 0, 1, 0, {i,s,o,n,n,n,n}},
/*  6 */ {"GOODBYE",      W::none,        0, 3, 3, 1, 1, 0, 0, 1, {i,o,s,n,n,n,n}},
/*  7 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/*  8 */ {"ERROR",        W::none,        0, 5, 7, 1, 1, 0, 0, 1, {i,i,i,o,s,a,o}},
/*  9 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 10 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 11 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 12 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 13 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 14 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 15 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 16 */ {"PUBLISH",      W::none,        1, 4, 6, 0, 1, 0, 0, 1, {i,i,o,s,a,o,n}},
/* 17 */ {"PUBLISHED",    W::publish,     1, 3, 3, 1, 0, 0, 0, 1, {i,i,i,n,n,n,n}},
/* 18 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 19 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 20 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 21 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 22 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 23 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 24 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 25 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 26 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 27 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 28 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 29 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 30 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 31 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},

//                      forEstablished ------------------------+
//                      forChallenging ---------------------+  |
//                     forEstablishing ------------------+  |  |
//                          isRouterRx ---------------+  |  |  |
//                          isClientRx ------------+  |  |  |  |
//                             maxSize ---------+  |  |  |  |  |
//                             minSize ------+  |  |  |  |  |  |
//                          idPosition ---+  |  |  |  |  |  |  |
//                                        |  |  |  |  |  |  |  |
// id     message         repliesTo       |  |  |  |  |  |  |  |
/* 32 */ {"SUBSCRIBE",    W::none,        1, 4, 4, 0, 1, 0, 0, 1, {i,i,o,s,n,n,n}},
/* 33 */ {"SUBSCRIBED",   W::subscribe,   1, 3, 3, 1, 0, 0, 0, 1, {i,i,i,n,n,n,n}},
/* 34 */ {"UNSUBSCRIBE",  W::none,        1, 3, 3, 0, 1, 0, 0, 1, {i,i,i,n,n,n,n}},
/* 35 */ {"UNSUBSCRIBED", W::unsubscribe, 1, 2, 2, 1, 0, 0, 0, 1, {i,i,n,n,n,n,n}},
/* 36 */ {"EVENT",        W::none,        0, 4, 6, 1, 0, 0, 0, 1, {i,i,i,o,a,o,n}},
/* 37 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 38 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 39 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 40 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 41 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 42 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 43 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 44 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 45 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 46 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 47 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 48 */ {"CALL",         W::none,        1, 4, 6, 0, 1, 0, 0, 1, {i,i,o,s,a,o,n}},
/* 49 */ {"CANCEL",       W::none,        0, 3, 3, 0, 1, 0, 0, 1, {i,i,o,n,n,n,n}},
/* 50 */ {"RESULT",       W::call,        1, 3, 5, 1, 0, 0, 0, 1, {i,i,o,a,o,n,n}},
/* 51 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 52 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 53 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 54 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 55 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 56 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 57 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 58 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 59 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 60 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 61 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 62 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 63 */ {nullptr,        W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 64 */ {"REGISTER",     W::none,        1, 4, 4, 0, 1, 0, 0, 1, {i,i,o,s,n,n,n}},
/* 65 */ {"REGISTERED",   W::enroll,      1, 3, 3, 1, 0, 0, 0, 1, {i,i,i,n,n,n,n}},
/* 66 */ {"UNREGISTER",   W::none,        1, 3, 3, 0, 1, 0, 0, 1, {i,i,i,n,n,n,n}},
/* 67 */ {"UNREGISTERED", W::unregister,  1, 2, 2, 1, 0, 0, 0, 1, {i,i,n,n,n,n,n}},
/* 68 */ {"INVOCATION",   W::none,        1, 4, 6, 1, 0, 0, 0, 1, {i,i,i,o,a,o,n}},
/* 69 */ {"INTERRUPT",    W::none,        0, 3, 3, 1, 0, 0, 0, 1, {i,i,o,n,n,n,n}},
/* 70 */ {"YIELD",        W::invocation,  0, 3, 5, 0, 1, 0, 0, 1, {i,i,o,a,o,n,n}}
    };

    auto index = static_cast<size_t>(type);
    if (index >= std::extent<decltype(traits)>::value)
        index = 0;
    return traits[index];
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool MessageTraits::isValidType() const
{
    return minSize != 0;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool MessageTraits::isValidRx(SessionState state,
                                             bool isRouter) const
{
    bool valid = isRouter ? isRouterRx : isClientRx;

    if (valid)
    {
        switch (state)
        {
        case SessionState::establishing:
            valid = forEstablishing;
            break;

        case SessionState::authenticating:
            valid = forChallenging;
            break;

        case SessionState::established:
        case SessionState::shuttingDown:
            valid = forEstablished;
            break;

        default:
            valid = false;
            break;
        }
    }

    return valid;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const char* MessageTraits::nameOr(const char* fallback) const
{
    return (name == nullptr) ? fallback : name;
}

} // namespace internal

} // namespace wamp
