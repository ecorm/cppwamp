/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ERROR_HPP
#define CPPWAMP_ERROR_HPP

//------------------------------------------------------------------------------
/** @file
    Contains facilities for reporting and describing errors. */
//------------------------------------------------------------------------------

#include <sstream>
#include <string>
#include <system_error>

//------------------------------------------------------------------------------
/** Throws an error::Logic exception having the given message string.
    @param msg A string describing the cause of the exception. */
//------------------------------------------------------------------------------
#define CPPWAMP_LOGIC_ERROR(msg) \
    error::Logic::raise(__FILE__, __LINE__, (msg));

//------------------------------------------------------------------------------
/** Conditionally throws an error::Logic exception having the given message
    string.
    @param cond A boolean expression that, if `true`, will cause an exception
                to be thrown.
    @param msg A string describing the cause of the exception. */
//------------------------------------------------------------------------------
#define CPPWAMP_LOGIC_CHECK(cond, msg) \
    {error::Logic::check((cond), __FILE__, __LINE__, (msg));}

namespace wamp
{

//******************************************************************************
// Exception Types
//******************************************************************************

namespace error
{

//------------------------------------------------------------------------------
/** General purpose runtime exception that wraps a std::error_code. */
//------------------------------------------------------------------------------
class Wamp : public std::system_error
{
public:
    /** Obtains a human-readable message from the given error code. */
    static std::string makeMessage(std::error_code ec);

    /** Obtains a human-readable message from the given error code and
        information string. */
    static std::string makeMessage(std::error_code ec, const std::string& info);

    /** Constructor taking an error code. */
    explicit Wamp(std::error_code ec);

    /** Constructor taking an error code and informational string. */
    Wamp(std::error_code ec, const std::string& info);
};


//------------------------------------------------------------------------------
/** Exception thrown when a pre-condition is not met. */
//------------------------------------------------------------------------------
struct Logic : public std::logic_error
{
    using std::logic_error::logic_error;

    /** Throws an error::Logic exception with the given details. */
    static void raise(const char* file, int line, const std::string& msg);

    /** Conditionally throws an error::Logic exception with the given
        details. */
    static void check(bool condition, const char* file, int line,
                      const std::string& msg);
};

} // namespace error


//******************************************************************************
// WAMP Error Codes
//******************************************************************************

//------------------------------------------------------------------------------
/** Error code values used with the WampCategory error category.
    The equivalencies between these codes are as follows:

    std::error_code                         | Equivalent condition value
    --------------------------------------- | --------------------------
    make_error_code(noSuchRealm)            | joinError
    make_error_code(noSuchRole)             | joinError
    make_error_code(systemShutdown)         | sessionEndedByPeer
    make_error_code(closeRealm)             | sessionEndedByPeer
    make_error_code(noSuchSubscription)     | unsubscribeError
    make_error_code(procedureAlreadyExists) | registerError
    make_error_code(noSuchProcedure)        | callError
    make_error_code(invalidArgument)        | callError */
//------------------------------------------------------------------------------
enum class WampErrc
{
    // Generic errors
    success = 0,            ///< Operation successful
    sessionEnded,           ///< Operation aborted; session ended by this peer
    sessionEndedByPeer,     ///< Session ended by other peer
    allTransportsFailed,    ///< All transports failed during connection
    joinError,              ///< Join error reported by router
    publishError,           ///< Publish error reported by broker
    subscribeError,         ///< Subscribe error reported by broker
    unsubscribeError,       ///< Unsubscribe error reported by broker
    registerError,          ///< Register error reported by dealer.
    unregisterError,        ///< Unregister error reported by dealer
    callError,              ///< Call error reported by callee or dealer

