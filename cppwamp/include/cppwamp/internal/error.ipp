/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../error.hpp"
#include <algorithm>
#include <array>
#include <sstream>
#include <type_traits>
#include <utility>
#include <boost/asio/error.hpp>
#include <boost/system/error_category.hpp>
#include <jsoncons/json_error.hpp>
#include <jsoncons_ext/cbor/cbor_error.hpp>
#include <jsoncons_ext/msgpack/msgpack_error.hpp>
#include "../api.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
// WampError Exception
//------------------------------------------------------------------------------

namespace error
{

//------------------------------------------------------------------------------
// error::Failure exception
//------------------------------------------------------------------------------

CPPWAMP_INLINE Failure::Failure(std::error_code ec)
    : std::system_error(ec, makeMessage(ec))
{}

CPPWAMP_INLINE Failure::Failure(std::error_code ec, const std::string& info)
    : std::system_error(ec, makeMessage(ec, info))
{}

CPPWAMP_INLINE std::string Failure::makeMessage(std::error_code ec)
{
    std::ostringstream oss;
    oss << "error::Failure: \n"
           "    error code = " << ec << "\n"
           "    message = \"" << ec.message() << "\"\n";
    return oss.str();
}

CPPWAMP_INLINE std::string Failure::makeMessage(std::error_code ec,
                                                const std::string& info)
{
    return makeMessage(ec) + "    info = \"" + info + "\"\n";
}

//------------------------------------------------------------------------------
// error::Logic exception
//------------------------------------------------------------------------------

/** @details
    The @ref CPPWAMP_LOGIC_ERROR macro should be used instead, which will
    conveniently fill in the `file` and `line` details. */
CPPWAMP_INLINE void Logic::raise(
    const char* file,      ///< The source file where the exception is raised
    int line,              ///< The source line where the exception is raised
    const std::string& msg ///< Describes the cause of the exception
)
{
    std::ostringstream oss;
    oss << file << ':' << line << ", wamp::error::Logic: " << msg;
    throw Logic(oss.str());
}

/** @details
    This function is intended for asserting preconditions.
    The @ref CPPWAMP_LOGIC_CHECK macro should be used instead, which will
    conveniently fill in the `file` and `line` details. */
CPPWAMP_INLINE void Logic::check(
    bool condition,        ///< If `true`, then an exception will be thrown
    const char* file,      ///< The source file where the exception is raised
    int line,              ///< The source line where the exception is raised
    const std::string& msg ///< Describes the cause of the exception
)
{
    if (!condition)
        raise(file, line, msg);
}

//------------------------------------------------------------------------------
// error::BadType exception and its subclasses
//------------------------------------------------------------------------------

CPPWAMP_INLINE BadType::BadType(const std::string& what)
    : std::runtime_error(what)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Access::Access(const std::string& what)
    : BadType("wamp::error::Access: " + what)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Access::Access(const std::string& from, const std::string& to)
    : Access("Attemping to access field type " + from + " as " + to)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Conversion::Conversion(const std::string& what)
    : BadType("wamp::error::Conversion: " + what)
{}

} // namespace error


//------------------------------------------------------------------------------
// WAMP Error Codes
//------------------------------------------------------------------------------

CPPWAMP_INLINE const char* SessionCategory::name() const noexcept
{
    return "wamp::SessionCategory";
}

