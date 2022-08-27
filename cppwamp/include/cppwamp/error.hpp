/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ERROR_HPP
#define CPPWAMP_ERROR_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for reporting and describing errors. */
//------------------------------------------------------------------------------

#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include "api.hpp"

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
class CPPWAMP_API Failure : public std::system_error
{
public:
    /** Obtains a human-readable message from the given error code. */
    static std::string makeMessage(std::error_code ec);

    /** Obtains a human-readable message from the given error code and
        information string. */
    static std::string makeMessage(std::error_code ec, const std::string& info);

    /** Constructor taking an error code. */
    explicit Failure(std::error_code ec);

    /** Constructor taking an error code and informational string. */
    Failure(std::error_code ec, const std::string& info);
};


//------------------------------------------------------------------------------
/** Exception thrown when a pre-condition is not met. */
//------------------------------------------------------------------------------
struct CPPWAMP_API Logic : public std::logic_error
{
    using std::logic_error::logic_error;

    /** Throws an error::Logic exception with the given details. */
    static void raise(const char* file, int line, const std::string& msg);

    /** Conditionally throws an error::Logic exception with the given
        details. */
    static void check(bool condition, const char* file, int line,
                      const std::string& msg);
};

//------------------------------------------------------------------------------
/** Base class for exceptions involving invalid Variant types. */
//------------------------------------------------------------------------------
struct CPPWAMP_API BadType : public std::runtime_error
{
    explicit BadType(const std::string& what);
};

//------------------------------------------------------------------------------
/** Exception type thrown when accessing a Variant as an invalid type. */
//------------------------------------------------------------------------------
struct CPPWAMP_API Access : public BadType
{
    explicit Access(const std::string& what);
    Access(const std::string& from, const std::string& to);
};

//------------------------------------------------------------------------------
/** Exception type thrown when converting a Variant to an invalid type. */
//------------------------------------------------------------------------------
struct CPPWAMP_API Conversion : public BadType
{
    explicit Conversion(const std::string& what);
};

//------------------------------------------------------------------------------
/** Exception type thrown when codec deserialization fails.
    @deprecated Decoders now return a `std::error_code`.*/
//------------------------------------------------------------------------------
struct CPPWAMP_API Decode: public std::runtime_error
{
    explicit Decode(const std::string& what);
};

} // namespace error


//******************************************************************************
// Session Error Codes
//******************************************************************************

//------------------------------------------------------------------------------
/** %Error code values used with the SessionCategory error category.
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
enum class SessionErrc
{
    // Generic errors
    success = 0,            ///< Operation successful
    sessionEnded,           ///< Operation aborted; session ended by this peer
    sessionEndedByPeer,     ///< Session ended by other peer
    sessionAbortedByPeer,   ///< Session aborted by other peer
    allTransportsFailed,    ///< All transports failed during connection
    joinError,              ///< Join error reported by router
    publishError,           ///< Publish error reported by broker
    subscribeError,         ///< Subscribe error reported by broker
    unsubscribeError,       ///< Unsubscribe error reported by broker
    registerError,          ///< Register error reported by dealer
    unregisterError,        ///< Unregister error reported by dealer
    callError,              ///< Call error reported by callee or dealer
    invalidState,           ///< Invalid state for this operation

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
    protocolViolation,      ///< Invalid WAMP message for current session state.
    notAuthorized,          ///< This peer is not authorized to perform the operation
    authorizationFailed,    ///< The authorization operation failed
    noSuchRealm,            ///< Attempt to join non-existent realm
    noSuchRole,             ///< Attempt to authenticate under unsupported role
    cancelled,              ///< A previously issued call was cancelled
    optionNotAllowed,       ///< Option is disallowed by the router
    discloseMeDisallowed,   ///< Router rejected client request to disclose its identity
    networkFailure,         ///< Router encountered a network failure
    unavailable,            ///< Callee is unable to handle an invocation
    noAvailableCallee,      ///< All registered callees are unable to handle an invocation
    featureNotSupported,    ///< Advanced feature is not supported

    // Errors mapped to predefined URIs not currently in the WAMP spec
    noEligibleCallee,       ///< Call options lead to the exclusion of all callees providing the procedure
    payloadSizeExceeded     ///< Serialized payload exceeds transport limits
};

//------------------------------------------------------------------------------
/** std::error_category used for reporting errors at the WAMP session layer.
    @see SessionErrc */
//------------------------------------------------------------------------------
class CPPWAMP_API SessionCategory : public std::error_category
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
    CPPWAMP_HIDDEN SessionCategory();

    friend SessionCategory& wampCategory();
};

//------------------------------------------------------------------------------
/** Obtains a reference to the static error category object for Wamp errors.
    @relates SessionCategory */
//------------------------------------------------------------------------------
CPPWAMP_API SessionCategory& wampCategory();

//------------------------------------------------------------------------------
/** Creates an error code value from an SessionErrc enumerator.
    @relates SessionCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_code make_error_code(SessionErrc errc);

//------------------------------------------------------------------------------
/** Creates an error condition value from an SessionErrc enumerator.
    @relates SessionCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_condition make_error_condition(SessionErrc errc);

//------------------------------------------------------------------------------
/** Looks up the SessionErrc enumerator that corresponds to the given error URI.
    @relates SessionCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API bool lookupWampErrorUri(const std::string& uri,
                                    SessionErrc fallback, SessionErrc& result);


//******************************************************************************
// Codec decoding Error Codes
//******************************************************************************

//------------------------------------------------------------------------------
/** %Error code values used with the DecodingCategory error category.
    All of the following non-zero codes are equivalent to the
    DecodingErrc::failure condition:
    - wamp::DecodingErrc
    - `jsoncons::json_errc`
    - `jsoncons::cbor::cbor_errc`
    - `jsoncons::msgpack::msgpack_errc` */
