/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "messagetraits.hpp"
#include <array>
#include <type_traits>
#include "../api.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE const MessageTraits& MessageTraits::lookup(MessageKind kind)
{
    using K = MessageKind;
    constexpr VariantKind n = VariantKind::null;
    constexpr VariantKind i = VariantKind::integer;
    constexpr VariantKind s = VariantKind::string;
    constexpr VariantKind a = VariantKind::array;
    constexpr VariantKind o = VariantKind::object;

    // NOLINTBEGIN(modernize-use-bool-literals)
    // NOLINTBEGIN(readability-implicit-bool-conversion)
    static const std::array<MessageTraits, 71> traits{
    {
//                           isRequest -------------------+
//                      forEstablished -----------------+ |
//                      forChallenging ---------------+ | |
//                   forAuthenticating -------------+ | | |
//                          isRouterRx -----------+ | | | |
//                          isClientRx ---------+ | | | | |
//                             maxSize -------+ | | | | | |
//                             minSize -----+ | | | | | | |
//                   requestIdPosition ---+ | | | | | | | |
//                                        | | | | | | | | |
// id     message         repliesTo       | | | | | | | | |
/*  0 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/*  1 */ {"HELLO",        K::none,        0,3,3,0,1,1,0,0,0, {i,s,o,n,n,n,n}},
/*  2 */ {"WELCOME",      K::hello,       0,3,3,1,0,1,1,0,0, {i,i,o,n,n,n,n}},
/*  3 */ {"ABORT",        K::hello,       0,3,3,1,1,1,1,1,0, {i,o,s,n,n,n,n}},
/*  4 */ {"CHALLENGE",    K::none,        0,3,3,1,0,1,1,0,0, {i,s,o,n,n,n,n}},
/*  5 */ {"AUTHENTICATE", K::none,        0,3,3,0,1,0,1,0,0, {i,s,o,n,n,n,n}},
/*  6 */ {"GOODBYE",      K::goodbye,     0,3,3,1,1,0,0,1,0, {i,o,s,n,n,n,n}},
/*  7 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/*  8 */ {"ERROR",        K::error,       2,5,7,1,1,0,0,1,0, {i,i,i,o,s,a,o}},
/*  9 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 10 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 11 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 12 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 13 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 14 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 15 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 16 */ {"PUBLISH",      K::none,        1,4,6,0,1,0,0,1,1, {i,i,o,s,a,o,n}},
/* 17 */ {"PUBLISHED",    K::publish,     1,3,3,1,0,0,0,1,0, {i,i,i,n,n,n,n}},
/* 18 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 19 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 20 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 21 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 22 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 23 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 24 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 25 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 26 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 27 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 28 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 29 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 30 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 31 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},

//                           isRequest -------------------+
//                      forEstablished -----------------+ |
//                      forChallenging ---------------+ | |
//                     forEstablishing -------------+ | | |
//                          isRouterRx -----------+ | | | |
//                          isClientRx ---------+ | | | | |
//                             maxSize -------+ | | | | | |
//                             minSize -----+ | | | | | | |
//                   requestIdPosition ---+ | | | | | | | |
//                                        | | | | | | | | |
// id     message         repliesTo       | | | | | | | | |
/* 32 */ {"SUBSCRIBE",    K::none,        1,4,4,0,1,0,0,1,1, {i,i,o,s,n,n,n}},
/* 33 */ {"SUBSCRIBED",   K::subscribe,   1,3,3,1,0,0,0,1,0, {i,i,i,n,n,n,n}},
/* 34 */ {"UNSUBSCRIBE",  K::none,        1,3,3,0,1,0,0,1,1, {i,i,i,n,n,n,n}},
/* 35 */ {"UNSUBSCRIBED", K::unsubscribe, 1,2,2,1,0,0,0,1,0, {i,i,n,n,n,n,n}},
/* 36 */ {"EVENT",        K::none,        0,4,6,1,0,0,0,1,0, {i,i,i,o,a,o,n}},
/* 37 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 38 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 39 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 40 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 41 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 42 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 43 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 44 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 45 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 46 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 47 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 48 */ {"CALL",         K::none,        1,4,6,0,1,0,0,1,1, {i,i,o,s,a,o,n}},
/* 49 */ {"CANCEL",       K::none,        1,3,3,0,1,0,0,1,0, {i,i,o,n,n,n,n}},
/* 50 */ {"RESULT",       K::call,        1,3,5,1,0,0,0,1,0, {i,i,o,a,o,n,n}},
/* 51 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 52 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 53 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 54 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 55 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 56 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 57 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 58 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 59 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 60 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 61 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 62 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 63 */ {nullptr,        K::none,        0,0,0,0,0,0,0,0,0, {i,n,n,n,n,n,n}},
/* 64 */ {"REGISTER",     K::none,        1,4,4,0,1,0,0,1,1, {i,i,o,s,n,n,n}},
/* 65 */ {"REGISTERED",   K::enroll,      1,3,3,1,0,0,0,1,0, {i,i,i,n,n,n,n}},
/* 66 */ {"UNREGISTER",   K::none,        1,3,3,0,1,0,0,1,1, {i,i,i,n,n,n,n}},
/* 67 */ {"UNREGISTERED", K::unregister,  1,2,2,1,0,0,0,1,0, {i,i,n,n,n,n,n}},
/* 68 */ {"INVOCATION",   K::none,        1,4,6,1,0,0,0,1,1, {i,i,i,o,a,o,n}},
/* 69 */ {"INTERRUPT",    K::none,        1,3,3,1,0,0,0,1,0, {i,i,o,n,n,n,n}},
/* 70 */ {"YIELD",        K::invocation,  1,3,5,0,1,0,0,1,0, {i,i,o,a,o,n,n}}
    }};
    // NOLINTEND(readability-implicit-bool-conversion)
    // NOLINTEND(modernize-use-bool-literals)

    using T = std::underlying_type<MessageKind>::type;
    auto index = static_cast<T>(kind);
    if (index >= static_cast<T>(traits.size()))
        index = 0;
    return traits.at(index);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool MessageTraits::isValidKind() const
{
    return minSize != 0;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool MessageTraits::isValidForState(SessionState state) const
{
    switch (state)
    {
    case SessionState::establishing:
        return forEstablishing;

    case SessionState::authenticating:
        return forAuthenticating;

    case SessionState::established:
    case SessionState::shuttingDown:
        return forEstablished;

    default:
        break;
    }

    return false;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const char* MessageTraits::nameOr(const char* fallback) const
{
    return (name == nullptr) ? fallback : name;
}

} // namespace internal

} // namespace wamp