CPPWAMP_INLINE std::string SessionCategory::message(int ev) const
{
    static const std::string msg[] =
    {
/* success                */ "Operation successful",
/* sessionEnded           */ "Operation aborted; session ended by this peer",
/* sessionEndedByPeer     */ "Session ended by other peer",
/* sessionAborted         */ "Session aborted by this peer",
/* sessionAbortedByPeer   */ "Session aborted by other peer",
/* allTransportsFailed    */ "All transports failed during connection",
/* joinError              */ "Join error reported by router",
/* publishError           */ "Publish error reported by broker",
/* subscribeError         */ "Subscribe error reported by broker",
/* unsubscribeError       */ "Unsubscribe error reported by broker",
/* registerError          */ "Register error reported by dealer",
/* unregisterError        */ "Unregister error reported by dealer",
/* callError              */ "Call error reported by callee or dealer",
/* invalidState           */ "Invalid state for this operation",
/* noSuchOption           */ "Missing WAMP message option",
/* badOption,             */ "Invalid WAMP message option",

/* invalidUri             */ "An invalid WAMP URI was provided",
/* noSuchProcedure        */ "No procedure was registered under the given URI",
/* procedureAlreadyExists */ "A procedure with the given URI is already registered",
/* noSuchRegistration     */ "Could not unregister; the given registration is not active",
/* noSuchSubscription     */ "Could not unsubscribe; the given subscription is not active",
/* invalidArgument        */ "The given argument types/values are not acceptable to the callee",
/* systemShutdown         */ "The other peer is shutting down",
/* closeRealm             */ "The other peer is leaving the realm",
/* goodbyeAndOut          */ "Session ended successfully",
/* protocolViolation      */ "Invalid, unexpected, or malformed WAMP message",
/* notAuthorized          */ "This peer is not authorized to perform the operation",
/* authorizationFailed    */ "The authorization operation failed",
/* noSuchRealm            */ "Attempt to join non-existent realm",
/* noSuchRole             */ "Attempt to authenticate under unsupported role",
/* cancelled              */ "A previously issued call was cancelled",
/* optionNotAllowed       */ "Option is disallowed by the router",
/* discloseMeDisallowed   */ "Router rejected client request to disclose its identity",
/* networkFailure         */ "Router encountered a network failure",
/* unavailable            */ "Callee is unable to handle an invocation",
/* noAvailableCallee      */ "All registered callees are unable to handle an invocation",
/* featureNotSupported    */ "Advanced feature is not supported",
/* noEligibleCallee       */ "Call options lead to the exclusion of all callees providing the procedure",
/* payloadSizeExceeded    */ "Serialized payload exceeds transport limits",
/* cannotAuthenticate     */ "Authentication failed",
/* timeout                */ "Operation timed out"
    };

    if (ev >= 0 && ev < (int)std::extent<decltype(msg)>::value)
        return msg[ev];
    else
        return "Unknown error";
}

CPPWAMP_INLINE bool SessionCategory::equivalent(const std::error_code& code,
                                                int condition) const noexcept
{
    if (code.category() == wampCategory())
    {
        if (code.value() == condition)
            return true;
        else
        {
            auto value = static_cast<SessionErrc>(code.value());
            switch (static_cast<SessionErrc>(condition))
            {
            case SessionErrc::joinError:
                return value == SessionErrc::noSuchRealm ||
                       value == SessionErrc::noSuchRole;

            case SessionErrc::sessionEndedByPeer:
                return value == SessionErrc::systemShutdown ||
                       value == SessionErrc::closeRealm;

            case SessionErrc::unsubscribeError:
                return value == SessionErrc::noSuchSubscription;

            case SessionErrc::registerError:
                return value == SessionErrc::procedureAlreadyExists;

            case SessionErrc::unregisterError:
                return value == SessionErrc::noSuchRegistration;

            case SessionErrc::callError:
                return value == SessionErrc::noSuchProcedure ||
                       value == SessionErrc::invalidArgument;

            default: return false;
            }
        }
    }
    else if (condition == (int)SessionErrc::success)
        return !code;
    else
        return false;
}

CPPWAMP_INLINE SessionCategory::SessionCategory() {}

CPPWAMP_INLINE SessionCategory& wampCategory()
{
    static SessionCategory instance;
    return instance;
}

CPPWAMP_INLINE std::error_code make_error_code(SessionErrc errc)
{
    return std::error_code(static_cast<int>(errc), wampCategory());
}

CPPWAMP_INLINE std::error_condition make_error_condition(SessionErrc errc)
{
    return std::error_condition(static_cast<int>(errc), wampCategory());
}

