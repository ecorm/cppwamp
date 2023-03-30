/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../errorcodes.hpp"
#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <boost/asio/error.hpp>
#include <boost/system/error_category.hpp>
#include <jsoncons/json_error.hpp>
#include <jsoncons_ext/cbor/cbor_error.hpp>
#include <jsoncons_ext/msgpack/msgpack_error.hpp>
#include "../api.hpp"
#include "../exceptions.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TEnum, std::size_t N>
std::string lookupErrorMessage(const char* categoryName, int errorCodeValue,
                               const std::string (&table)[N])
{
    static_assert(N == unsigned(TEnum::count), "");
    if (errorCodeValue >= 0 && errorCodeValue < int(N))
        return table[errorCodeValue];
    else
        return std::string(categoryName) + ':' + std::to_string(errorCodeValue);
}

} // namespace internal


//------------------------------------------------------------------------------
// Generic Error Codes
//------------------------------------------------------------------------------

CPPWAMP_INLINE const char* GenericCategory::name() const noexcept
{
    return "wamp::GenericCategory";
}

CPPWAMP_INLINE std::string GenericCategory::message(int ev) const
{
    static const std::string msg[] =
    {
        /* success       */ "Operation successful",
        /* abandoned     */ "Operation abandoned by this peer",
        /* invalidState  */ "Invalid state for this operation",
        /* absent        */ "Item is absent",
        /* badType,      */ "Invalid or unexpected type"
    };

    return internal::lookupErrorMessage<Errc>("wamp::GenericCategory", ev, msg);
}

CPPWAMP_INLINE bool GenericCategory::equivalent(const std::error_code& code,
                                                int condition) const noexcept
{
    if (code.category() == wampCategory())
        return code.value() == condition;
    else if (condition == (int)Errc::success)
        return !code;
    else
        return false;
}

CPPWAMP_INLINE GenericCategory::GenericCategory() {}

CPPWAMP_INLINE GenericCategory& genericCategory()
{
    static GenericCategory instance;
    return instance;
}

CPPWAMP_INLINE std::error_code make_error_code(Errc errc)
{
    return std::error_code(static_cast<int>(errc), genericCategory());
}

CPPWAMP_INLINE std::error_condition make_error_condition(Errc errc)
{
    return std::error_condition(static_cast<int>(errc), genericCategory());
}


//------------------------------------------------------------------------------
// WAMP Protocol Error Codes
//------------------------------------------------------------------------------

CPPWAMP_INLINE const char* WampCategory::name() const noexcept
{
    return "wamp::WampCategory";
}

CPPWAMP_INLINE std::string WampCategory::message(int ev) const
{
    static const std::string msg[] =
    {
/* success                */ "Operation successful",
/* unknown                */ "Unknown error URI",

/* systemShutdown         */ "The other peer is shutting down",
/* closeRealm             */ "The other peer is leaving the realm",
/* sessionKilled          */ "Session was killed by the other peer",
/* goodbyeAndOut          */ "Session ended successfully",

/* invalidArgument        */ "The given argument types/values are not acceptable to the callee",
/* invalidUri             */ "An invalid WAMP URI was provided",
/* noSuchPrincipal        */ "Authentication attempted with a non-existent authid",
/* noSuchProcedure        */ "No procedure was registered under the given URI",
/* noSuchRealm            */ "Attempt to join non-existent realm",
/* noSuchRegistration     */ "Could not unregister; the given registration is not active",
/* noSuchRole             */ "Attempt to authenticate under unsupported role",
/* noSuchSubscription     */ "Could not unsubscribe; the given subscription is not active",
/* payloadSizeExceeded    */ "Serialized payload exceeds transport size limits",
/* procedureAlreadyExists */ "A procedure with the given URI is already registered",
/* protocolViolation      */ "Invalid, unexpected, or malformed WAMP message",

/* authenticationDenied   */ "Authentication was denied",
/* authenticationFailed   */ "The authentication operation itself failed",
/* authenticationRequired */ "Anonymous authentication not permitted",
/* authorizationDenied    */ "Not authorized to perform the action",
/* authorizationFailed    */ "The authorization operation itself failed",
/* authorizationRequired  */ "Authorization information was missing",
/* cancelled              */ "The previously issued call was cancelled",
/* featureNotSupported    */ "Advanced feature is not supported",
/* discloseMeDisallowed   */ "Client request to disclose its identity was rejected",
/* optionNotAllowed       */ "Option is disallowed by the router",
/* networkFailure         */ "Router encountered a network failure",
/* noAvailableCallee      */ "All registered callees are unable to handle the invocation",
/* noMatchingAuthMethod   */ "No matching authentication method was found",
/* unavailable            */ "Callee is unable to handle the invocation",

/* timeout                */ "Operation timed out"
    };

    return internal::lookupErrorMessage<WampErrc>("wamp::WampCategory", ev,
                                                  msg);
}

