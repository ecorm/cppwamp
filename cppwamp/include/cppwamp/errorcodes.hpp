/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ERRORCODES_HPP
#define CPPWAMP_ERRORCODES_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Provides error codes and their categories. */
//------------------------------------------------------------------------------

#include <string>
#include <system_error>
#include "api.hpp"

namespace wamp
{

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

// TODO: Deprecate wamp.close.goodbye_and_out?
// https://github.com/wamp-proto/wamp-proto/issues/484

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
    success                =  0, ///< Operation successful
    unknown                =  1, ///< Unknown error URI

    // Session close reasons
    closeRealm             =  2, ///< Session close initiated
    goodbyeAndOut          =  3, ///< Session closed normally
    sessionKilled          =  4, ///< Session was killed by the router
    closedNormally         =  5, ///< Session closed normally
    systemShutdown         =  6, ///< Session closing due to imminent shutdown

    // Basic profile errors
    invalidArgument        =  7, ///< The procedure rejected the argument types/values
    invalidUri             =  8, ///< An invalid WAMP URI was provided
    noSuchPrincipal        =  9, ///< Authentication attempted with a non-existent authid
    noSuchProcedure        = 10, ///< No procedure was registered under the given URI
    noSuchRealm            = 11, ///< No realm exists with the given URI
    noSuchRegistration     = 12, ///< No registration exists with the given ID
    noSuchRole             = 13, ///< Attempt to authenticate under unsupported role
    noSuchSubscription     = 14, ///< No subscription exists with the given ID
    payloadSizeExceeded    = 15, ///< Serialized payload exceeds transport size limits
    procedureAlreadyExists = 16, ///< A procedure with the given URI is already registered
    protocolViolation      = 17, ///< Invalid, unexpected, or malformed WAMP message

    // Advanced profile errors
    authenticationDenied   = 18, ///< Authentication was denied
    authenticationFailed   = 19, ///< The authentication operation itself failed
    authenticationRequired = 20, ///< Anonymous authentication not permitted
    authorizationDenied    = 21, ///< Not authorized to perform the action
    authorizationFailed    = 22, ///< The authorization operation itself failed
    authorizationRequired  = 23, ///< Authorization information was missing
    cancelled              = 24, ///< The previously issued call was cancelled
    featureNotSupported    = 25, ///< Advanced feature is not supported
    discloseMeDisallowed   = 26, ///< Client request to disclose its identity was rejected
    optionNotAllowed       = 27, ///< Option is disallowed by the router
    networkFailure         = 28, ///< Router encountered a network failure
    noAvailableCallee      = 29, ///< No available registered callee to handle the invocation
    noMatchingAuthMethod   = 30, ///< No matching authentication method was found
    noSuchSession          = 31, ///< No session exists with the given ID
    timeout                = 32, ///< Operation timed out
    unavailable            = 33, ///< Callee is unable to handle the invocation

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


//******************************************************************************
// Miscellaneous Error Codes
//******************************************************************************

//------------------------------------------------------------------------------
/** %Error code values used with the MiscCategory error category. */
//------------------------------------------------------------------------------
enum class MiscErrc
{
    success       = 0, ///< Operation successful
    abandoned     = 1, ///< Operation abandoned by this peer
    invalidState  = 2, ///< Invalid state for this operation
    absent        = 3, ///< Item is absent
    alreadyExists = 4, ///< Item already exists
    badType       = 5, ///< Invalid or unexpected type
    noSuchTopic   = 6, ///< No subscription under the given topic URI
    count
};

//------------------------------------------------------------------------------
/** std::error_category used for reporting miscellanous errors not belonging
    to another category.
    @see MiscErrc */
//------------------------------------------------------------------------------
class CPPWAMP_API MiscCategory : public std::error_category
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
    CPPWAMP_HIDDEN MiscCategory();

    friend MiscCategory& genericCategory();
};

//------------------------------------------------------------------------------
/** Obtains a reference to the static error category object for Generic errors.
    @relates MiscCategory */
//------------------------------------------------------------------------------
CPPWAMP_API MiscCategory& genericCategory();

//------------------------------------------------------------------------------
/** Creates an error code value from an MiscErrc enumerator.
    @relates MiscCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_code make_error_code(MiscErrc errc);

//------------------------------------------------------------------------------
/** Creates an error condition value from an MiscErrc enumerator.
    @relates MiscCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_condition make_error_condition(MiscErrc errc);

} // namespace wamp


//------------------------------------------------------------------------------
#if !defined CPPWAMP_FOR_DOXYGEN
namespace std
{

template <>
struct CPPWAMP_API is_error_condition_enum<wamp::WampErrc>
    : public true_type
{};

template <>
struct CPPWAMP_API is_error_condition_enum<wamp::DecodingErrc>
    : public true_type
{};

template <>
struct CPPWAMP_API is_error_condition_enum<wamp::TransportErrc>
    : public true_type
{};

template <>
struct CPPWAMP_API is_error_condition_enum<wamp::MiscErrc>
    : public true_type
{};

} // namespace std
#endif // !defined CPPWAMP_FOR_DOXYGEN


#ifndef CPPWAMP_COMPILED_LIB
#include "internal/errorcodes.ipp"
#endif

#endif // CPPWAMP_ERRORCODES_HPP