//------------------------------------------------------------------------------
/** @return 'true' if the corresponding error code was found. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE bool errorUriToCode(
    const std::string& uri, ///< The URI to search under.
    SessionErrc fallback,   ///< Default value to used if the URI was not found.
    SessionErrc& result     /**< [out] The error code corresponding to the given
                                 URI, or the given fallback value if not found */
)
{
    struct Record
    {
        const char* uri;
        SessionErrc errc;

        bool operator<(const std::string& key) const {return uri < key;}
    };

    using SE = SessionErrc;
    static const Record sortedByUri[] =
    {
        {"wamp.error.authorization_failed",          SE::authorizationFailed},
        {"wamp.error.canceled",                      SE::cancelled},
        {"wamp.error.cannot_authenticate",           SE::cannotAuthenticate},
        {"wamp.error.close_realm",                   SE::closeRealm},
        {"wamp.error.feature_not_supported",         SE::featureNotSupported},
        {"wamp.error.goodbye_and_out",               SE::goodbyeAndOut},
        {"wamp.error.invalid_argument",              SE::invalidArgument},
        {"wamp.error.invalid_uri",                   SE::invalidUri},
        {"wamp.error.network_failure",               SE::networkFailure},
        {"wamp.error.no_available_callee",           SE::noAvailableCallee},
        {"wamp.error.no_eligible_callee",            SE::noEligibleCallee},
        {"wamp.error.no_such_procedure",             SE::noSuchProcedure},
        {"wamp.error.no_such_realm",                 SE::noSuchRealm},
        {"wamp.error.no_such_registration",          SE::noSuchRegistration},
        {"wamp.error.no_such_role",                  SE::noSuchRole},
        {"wamp.error.no_such_subscription",          SE::noSuchSubscription},
        {"wamp.error.not_authorized",                SE::notAuthorized},
        {"wamp.error.option_disallowed.disclose_me", SE::discloseMeDisallowed},
        {"wamp.error.option_not_allowed",            SE::optionNotAllowed},
        {"wamp.error.payload_size_exceeded",         SE::payloadSizeExceeded},
        {"wamp.error.procedure_already_exists",      SE::procedureAlreadyExists},
        {"wamp.error.protocol_violation",            SE::protocolViolation},
        {"wamp.error.system_shutdown",               SE::systemShutdown},
        {"wamp.error.timeout",                       SE::timeout},
        {"wamp.error.unavailable",                   SE::unavailable}
    };

    auto end = std::end(sortedByUri);
    auto iter = std::lower_bound(std::begin(sortedByUri), end, uri);
    bool found = (iter != end) && (iter->uri == uri);
    result = found ? iter->errc : fallback;
    return found;
}


//------------------------------------------------------------------------------
/** @return The corresponding error URI, or an empty string if not found. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE const std::string& errorCodeToUri(SessionErrc errc)
{
    static const std::string sortedByErrc[] =
    {
        "wamp.error.invalid_uri",
        "wamp.error.no_such_procedure",
        "wamp.error.procedure_already_exists",
        "wamp.error.no_such_registration",
        "wamp.error.no_such_subscription",
        "wamp.error.invalid_argument",
        "wamp.error.system_shutdown",
        "wamp.error.close_realm",
        "wamp.error.goodbye_and_out",
        "wamp.error.protocol_violation",
        "wamp.error.not_authorized",
        "wamp.error.authorization_failed",
        "wamp.error.no_such_realm",
        "wamp.error.no_such_role",
        "wamp.error.canceled",
        "wamp.error.option_not_allowed",
        "wamp.error.option_disallowed.disclose_me",
        "wamp.error.network_failure",
        "wamp.error.unavailable",
        "wamp.error.no_available_callee",
        "wamp.error.feature_not_supported",
        "wamp.error.no_eligible_callee",
        "wamp.error.payload_size_exceeded",
        "wamp.error.cannot_authenticate",
        "wamp.error.timeout"
    };

    static constexpr int extent = std::extent<decltype(sortedByErrc)>::value;
    static constexpr auto firstValue = static_cast<int>(SessionErrc::invalidUri);
    static constexpr auto lastValue = firstValue + extent - 1;
    static const std::string empty;

    auto n = static_cast<int>(errc);
    bool found = (n >= firstValue) && (n <= lastValue);
    auto index = n - firstValue;
    return found ? sortedByErrc[index] : empty;
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
        /* failure           */ "Decoding failed",
        /* emptyInput        */ "Input is empty or has no tokens",
        /* expectedStringKey */ "Expected a string key",
        /* badBase64Length   */ "Invalid Base64 string length",
        /* badBase64Padding  */ "Invalid Base64 padding",
        /* badBase64Char     */ "Invalid Base64 character"
    };

    if (ev >= 0 && ev < (int)std::extent<decltype(msg)>::value)
        return msg[ev];
    else
        return "Unknown error";
}

