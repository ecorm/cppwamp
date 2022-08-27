/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../error.hpp"
#include <map>
#include <sstream>
#include <type_traits>
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
           "    error value = " << ec.value() << "\n"
           "    category = \"" << ec.category().name() << "\"\n"
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

//------------------------------------------------------------------------------
// error::Decode exception
//------------------------------------------------------------------------------

CPPWAMP_INLINE Decode::Decode(const std::string& what)
    : std::runtime_error("wamp::error::Decode: " + what)
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
/* sessionAbortedByPeer   */ "Session aborted by other peer",
/* allTransportsFailed    */ "All transports failed during connection",
/* joinError              */ "Join error reported by router",
/* publishError           */ "Publish error reported by broker",
/* subscribeError         */ "Subscribe error reported by broker",
/* unsubscribeError       */ "Unsubscribe error reported by broker",
/* registerError          */ "Register error reported by dealer",
/* unregisterError        */ "Unregister error reported by dealer",
/* callError              */ "Call error reported by callee or dealer",
/* invalidState,          */ "Invalid state for this operation",

/* invalidUri             */ "An invalid WAMP URI was provided",
/* noSuchProcedure        */ "No procedure was registered under the given URI",
/* procedureAlreadyExists */ "A procedure with the given URI is already registered",
/* noSuchRegistration     */ "Could not unregister; the given registration is not active",
/* noSuchSubscription     */ "Could not unsubscribe; the given subscription is not active",
/* invalidArgument        */ "The given argument types/values are not acceptable to the callee",
/* systemShutdown         */ "The other peer is shutting down",
/* closeRealm             */ "The other peer is leaving the realm",
/* goodbyeAndOut          */ "Session ended successfully",
/* protocolViolation      */ "Invalid WAMP message for current session state",
/* notAuthorized          */ "This peer is not authorized to perform the operation",
/* authorizationFailed    */ "The authorization operation failed",
/* noSuchRealm            */ "Attempt to join non-existent realm",
/* noSuchRole             */ "Attempt to authenticate under unsupported role",
/* cancelled              */ "A previously issued call was cancelled",
/* optionNotAllowed       */ "Option is disallowed by the router",
/* noEligibleCallee       */ "Call options lead to the exclusion of all callees providing the procedure",
/* discloseMeDisallowed   */ "Router rejected client request to disclose its identity",
/* networkFailure         */ "Router encountered a network failure"
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
//-----------------------------------------------------------------------------
CPPWAMP_INLINE bool lookupWampErrorUri(
    const std::string& uri, ///< The URI to search under.
    SessionErrc fallback,   ///< Defaul value to used if the URI was not found.
    SessionErrc& result     /**< [out] The error code corresponding to the given
                                 URI, or the given fallback value if not found */
)
{
    /* TODO: Add:
        wamp.error.unavailable
        wamp.error.no_such_subscription
        wamp.error.feature_not_supported
        wamp.error.option_disallowed.disclose_me
        no_eligible_callee -> wamp.error.no_available_callee
        cppwamp.error.result_too_long
    */

    using SE = SessionErrc;
    static std::map<std::string, SessionErrc> table =
    {
        {"wamp.error.invalid_uri",                   SE::invalidUri},
        {"wamp.error.no_such_procedure",             SE::noSuchProcedure},
        {"wamp.error.procedure_already_exists",      SE::procedureAlreadyExists},
        {"wamp.error.no_such_registration",          SE::noSuchRegistration},
        {"wamp.error.no_such_subscription",          SE::noSuchSubscription},
        {"wamp.error.invalid_argument",              SE::invalidArgument},
        {"wamp.error.system_shutdown",               SE::systemShutdown},
        {"wamp.error.close_realm",                   SE::closeRealm},
        {"wamp.error.goodbye_and_out",               SE::goodbyeAndOut},
        {"wamp.error.protocol_violation",            SE::protocolViolation},
        {"wamp.error.not_authorized",                SE::notAuthorized},
        {"wamp.error.authorization_failed",          SE::authorizationFailed},
        {"wamp.error.no_such_realm",                 SE::noSuchRealm},
        {"wamp.error.no_such_role",                  SE::noSuchRole},
        {"wamp.error.canceled",                      SE::cancelled},
        {"wamp.error.option_not_allowed",            SE::optionNotAllowed},
        {"wamp.error.no_eligible_callee",            SE::noEligibleCallee},
        {"wamp.error.option_disallowed.disclose_me", SE::discloseMeDisallowed},
        {"wamp.error.network_failure",               SE::networkFailure}
    };

    auto kv = table.find(uri);
    bool found = kv != table.end();
    result = found ? kv->second : fallback;
    return found;
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
        /* success           */ "Operation succesful",
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
// WAMP Protocol Error Codes
//------------------------------------------------------------------------------

CPPWAMP_INLINE const char* ProtocolCategory::name() const noexcept
{
    return "wamp::ProtocolCategory";
}

CPPWAMP_INLINE std::string ProtocolCategory::message(int ev) const
{
    static const std::string msg[] =
    {
        /* success        */ "Operation successful",
        /* badDecode      */ "Error decoding WAMP message payload",
        /* badSchema      */ "Invalid WAMP message schema",
        /* unsupportedMsg */ "Received unsupported WAMP message",
        /* unexpectedMsg  */ "Received unexpected WAMP message"
    };

    if (ev >= 0 && ev < (int)std::extent<decltype(msg)>::value)
        return msg[ev];
    else
        return "Unknown error";
}

CPPWAMP_INLINE bool ProtocolCategory::equivalent(const std::error_code& code,
                                                 int condition) const noexcept
{
    const auto& cat = code.category();

    if (cat == protocolCategory())
        return code.value() == condition;
    else if (condition == (int)ProtocolErrc::success)
        return !code;
    else if (bool(code) && (condition == (int)ProtocolErrc::badDecode))
        return code == DecodingErrc::failure;
    else
        return false;
}

CPPWAMP_INLINE ProtocolCategory::ProtocolCategory() {}

CPPWAMP_INLINE ProtocolCategory& protocolCategory()
{
    static ProtocolCategory instance;
    return instance;
}

CPPWAMP_INLINE std::error_code make_error_code(ProtocolErrc errc)
{
    return std::error_code(static_cast<int>(errc), protocolCategory());
}

CPPWAMP_INLINE std::error_condition make_error_condition(ProtocolErrc errc)
{
    return std::error_condition(static_cast<int>(errc), protocolCategory());
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
        /* success */     "Operation successful",
        /* aborted */     "Operation aborted",
        /* failed */      "Operation failed",
        /* badTxLength */ "Outgoing message exceeds maximum length",
        /* badRxLength */ "Incoming message exceeds maximum length"
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
            return code == std::errc::operation_canceled;

        case (int)TransportErrc::failed:
        {
            return ( code.category() == std::generic_category() ||
                     code.category() == std::system_category() ) && bool(code);
        }

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