    // Errors mapped to predefined URIs
    invalidUri,             ///< An invalid WAMP URI was provided
    noSuchProcedure,        ///< No procedure was registered under the given URI
    procedureAlreadyExists, ///< A procedure with the given URI is already registered
    noSuchRegistration,     ///< Could not unregister; the given registration is not active
    noSuchSubscription,     ///< Could not unsubscribe; the given subscription is not active
    invalidArgument,        ///< The given argument types/values are not acceptable to the called procedure
    systemShutdown,         ///< The other peer is shutting down
    closeRealm,             ///< The other peer is leaving the realm
    goodbyeAndOut,          ///< Session ended successfully
    notAuthorized,          ///< This peer is not authorized to perform the operation
    authorizationFailed,    ///< The authorization operation failed
    noSuchRealm,            ///< Attempt to join non-existent realm
    noSuchRole              ///< Attempt to authenticate under unsupported role
};

//------------------------------------------------------------------------------
/** std::error_category used for reporting errors at the WAMP layer.
    @see WampErrc */
//------------------------------------------------------------------------------
class WampCategory : public std::error_category
{
public:
    /** Obtains the name of the category. */
    virtual const char* name() const noexcept override;

    /** Obtains the explanatory string. */
    virtual std::string message(int ev) const override;

    /** Compares `error_code` and and error condition for equivalence. */
    virtual bool equivalent(const std::error_code& code,
                            int condition) const noexcept override;

private:
    WampCategory();

    friend WampCategory& wampCategory();
};

//------------------------------------------------------------------------------
/** Obtains a reference to the static error category object for Wamp errors.
    @relates WampCategory */
//------------------------------------------------------------------------------
WampCategory& wampCategory();

//------------------------------------------------------------------------------
/** Creates an error code value from an WampErrc enumerator.
    @relates WampCategory */
//-----------------------------------------------------------------------------
std::error_code make_error_code(WampErrc errc);

//------------------------------------------------------------------------------
/** Creates an error condition value from an WampErrc enumerator.
    @relates WampCategory */
//-----------------------------------------------------------------------------
std::error_condition make_error_condition(WampErrc errc);

//------------------------------------------------------------------------------
/** Looks up the WampErrc enumerator that corresponds to the given error URI.
    @relates WampCategory */
//-----------------------------------------------------------------------------
bool lookupWampErrorUri(const std::string& uri, WampErrc& errc);


//******************************************************************************
// Protocol Error Codes
//******************************************************************************

//------------------------------------------------------------------------------
/** Error code values used with the ProtocolCategory error category. */
//------------------------------------------------------------------------------
enum class ProtocolErrc
{
    success = 0,    ///< Operation successful
    badDecode,      ///< Error decoding WAMP message payload
    badSchema,      ///< Invalid WAMP message schema
    unsupportedMsg, ///< Received unsupported WAMP message
    unexpectedMsg   ///< Received unexpected WAMP message
};

//------------------------------------------------------------------------------
/** std::error_category used for reporting protocol errors related to invalid
    WAMP messages.
    @see ProtocolErrc */
//------------------------------------------------------------------------------
class ProtocolCategory : public std::error_category
{
public:
    /** Obtains the name of the category. */
    virtual const char* name() const noexcept override;

    /** Obtains the explanatory string. */
    virtual std::string message(int ev) const override;

    /** Compares `error_code` and and error condition for equivalence. */
    virtual bool equivalent(const std::error_code& code,
                            int condition) const noexcept override;

private:
    ProtocolCategory();

    friend ProtocolCategory& protocolCategory();
};

//------------------------------------------------------------------------------
/** Obtains a reference to the static error category object for protocol errors.
    @relates ProtocolCategory */
//------------------------------------------------------------------------------
ProtocolCategory& protocolCategory();

//------------------------------------------------------------------------------
/** Creates an error code value from a ProtocolErrc enumerator.
    @relates ProtocolCategory */
//-----------------------------------------------------------------------------
std::error_code make_error_code(ProtocolErrc errc);

//------------------------------------------------------------------------------
/** Creates an error condition value from a ProtocolErrc enumerator.
    @relates ProtocolCategory */
//-----------------------------------------------------------------------------
std::error_condition make_error_condition(ProtocolErrc errc);


//******************************************************************************
// Transport Error Codes
//******************************************************************************

//------------------------------------------------------------------------------
/** Error code values used with the TransportCategory error category. */
//------------------------------------------------------------------------------
enum class TransportErrc
{
    success     = 0, ///< Operation successful
    aborted     = 1, ///< Operation aborted
    failed      = 2, ///< Operation failed
    badTxLength = 3, ///< Outgoing message exceeds maximum length
    badRxLength = 4  ///< Incoming message exceeds maximum length
};

//------------------------------------------------------------------------------
/** std::error_category used for reporting errors at the transport layer.
    @see TransportErrc */
//------------------------------------------------------------------------------
class TransportCategory : public std::error_category
{
public:
    /** Obtains the name of the category. */
    virtual const char* name() const noexcept override;