CPPWAMP_INLINE bool DecodingCategory::equivalent(const std::error_code& code,
                                                 int condition) const noexcept
{
    const auto& cat = code.category();
    if (!code)
    {
        return condition == (int)DecodingErrc::success;
    }
    else if (condition == (int)DecodingErrc::failure)
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
// Generic Transport Error Codes
//------------------------------------------------------------------------------

CPPWAMP_INLINE const char* TransportCategory::name() const noexcept
{
    return "wamp::TransportCategory";
}

CPPWAMP_INLINE std::string TransportCategory::message(int ev) const
{
    static const std::string msg[] =
    {
        /* success */      "Transport operation successful",
        /* aborted */      "Transport operation aborted",
        /* failed */       "Transport operation failed",
        /* disconnected */ "Transport disconnected by other peer",
        /* badTxLength */  "Outgoing message exceeds transport's maximum length",
        /* badRxLength */  "Incoming message exceeds transport's maximum length"
    };

    if (ev >= 0 && ev < (int)std::extent<decltype(msg)>::value)
        return msg[ev];
    else
        return "Unknown error";
}

CPPWAMP_INLINE bool TransportCategory::equivalent(const std::error_code& code,
                                                  int condition) const noexcept
{
    if (code.category() == transportCategory())
    {
        if (code.value() == condition)
            return true;
        else if (condition == (int)TransportErrc::failed)
            return code.value() == (int)TransportErrc::badTxLength ||
                   code.value() == (int)TransportErrc::badRxLength;
        else
            return false;
    }
    else
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


//------------------------------------------------------------------------------
// Raw Socket Transport Error Codes
//------------------------------------------------------------------------------

CPPWAMP_INLINE const char* RawsockCategory::name() const noexcept
{
    return "wamp::RawsockCategory";
}

CPPWAMP_INLINE std::string RawsockCategory::message(int ev) const
{
    static const std::string msg[] =
    {
        /* success */               "Operation succesful",
        /* badSerializer */         "Serializer unsupported",
        /* badMaxLength */          "Maximum message length unacceptable",
        /* reservedBitsUsed */      "Use of reserved bits (unsupported feature)",
        /* maxConnectionsReached */ "Maximum connection count reached"
    };

    static const std::string extra[] =
    {
        /* badHandshake */          "Invalid handshake format from peer",
        /* badMessageType */        "Invalid message type"
    };

    if (ev >= 0 && ev < (int)std::extent<decltype(msg)>::value)
        return msg[ev];
    else if (ev >= 16 && ev < (16 + (int)std::extent<decltype(msg)>::value))
        return extra[ev - 16];
    else
        return "Unknown error";
}

CPPWAMP_INLINE bool RawsockCategory::equivalent(const std::error_code& code,
                                                  int condition) const noexcept
{
    if (code.category() == rawsockCategory())
        return code.value() == condition;
    else
        return !code && (condition == (int)RawsockErrc::success);
}

CPPWAMP_INLINE RawsockCategory::RawsockCategory() {}

CPPWAMP_INLINE RawsockCategory& rawsockCategory()
{
    static RawsockCategory instance;
    return instance;
}

CPPWAMP_INLINE std::error_code make_error_code(RawsockErrc errc)
{
    return std::error_code(static_cast<int>(errc), rawsockCategory());
}

CPPWAMP_INLINE std::error_condition make_error_condition(RawsockErrc errc)
{
    return std::error_condition(static_cast<int>(errc), rawsockCategory());
}

} // namespace wamp