//------------------------------------------------------------------------------
enum class DecodingErrc
{
    success           = 0, ///< Operation succesful
    failure           = 1, ///< Decoding failed
    emptyInput        = 2, ///< Input is empty or has no tokens
    expectedStringKey = 3, ///< Expected a string key
    badBase64Length   = 4, ///< Invalid Base64 string length
    badBase64Padding  = 5, ///< Invalid Base64 padding
    badBase64Char     = 6  ///< Invalid Base64 character
};

//------------------------------------------------------------------------------
/** std::error_category used for reporting deserialization errors.
    @see DecodingErrc */
//------------------------------------------------------------------------------
class CPPWAMP_API DecodingCategory : public std::error_category
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
    CPPWAMP_HIDDEN DecodingCategory();

    friend DecodingCategory& decodingCategory();
};

//------------------------------------------------------------------------------
/** Obtains a reference to the static error category object for deserialization
    errors.
    @relates DecodingCategory */
//------------------------------------------------------------------------------
CPPWAMP_API DecodingCategory& decodingCategory();

//------------------------------------------------------------------------------
/** Creates an error code value from a DecodingErrc enumerator.
    @relates DecodingCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_code make_error_code(DecodingErrc errc);

//------------------------------------------------------------------------------
/** Creates an error condition value from a DecodingErrc enumerator.
    @relates DecodingCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_condition make_error_condition(DecodingErrc errc);


//******************************************************************************
// Protocol Error Codes
//******************************************************************************
// TODO: Deprecate

//------------------------------------------------------------------------------
/** %Error code values used with the ProtocolCategory error category.
    All of the following non-zero codes are equivalent to the
    ProtocolErrc::badDecode condition:
    - wamp::DecodingErrc
    - `jsoncons::json_errc`
    - `jsoncons::cbor::cbor_errc`
    - `jsoncons::msgpack::msgpack_errc` */
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
/** std::error_category used for reporting protocol errors related to badly
    formed WAMP messages.
    @see ProtocolErrc */
//------------------------------------------------------------------------------
class CPPWAMP_API ProtocolCategory : public std::error_category
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
    CPPWAMP_HIDDEN ProtocolCategory();

    friend ProtocolCategory& protocolCategory();
};

//------------------------------------------------------------------------------
/** Obtains a reference to the static error category object for protocol errors.
    @relates ProtocolCategory */
//------------------------------------------------------------------------------
CPPWAMP_API ProtocolCategory& protocolCategory();

//------------------------------------------------------------------------------
/** Creates an error code value from a ProtocolErrc enumerator.
    @relates ProtocolCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_code make_error_code(ProtocolErrc errc);

//------------------------------------------------------------------------------
/** Creates an error condition value from a ProtocolErrc enumerator.
    @relates ProtocolCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_condition make_error_condition(ProtocolErrc errc);


//******************************************************************************
// Transport Error Codes
//******************************************************************************

//------------------------------------------------------------------------------
/** %Error code values used with the TransportCategory error category. */
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
class CPPWAMP_API TransportCategory : public std::error_category
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
    CPPWAMP_HIDDEN TransportCategory();

    friend TransportCategory& transportCategory();
};

//------------------------------------------------------------------------------
/** Obtains a reference to the static error category object for transport
    errors.
    @relates TransportCategory */
//------------------------------------------------------------------------------
CPPWAMP_API TransportCategory& transportCategory();

//------------------------------------------------------------------------------
/** Creates an error code value from a TransportErrc enumerator.
    @relates TransportCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_code make_error_code(TransportErrc errc);

//------------------------------------------------------------------------------
/** Creates an error condition value from a TransportErrc enumerator.
    @relates TransportCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_condition make_error_condition(TransportErrc errc);


//******************************************************************************
// Raw Socket Error Codes
//******************************************************************************

//------------------------------------------------------------------------------
/** %Error code values used with the RawsockCategory error category. */
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
    transports.
    @see RawsockErrc */
//------------------------------------------------------------------------------
class CPPWAMP_API RawsockCategory : public std::error_category
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
    CPPWAMP_HIDDEN RawsockCategory();

    friend RawsockCategory& rawsockCategory();
};

//------------------------------------------------------------------------------
/** Obtains a reference to the static error category object for raw socket
    errors.
    @relates RawsockCategory */
//------------------------------------------------------------------------------
CPPWAMP_API RawsockCategory& rawsockCategory();

//------------------------------------------------------------------------------
/** Creates an error code value from a RawsockErrc enumerator.
    @relates RawsockCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_code make_error_code(RawsockErrc errc);

//------------------------------------------------------------------------------
/** Creates an error condition value from a RawsockErrc enumerator.
    @relates RawsockCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_condition make_error_condition(RawsockErrc errc);

} // namespace wamp


//------------------------------------------------------------------------------
#if !defined CPPWAMP_FOR_DOXYGEN
namespace std
{

template <>
struct CPPWAMP_API is_error_condition_enum<wamp::SessionErrc>
    : public true_type
{};

template <>
struct CPPWAMP_API is_error_condition_enum<wamp::DecodingErrc>
    : public true_type
{};

template <>
struct CPPWAMP_API is_error_condition_enum<wamp::ProtocolErrc>
    : public true_type
{};

template <>
struct CPPWAMP_API is_error_condition_enum<wamp::TransportErrc>
    : public true_type
{};

template <>
struct CPPWAMP_API is_error_condition_enum<wamp::RawsockErrc>
    : public true_type
{};

} // namespace std
#endif // !defined CPPWAMP_FOR_DOXYGEN


#ifndef CPPWAMP_COMPILED_LIB
#include "internal/error.ipp"
#endif

#endif // CPPWAMP_ERROR_HPP