    /** Obtains the explanatory string. */
    virtual std::string message(int ev) const override;

    /** Compares `error_code` and and error condition for equivalence. */
    virtual bool equivalent(const std::error_code& code,
                            int condition) const noexcept override;

private:
    TransportCategory();

    friend TransportCategory& transportCategory();
};

//------------------------------------------------------------------------------
/** Obtains a reference to the static error category object for transport
    errors.
    @relates TransportCategory */
//------------------------------------------------------------------------------
TransportCategory& transportCategory();

//------------------------------------------------------------------------------
/** Creates an error code value from a TransportErrc enumerator.
    @relates TransportCategory */
//-----------------------------------------------------------------------------
std::error_code make_error_code(TransportErrc errc);

//------------------------------------------------------------------------------
/** Creates an error condition value from a TransportErrc enumerator.
    @relates TransportCategory */
//-----------------------------------------------------------------------------
std::error_condition make_error_condition(TransportErrc errc);


//******************************************************************************
// Raw Socket Error Codes
//******************************************************************************

//------------------------------------------------------------------------------
/** Error code values used with the RawsockCategory error category. */
//------------------------------------------------------------------------------
enum class RawsockErrc
{
    success                 = 0, ///< Operation succesful
    badSerializer           = 1, ///< Serializer unsupported
    badMaxLength            = 2, ///< Maximum message length unacceptable
    reservedBitsUsed        = 3, ///< Use of reserved bits (unsupported feature)
    maxConnectionsReached   = 4, ///< Maximum connection count reached
    // 5-15 reserved for future WAMP raw socket error responses

    badHandshake            = 16, ///< Invalid handshake format from peer
    badMessageType          = 17  ///< Invalid message type
};

//------------------------------------------------------------------------------
/** std::error_category used for reporting errors specific to raw socket
    transports. */
//------------------------------------------------------------------------------
class RawsockCategory : public std::error_category
{
public:
    /** Obtains the name of the category. */
    virtual const char* name() const noexcept override;

    /** Obtains the explanatory string. */
    virtual std::string message(int ev) const override;

    /** Compares `error_code` and and error condition for equivalence. */
    virtual bool equivalent(const std::error_code& code,
                            int condition) const noexcept override;

private:
    RawsockCategory();

    friend RawsockCategory& rawsockCategory();
};

//------------------------------------------------------------------------------
/** Obtains a reference to the static error category object for raw socket
    errors.
    @relates RawsockCategory */
//------------------------------------------------------------------------------
RawsockCategory& rawsockCategory();

//------------------------------------------------------------------------------
/** Creates an error code value from a RawsockErrc enumerator.
    @relates RawsockCategory */
//-----------------------------------------------------------------------------
std::error_code make_error_code(RawsockErrc errc);

//------------------------------------------------------------------------------
/** Creates an error condition value from a RawsockErrc enumerator.
    @relates RawsockCategory */
//-----------------------------------------------------------------------------
std::error_condition make_error_condition(RawsockErrc errc);

} // namespace wamp

//------------------------------------------------------------------------------
#if !defined CPPWAMP_FOR_DOXYGEN
namespace std
{

template <>
struct is_error_condition_enum<wamp::WampErrc> : public true_type {};

template <>
struct is_error_condition_enum<wamp::ProtocolErrc> : public true_type {};

template <>
struct is_error_condition_enum<wamp::TransportErrc> : public true_type {};

template <>
struct is_error_condition_enum<wamp::RawsockErrc> : public true_type {};

} // namespace std
#endif // !defined CPPWAMP_FOR_DOXYGEN


#ifndef CPPWAMP_COMPILED_LIB
#include "internal/error.ipp"
#endif

#endif // CPPWAMP_ERROR_HPP
