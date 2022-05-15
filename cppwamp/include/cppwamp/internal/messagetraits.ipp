/*------------------------------------------------------------------------------
              Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
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
//                    forEstablished ------------------------+
//                    forChallenging ---------------------+  |
//                   forEstablishing ------------------+  |  |
//                        isRouterRx ---------------+  |  |  |
//                        isClientRx ------------+  |  |  |  |
//                           maxSize ---------+  |  |  |  |  |
//                           minSize ------+  |  |  |  |  |  |
//                        idPosition ---+  |  |  |  |  |  |  |
//                                      |  |  |  |  |  |  |  |
// id  message         repliesTo        |  |  |  |  |  |  |  |
/*  0, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/*  1, hello        */ {W::none,        0, 3, 3, 0, 1, 1, 0, 0, {i,s,o,n,n,n,n}},
/*  2, welcome      */ {W::none,        0, 3, 3, 1, 0, 1, 1, 0, {i,i,o,n,n,n,n}},
/*  3, abort        */ {W::none,        0, 3, 3, 1, 0, 1, 1, 0, {i,o,s,n,n,n,n}},
/*  4, challenge    */ {W::none,        0, 3, 3, 1, 0, 1, 1, 0, {i,s,o,n,n,n,n}},
/*  5, authenticate */ {W::challenge,   0, 3, 3, 0, 1, 0, 1, 0, {i,s,o,n,n,n,n}},
/*  6, goodbye      */ {W::none,        0, 3, 3, 1, 1, 0, 0, 1, {i,o,s,n,n,n,n}},
/*  7, heartbeat    */ {W::none,        0, 3, 4, 0, 0, 0, 0, 1, {i,i,i,s,n,n,n}},
/*  8, error        */ {W::none,        0, 5, 7, 1, 1, 0, 0, 1, {i,i,i,o,s,a,o}},
/*  9, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 10, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 11, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 12, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 13, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 14, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 15, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 16, publish      */ {W::none,        1, 4, 6, 0, 1, 0, 0, 1, {i,i,o,s,a,o,n}},
/* 17, published    */ {W::publish,     1, 3, 3, 1, 0, 0, 0, 1, {i,i,i,n,n,n,n}},
/* 18, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 19, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 20, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 21, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 22, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 23, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 24, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 25, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 26, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 27, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 28, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 29, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 30, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 31, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},

//                    forEstablished ------------------------+
//                    forChallenging ---------------------+  |
//                   forEstablishing ------------------+  |  |
//                        isRouterRx ---------------+  |  |  |
//                        isClientRx ------------+  |  |  |  |
//                           maxSize ---------+  |  |  |  |  |
//                           minSize ------+  |  |  |  |  |  |
//                        idPosition ---+  |  |  |  |  |  |  |
//                                      |  |  |  |  |  |  |  |
// id  message         repliesTo        |  |  |  |  |  |  |  |
/* 32, subscribe    */ {W::none,        1, 4, 4, 0, 1, 0, 0, 1, {i,i,o,s,n,n,n}},
/* 33, subscribed   */ {W::subscribe,   1, 3, 3, 1, 0, 0, 0, 1, {i,i,i,n,n,n,n}},
/* 34, unsubscribe  */ {W::none,        1, 3, 3, 0, 1, 0, 0, 1, {i,i,i,n,n,n,n}},
/* 35, unsubscribed */ {W::unsubscribe, 1, 2, 2, 1, 0, 0, 0, 1, {i,i,n,n,n,n,n}},
/* 36, event        */ {W::none,        0, 4, 6, 1, 0, 0, 0, 1, {i,i,i,o,a,o,n}},
/* 37, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 38, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 39, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 40, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 41, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 42, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 43, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 44, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 45, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 46, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 47, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 48, call         */ {W::none,        1, 4, 6, 0, 1, 0, 0, 1, {i,i,o,s,a,o,n}},
/* 49, cancel       */ {W::none,        0, 3, 3, 0, 1, 0, 0, 1, {i,i,o,n,n,n,n}},
/* 50, result       */ {W::call,        1, 3, 5, 1, 0, 0, 0, 1, {i,i,o,a,o,n,n}},
/* 51, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 52, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 53, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 54, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 55, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 56, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 57, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 58, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 59, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 60, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 61, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 62, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 63, ---          */ {W::none,        0, 0, 0, 0, 0, 0, 0, 0, {i,n,n,n,n,n,n}},
/* 64, enroll       */ {W::none,        1, 4, 4, 0, 1, 0, 0, 1, {i,i,o,s,n,n,n}},
/* 65, registered   */ {W::enroll,      1, 3, 3, 1, 0, 0, 0, 1, {i,i,i,n,n,n,n}},
/* 66, unregister   */ {W::none,        1, 3, 3, 0, 1, 0, 0, 1, {i,i,i,n,n,n,n}},
/* 67, unregistered */ {W::unregister,  1, 2, 2, 1, 0, 0, 0, 1, {i,i,n,n,n,n,n}},
/* 68, invocation   */ {W::none,        1, 4, 6, 1, 0, 0, 0, 1, {i,i,i,o,a,o,n}},
/* 69, interrupt    */ {W::none,        0, 3, 3, 1, 0, 0, 0, 1, {i,i,o,n,n,n,n}},
/* 70, yield        */ {W::invocation,  0, 3, 5, 0, 1, 0, 0, 1, {i,i,o,a,o,n,n}}
    };

    auto index = static_cast<size_t>(type);
    if (index >= std::extent<decltype(traits)>::value)
        index = 0;
    return traits[index];
}

} // namespace internal

} // namespace wamp