CPPWAMP_INLINE bool WampCategory::equivalent(const std::error_code& code,
                                                int condition) const noexcept
{
    if (code.category() == wampCategory())
    {
        if (code.value() == condition)
        {
            return true;
        }
        else
        {
            auto value = static_cast<WampErrc>(code.value());
            switch (static_cast<WampErrc>(condition))
            {
            case WampErrc::sessionKilled:
                return value == WampErrc::systemShutdown ||
                       value == WampErrc::closeRealm;

            case WampErrc::cancelled:
                return value == WampErrc::timeout;

            case WampErrc::optionNotAllowed:
                return value == WampErrc::discloseMeDisallowed;

            default: return false;
            }
        }
    }
    else if (condition == (int)WampErrc::success)
        return !code;
    else
        return false;
}

CPPWAMP_INLINE WampCategory::WampCategory() {}

CPPWAMP_INLINE WampCategory& wampCategory()
{
    static WampCategory instance;
    return instance;
}

CPPWAMP_INLINE std::error_code make_error_code(WampErrc errc)
{
    return std::error_code(static_cast<int>(errc), wampCategory());
}

CPPWAMP_INLINE std::error_condition make_error_condition(WampErrc errc)
{
    return std::error_condition(static_cast<int>(errc), wampCategory());
}

//------------------------------------------------------------------------------
/** @return WampErrc::unknown if the error code was not found. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE WampErrc errorUriToCode(const std::string& uri)
{
    struct Record
    {
        const char* uri;
        WampErrc errc;

        bool operator<(const std::string& key) const {return uri < key;}
    };

    using WE = WampErrc;
    static const Record sortedByUri[] =
    {
        {"cppwamp.error.success",                    WE::success},
        {"cppwamp.error.unknown",                    WE::unknown},
        {"wamp.close.close_realm",                   WE::closeRealm},
        {"wamp.close.goodbye_and_out",               WE::goodbyeAndOut},
        {"wamp.close.normal",                        WE::sessionKilled},
        {"wamp.close.system_shutdown",               WE::systemShutdown},
        {"wamp.error.authentication_denied",         WE::authenticationDenied},
        {"wamp.error.authentication_failed",         WE::authenticationFailed},
        {"wamp.error.authentication_required",       WE::authenticationRequired},
        {"wamp.error.authorization_denied",          WE::authorizationDenied},
        {"wamp.error.authorization_failed",          WE::authorizationFailed},
        {"wamp.error.authorization_required",        WE::authorizationRequired},
        {"wamp.error.canceled",                      WE::cancelled},
        {"wamp.error.close_realm",        /*Legacy*/ WE::closeRealm},
        {"wamp.error.feature_not_supported",         WE::featureNotSupported},
        {"wamp.error.goodbye_and_out",    /*Legacy*/ WE::goodbyeAndOut},
        {"wamp.error.invalid_argument",              WE::invalidArgument},
        {"wamp.error.invalid_uri",                   WE::invalidUri},
        {"wamp.error.network_failure",               WE::networkFailure},
        {"wamp.error.no_auth_method",     /*Legacy*/ WE::noMatchingAuthMethod},
        {"wamp.error.no_available_callee",           WE::noAvailableCallee},
        {"wamp.error.no_matching_auth_method",       WE::noMatchingAuthMethod},
        {"wamp.error.no_such_principal",             WE::noSuchPrincipal},
        {"wamp.error.no_such_procedure",             WE::noSuchProcedure},
        {"wamp.error.no_such_realm",                 WE::noSuchRealm},
        {"wamp.error.no_such_registration",          WE::noSuchRegistration},
        {"wamp.error.no_such_role",                  WE::noSuchRole},
        {"wamp.error.no_such_subscription",          WE::noSuchSubscription},
        {"wamp.error.not_authorized",     /*Legacy*/ WE::authorizationDenied},
        {"wamp.error.option_disallowed.disclose_me", WE::discloseMeDisallowed},
        {"wamp.error.option_not_allowed",            WE::optionNotAllowed},
        {"wamp.error.payload_size_exceeded",         WE::payloadSizeExceeded},
        {"wamp.error.procedure_already_exists",      WE::procedureAlreadyExists},
        {"wamp.error.protocol_violation",            WE::protocolViolation},
        {"wamp.error.system_shutdown",    /*Legacy*/ WE::systemShutdown},
        {"wamp.error.timeout",                       WE::timeout},
        {"wamp.error.unavailable",                   WE::unavailable}
    };

    static constexpr auto extent = std::extent<decltype(sortedByUri)>::value;
    static constexpr unsigned legacyUriCount = 5;
    static_assert(extent == unsigned(WampErrc::count) + legacyUriCount, "");

    auto end = std::end(sortedByUri);
    auto iter = std::lower_bound(std::begin(sortedByUri), end, uri);
    bool found = (iter != end) && (iter->uri == uri);
    return found ? iter->errc : WampErrc::unknown;
}


