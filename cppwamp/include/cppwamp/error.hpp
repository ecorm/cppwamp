/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
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

} // namespace error


//******************************************************************************
// Generic Error Codes
//******************************************************************************

//------------------------------------------------------------------------------
/** %Error code values used with the GenericCategory error category.
    The equivalencies to these codes are as follows:

    std::error_code                           | Equivalent condition value
    ----------------------------------------- | --------------------------
    make_error_code(WampErrc::systemShutdown) | sessionKilled
    make_error_code(WampErrc::closeRealm)     | sessionKilled
    make_error_code(WampErrc::sessionKilled)  | sessionKilled */
//------------------------------------------------------------------------------
enum class Errc
{
    success      = 0, ///< Operation successful
    abandoned    = 1, ///< Operation abandoned by this peer
    invalidState = 2, ///< Invalid state for this operation
    absent       = 3, ///< Item is absent
    badType      = 4, ///< Invalid or unexpected type
    count
};

//------------------------------------------------------------------------------
/** std::error_category used for reporting errors at the WAMP session layer.
    @see Errc */
//------------------------------------------------------------------------------
class CPPWAMP_API GenericCategory : public std::error_category
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
    CPPWAMP_HIDDEN GenericCategory();

    friend GenericCategory& genericCategory();
};

//------------------------------------------------------------------------------
/** Obtains a reference to the static error category object for Generic errors.
    @relates GenericCategory */
//------------------------------------------------------------------------------
CPPWAMP_API GenericCategory& genericCategory();

//------------------------------------------------------------------------------
/** Creates an error code value from an Errc enumerator.
    @relates GenericCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_code make_error_code(Errc errc);

//------------------------------------------------------------------------------
/** Creates an error condition value from an Errc enumerator.
    @relates GenericCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_condition make_error_condition(Errc errc);

//------------------------------------------------------------------------------
/** Converts an error code to a string containing the category and number. */
//-----------------------------------------------------------------------------
CPPWAMP_API std::string briefErrorCodeString(std::error_code ec);

//------------------------------------------------------------------------------
/** Converts an error to a string containing the category, number, and
    associated message. */
//-----------------------------------------------------------------------------
CPPWAMP_API std::string detailedErrorCodeString(std::error_code ec);


//******************************************************************************
// WAMP Protocol Error Codes
//******************************************************************************

// TODO: https://github.com/wamp-proto/wamp-proto/pull/444

//------------------------------------------------------------------------------
/** %Error code values used with the WampCategory error category.
    The equivalencies to these codes are as follows:

    std::error_code                                 | Equivalent condition value
    ----------------------------------------------- | --------------------------
    make_error_code(WampErrc::systemShutdown)       | sessionKilled
    make_error_code(WampErrc::closeRealm)           | sessionKilled
    make_error_code(WampErrc::timeout)              | cancelled
    make_error_code(WampErrc::discloseMeDisallowed) | optionNotAllowed */
//------------------------------------------------------------------------------
enum class WampErrc
{
    success = 0,            ///< Operation successful
    unknown,                ///< Unknown error URI

    // Session close reasons
    closeRealm,             ///< The other peer is leaving the realm
    goodbyeAndOut,          ///< Session ended successfully
    sessionKilled,          ///< Session was killed by the other peer
    systemShutdown,         ///< The other peer is shutting down

    // Basic profile errors
    authorizationFailed,        ///< The authorization operation itself failed
    invalidArgument,            ///< The given argument types/values are not acceptable to the called procedure
    invalidUri,                 ///< An invalid WAMP URI was provided
    noSuchProcedure,            ///< No procedure was registered under the given URI
    noSuchRealm,                ///< Attempt to join non-existent realm
    noSuchRegistration,         ///< Could not unregister; the given registration is not active
    noSuchRole,                 ///< Attempt to authenticate under unsupported role
    noSuchSubscription,         ///< Could not unsubscribe; the given subscription is not active
    notAuthorized,              ///< This peer is not authorized to perform the operation
    procedureAlreadyExists,     ///< A procedure with the given URI is already registered
    protocolViolation,          ///< Invalid, unexpected, or malformed WAMP message.

    // Advanced profile errors
    cancelled,                  ///< The previously issued call was cancelled
    featureNotSupported,        ///< Advanced feature is not supported
    discloseMeDisallowed,       ///< Router rejected client request to disclose its identity
    optionNotAllowed,           ///< Option is disallowed by the router
    networkFailure,             ///< Router encountered a network failure
    noAvailableCallee,          ///< All registered callees are unable to handle the invocation
    timeout,                    ///< Operation timed out
    unavailable,                ///< Callee is unable to handle the invocation

    // Errors not currently in the WAMP spec, but used by CppWAMP and Crossbar
    // https://github.com/crossbario/autobahn-python/blob/master/autobahn/wamp/exception.py
    authenticationFailed,       ///< The authentication operation itself failed
    payloadSizeExceeded,        ///< Serialized payload exceeds transport limits
    count
};

