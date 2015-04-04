/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <map>
#include <sstream>
#include <type_traits>
#include "config.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
// WampError Exception
//------------------------------------------------------------------------------

namespace error
{

//------------------------------------------------------------------------------
// error::Wamp exception
//------------------------------------------------------------------------------

CPPWAMP_INLINE Wamp::Wamp(std::error_code ec)
    : std::system_error(ec, makeMessage(ec))
{}

CPPWAMP_INLINE Wamp::Wamp(std::error_code ec,
                                    const std::string& info)
    : std::system_error(ec, makeMessage(ec, info))
{}

CPPWAMP_INLINE std::string Wamp::makeMessage(std::error_code ec)
{
    std::ostringstream oss;
    oss << "error::Wamp: \n"
           "    error value = " << ec.value() << "\n"
           "    category = \"" << ec.category().name() << "\"\n"
           "    message = \"" << ec.message() << "\"\n";
    return oss.str();
}

CPPWAMP_INLINE std::string Wamp::makeMessage(std::error_code ec,
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


} // namespace error


//------------------------------------------------------------------------------
// WAMP Error Codes
//------------------------------------------------------------------------------

CPPWAMP_INLINE const char* WampCategory::name() const noexcept
{
    return "wamp::Wamp";
}

CPPWAMP_INLINE std::string WampCategory::message(int ev) const
{
    static const std::string msg[] =
    {
/* success                */ "Operation successful",
/* sessionEnded           */ "Operation aborted; session ended by this peer",
/* sessionEndedByPeer     */ "Session ended by other peer",
/* allTransportsFailed    */ "All transports failed during connection",
/* joinError              */ "Join error reported by router",
/* publishError           */ "Publish error reported by broker",
/* subscribeError         */ "Subscribe error reported by broker",
/* unsubscribeError       */ "Unsubscribe error reported by broker",
/* registerError          */ "Register error reported by dealer",
/* unregisterError        */ "Unregister error reported by dealer",
/* callError              */ "Call error reported by callee or dealer",

/* invalidUri             */ "An invalid WAMP URI was provided",
/* noSuchProcedure        */ "No procedure was registered under the given URI",
/* procedureAlreadyExists */ "A procedure with the given URI is already registered",
/* noSuchRegistration     */ "Could not unregister; the given registration is not active",
/* noSuchSubscription     */ "Could not unsubscribe; the given subscription is not active",
/* invalidArgument        */ "The given argument types/values are not acceptable to the called procedure",
/* systemShutdown         */ "The other peer is shutting down",
/* closeRealm             */ "The other peer is leaving the realm",
/* goodbyeAndOut          */ "Session ended successfully",
/* notAuthorized          */ "This peer is not authorized to perform the operation",
/* authorizationFailed    */ "The authorization operation failed",
/* noSuchRealm            */ "Attempt to join non-existent realm",
/* noSuchRole             */ "Attempt to authenticate under unsupported role"
    };

    if (ev >= 0 && ev < (int)std::extent<decltype(msg)>::value)
        return msg[ev];
    else
        return "Unknown error";
}

CPPWAMP_INLINE bool WampCategory::equivalent(const std::error_code& code,
                                             int condition) const noexcept
{
    if (code.category() == wampCategory())
    {
        if (code.value() == condition)
            return true;
        else
        {
            auto value = static_cast<WampErrc>(code.value());
            switch (static_cast<WampErrc>(condition))
            {
            case WampErrc::joinError:
                return value == WampErrc::noSuchRealm ||
                       value == WampErrc::noSuchRole;

            case WampErrc::sessionEndedByPeer:
                return value == WampErrc::systemShutdown ||
                       value == WampErrc::closeRealm;

            case WampErrc::unsubscribeError:
                return value == WampErrc::noSuchSubscription;

            case WampErrc::registerError:
                return value == WampErrc::procedureAlreadyExists;

            case WampErrc::unregisterError:
                return value == WampErrc::noSuchRegistration;

            case WampErrc::callError:
                return value == WampErrc::noSuchProcedure ||
                       value == WampErrc::invalidArgument;

            default: return false;
            }
        }
    }
    else if (condition == (int)WampErrc::success)
        return !code;
    else return false;
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
/** @return `true` if a matching WampErrc enumerator was found. */
//-----------------------------------------------------------------------------
CPPWAMP_INLINE bool lookupWampErrorUri(
    const std::string& uri, ///< The URI to search under.
    WampErrc& errc          ///< Out parameter to contain the result, if found.
)
{
    static std::map<std::string, WampErrc> table =
    {
        {"wamp.error.invalid_uri",              WampErrc::invalidUri},
        {"wamp.error.no_such_procedure",        WampErrc::noSuchProcedure},
        {"wamp.error.procedure_already_exists", WampErrc::procedureAlreadyExists},
        {"wamp.error.no_such_registration",     WampErrc::noSuchRegistration},
        {"wamp.error.no_such_subscription",     WampErrc::noSuchSubscription},
        {"wamp.error.invalid_argument",         WampErrc::invalidArgument},
        {"wamp.error.system_shutdown",          WampErrc::systemShutdown},
        {"wamp.error.close_realm",              WampErrc::closeRealm},
        {"wamp.error.goodbye_and_out",          WampErrc::goodbyeAndOut},
        {"wamp.error.not_authorized",           WampErrc::notAuthorized},
        {"wamp.error.authorization_failed",     WampErrc::authorizationFailed},
        {"wamp.error.no_such_realm",            WampErrc::noSuchRealm},
        {"wamp.error.no_such_role",             WampErrc::noSuchRole},
    };

    auto kv = table.find(uri);
    bool found = kv != table.end();
    if (found)
        errc = kv->second;
    return found;
}


//------------------------------------------------------------------------------
// WAMP Protocol Error Codes
//------------------------------------------------------------------------------

CPPWAMP_INLINE const char* ProtocolCategory::name() const noexcept
{
    return "wamp::Protocol";
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
    if (code.category() == protocolCategory())
        return code.value() == condition;
    else if (condition == (int)ProtocolErrc::success)
        return !code;
    else return false;
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
    return "wamp::Transport";
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
    return "wamp::Rawsock";
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