//------------------------------------------------------------------------------
/** @throws error::Logic if the given code is not a valid enumerator value. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE const std::string& errorCodeToUri(WampErrc errc)
{
    static const std::string sortedByErrc[] =
    {
        "cppwamp.error.success",
        "cppwamp.error.unknown",

        "wamp.close.close_realm",
        "wamp.close.goodbye_and_out",
        "wamp.close.normal",
        "wamp.close.system_shutdown",

        "wamp.error.invalid_argument",
        "wamp.error.invalid_uri",
        "wamp.error.no_such_principal",
        "wamp.error.no_such_procedure",
        "wamp.error.no_such_realm",
        "wamp.error.no_such_registration",
        "wamp.error.no_such_role",
        "wamp.error.no_such_subscription",
        "wamp.error.payload_size_exceeded",
        "wamp.error.procedure_already_exists",
        "wamp.error.protocol_violation",

        "wamp.error.authentication_denied",
        "wamp.error.authentication_failed",
        "wamp.error.authentication_required",
        "wamp.error.authorization_denied",
        "wamp.error.authorization_failed",
        "wamp.error.authorization_required",
        "wamp.error.canceled",
        "wamp.error.feature_not_supported",
        "wamp.error.option_disallowed.disclose_me",
        "wamp.error.option_not_allowed",
        "wamp.error.network_failure",
        "wamp.error.no_available_callee",
        "wamp.error.no_matching_auth_method",
        "wamp.error.timeout",
        "wamp.error.unavailable"
    };

    using T = std::underlying_type<WampErrc>::type;
    static constexpr T extent = std::extent<decltype(sortedByErrc)>::value;
    static_assert(extent == T(WampErrc::count), "");
    auto n = static_cast<T>(errc);
    CPPWAMP_LOGIC_CHECK((n >= 0) && (n < extent),
                        "wamp::errorCodeToUri code is not a valid enumerator");
    return sortedByErrc[n];
}

//------------------------------------------------------------------------------
/** If the error code's category is wampCategory(), returns the same result
    as errorCodeToUri(WampErrc). Otherwise, the format is
    `cppwamp.error.<category name>.<code value>`. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::string errorCodeToUri(std::error_code ec)
{
    if (ec.category() == wampCategory())
        return errorCodeToUri(static_cast<WampErrc>(ec.value()));
    return std::string("cppwamp.error.") + ec.category().name() +
           std::to_string(ec.value());
}

//------------------------------------------------------------------------------
/** The format is `<category>:<value>`. */
//-----------------------------------------------------------------------------
CPPWAMP_INLINE std::string briefErrorCodeString(std::error_code ec)
{
    return std::string{ec.category().name()} + ':' + std::to_string(ec.value());
}

//------------------------------------------------------------------------------
/** The format is `<category>:<value> (<message>)`. */
//-----------------------------------------------------------------------------
CPPWAMP_INLINE std::string detailedErrorCodeString(std::error_code ec)
{
    return std::string{ec.category().name()} + ':' +
           std::to_string(ec.value()) + " (" + ec.message() + ')';
}

//------------------------------------------------------------------------------
// Deserialization Error Codes
//------------------------------------------------------------------------------

CPPWAMP_INLINE const char* DecodingCategory::name() const noexcept
{
    return "wamp::DecodingCategory";
}