//------------------------------------------------------------------------------
/** std::error_category used for reporting errors at the WAMP session layer.
    @see WampErrc */
//------------------------------------------------------------------------------
class CPPWAMP_API WampCategory : public std::error_category
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
    CPPWAMP_HIDDEN WampCategory();

    friend WampCategory& wampCategory();
};

//------------------------------------------------------------------------------
/** Obtains a reference to the static error category object for Wamp errors.
    @relates WampCategory */
//------------------------------------------------------------------------------
CPPWAMP_API WampCategory& wampCategory();

//------------------------------------------------------------------------------
/** Creates an error code value from an WampErrc enumerator.
    @relates WampCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_code make_error_code(WampErrc errc);

//------------------------------------------------------------------------------
/** Creates an error condition value from an WampErrc enumerator.
    @relates WampCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_condition make_error_condition(WampErrc errc);

//------------------------------------------------------------------------------
/** Looks up the WampErrc enumerator that corresponds to the given error URI.
    @relates WampCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API WampErrc errorUriToCode(const std::string& uri);

//------------------------------------------------------------------------------
/** Obtains the error URI corresponding to the given error code belonging
    to WampCategory.
    @relates WampCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API const std::string& errorCodeToUri(WampErrc errc);

//------------------------------------------------------------------------------
/** Generates an error URI corresponding to the given error code.
    @relates WampCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::string errorCodeToUri(std::error_code ec);


//******************************************************************************
// Codec decoding Error Codes
//******************************************************************************

//------------------------------------------------------------------------------
/** %Error code values used with the DecodingCategory error category.
    All of the following non-zero codes are equivalent to the
    DecodingErrc::failed condition:
    - Non-zero wamp::DecodingErrc
    - `jsoncons::json_errc`
    - `jsoncons::cbor::cbor_errc`
    - `jsoncons::msgpack::msgpack_errc` */
//------------------------------------------------------------------------------
enum class DecodingErrc
{
    success           = 0, ///< Decoding succesful
    failed            = 1, ///< Decoding failed
    emptyInput        = 2, ///< Input is empty or has no tokens
    expectedStringKey = 3, ///< Expected a string key
    badBase64Length   = 4, ///< Invalid Base64 string length
    badBase64Padding  = 5, ///< Invalid Base64 padding
    badBase64Char     = 6, ///< Invalid Base64 character
    count
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
// Transport Error Codes
//******************************************************************************

//------------------------------------------------------------------------------
/** %Error code values used with the TransportCategory error category.

    Codes equivalent to the TransportErrc::aborted condition are
    - `std::errc::operation_cancelled`
    - `boost::asio::error::operation_aborted`

    Codes equivalent to the TransportErrc::failed condition are
    - Any `TransportErrc` code greater than `failed`
    - any non-zero code of the `std::generic_catetory`
    - any non-zero code of the `std::system_catetory`
    - any non-zero code of the `boost::system::generic_catetory`
    - any non-zero code of the `boost::system::system_catetory`
    - any non-zero code of the `boost::asio::error::get_addrinfo_category`
    - any non-zero code of the `boost::asio::error::get_misc_category`
    - any non-zero code of the `boost::asio::error::get_netdb_category`

    Codes equivalent to the TransportErrc::disconnected are
    - `std::errc::connection_reset`
    - `boost::asio::error::connection_reset`
    - `boost::asio::error::eof` */
//------------------------------------------------------------------------------
enum class TransportErrc
{
    success        = 0,  ///< Transport operation successful
    aborted        = 1,  ///< Transport operation aborted
    disconnected   = 2,  ///< Transport disconnected by other peer
    failed         = 3,  ///< Transport operation failed
    exhausted      = 4,  ///< All transports failed during connection
    tooLong        = 5,  ///< Incoming message exceeds transport's length limit
    badHandshake   = 6,  ///< Received invalid handshake
    badCommand     = 7,  ///< Received invalid transport command
    badSerializer  = 8,  ///< Unsupported serialization format
    badLengthLimit = 9,  ///< Unacceptable maximum message length
    badFeature     = 10, ///< Unsupported transport feature
    saturated      = 11, ///< Connection limit reached
    count
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

} // namespace wamp


//------------------------------------------------------------------------------
#if !defined CPPWAMP_FOR_DOXYGEN
namespace std
{

template <>
struct CPPWAMP_API is_error_condition_enum<wamp::Errc> : public true_type
{};

template <>
struct CPPWAMP_API is_error_condition_enum<wamp::WampErrc> : public true_type
{};

template <>
struct CPPWAMP_API is_error_condition_enum<wamp::DecodingErrc>
    : public true_type
{};

template <>
struct CPPWAMP_API is_error_condition_enum<wamp::TransportErrc>
    : public true_type
{};

} // namespace std
#endif // !defined CPPWAMP_FOR_DOXYGEN


#ifndef CPPWAMP_COMPILED_LIB
#include "internal/error.ipp"
#endif

#endif // CPPWAMP_ERROR_HPP