CPPWAMP_INLINE std::string DecodingCategory::message(int ev) const
{
    static const std::string msg[] =
    {
        /* success           */ "Decoding succesful",
        /* failed            */ "Decoding failed",
        /* emptyInput        */ "Input is empty or has no tokens",
        /* expectedStringKey */ "Expected a string key",
        /* badBase64Length   */ "Invalid Base64 string length",
        /* badBase64Padding  */ "Invalid Base64 padding",
        /* badBase64Char     */ "Invalid Base64 character"
    };

    return internal::lookupErrorMessage<DecodingErrc>(
        "wamp::DecodingCategory", ev, msg);
}

CPPWAMP_INLINE bool DecodingCategory::equivalent(const std::error_code& code,
                                                 int condition) const noexcept
{
    const auto& cat = code.category();
    if (!code)
    {
        return condition == (int)DecodingErrc::success;
    }
    else if (condition == (int)DecodingErrc::failed)
    {
        return cat == decodingCategory() ||
               cat == jsoncons::json_error_category() ||
               cat == jsoncons::cbor::cbor_error_category() ||
               cat == jsoncons::msgpack::msgpack_error_category();
    }
    else if (cat == decodingCategory())
    {
        return code.value() == condition;
    }
    return false;
}

CPPWAMP_INLINE DecodingCategory::DecodingCategory() {}

CPPWAMP_INLINE DecodingCategory& decodingCategory()
{
    static DecodingCategory instance;
    return instance;
}

CPPWAMP_INLINE std::error_code make_error_code(DecodingErrc errc)
{
    return std::error_code(static_cast<int>(errc), decodingCategory());
}

CPPWAMP_INLINE std::error_condition make_error_condition(DecodingErrc errc)
{
    return std::error_condition(static_cast<int>(errc),
                                decodingCategory());
}


//------------------------------------------------------------------------------
// Transport Error Codes
//------------------------------------------------------------------------------

CPPWAMP_INLINE const char* TransportCategory::name() const noexcept
{
    return "wamp::TransportCategory";
}

CPPWAMP_INLINE std::string TransportCategory::message(int ev) const
{
    static const std::string msg[] =
    {
        /* success        */ "Transport operation successful",
        /* aborted        */ "Transport operation aborted",
        /* disconnected   */ "Transport disconnected by other peer",
        /* failed         */ "Transport operation failed",
        /* exhausted      */ "All transports failed during connection",
        /* tooLong        */ "Incoming message exceeds transport's length limit",
        /* badHandshake   */ "Received invalid handshake",
        /* badCommand     */ "Received invalid transport command",
        /* badSerializer  */ "Unsupported serialization format",
        /* badLengthLimit */ "Unacceptable maximum message length",
        /* badFeature     */ "Unsupported transport feature",
        /* saturated      */ "Connection limit reached"
    };

    return internal::lookupErrorMessage<TransportErrc>(
        "wamp::TransportCategory", ev, msg);
}

CPPWAMP_INLINE bool TransportCategory::equivalent(const std::error_code& code,
                                                  int condition) const noexcept
{
    if (code.category() == transportCategory())
    {
        if (code.value() == condition)
            return true;
        else if (condition == (int)TransportErrc::failed)
            return code.value() > (int)TransportErrc::failed;
        else
            return false;
    }
    else
    {
        switch (condition)
        {
        case (int)TransportErrc::success:
            return !code;

        case (int)TransportErrc::aborted:
            return code == std::errc::operation_canceled ||
                   code == make_error_code(boost::asio::error::operation_aborted);

        case (int)TransportErrc::failed:
        {
            if (!code)
                return false;
            const auto& cat = code.category();
            return cat == std::generic_category() ||
                   cat == std::system_category() ||
                   cat == boost::system::generic_category() ||
                   cat == boost::system::system_category() ||
                   cat == boost::asio::error::get_addrinfo_category() ||
                   cat == boost::asio::error::get_misc_category() ||
                   cat == boost::asio::error::get_netdb_category();
        }

        case (int)TransportErrc::disconnected:
            return code == std::errc::connection_reset ||
                   code == make_error_code(boost::asio::error::connection_reset) ||
                   code == make_error_code(boost::asio::error::eof);

        default:
            return false;
        }
    }
}

CPPWAMP_INLINE TransportCategory::TransportCategory() {}

CPPWAMP_INLINE TransportCategory& transportCategory()
{
    static TransportCategory instance;
    return instance;
}

CPPWAMP_INLINE std::error_code make_error_code(TransportErrc errc)
{
    return std::error_code(static_cast<int>(errc), transportCategory());
}

CPPWAMP_INLINE std::error_condition make_error_condition(TransportErrc errc)
{
    return std::error_condition(static_cast<int>(errc), transportCategory());
}

} // namespace wamp