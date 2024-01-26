/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023-2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_HTTPSTATUS_HPP
#define CPPWAMP_TRANSPORTS_HTTPSTATUS_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains basic HTTP protocol definitions. */
//------------------------------------------------------------------------------

#include <system_error>
#include "../api.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Standard HTTP status codes. */
//------------------------------------------------------------------------------
enum class HttpStatus : unsigned
{
    none                          = 0, // Non-standard, used internally

    continueRequest               = 100,
    switchingProtocols            = 101,
    processing                    = 102,

    ok                            = 200,
    created                       = 201,
    accepted                      = 202,
    nonAuthoritativeInformation   = 203,
    noContent                     = 204,
    resetContent                  = 205,
    partialContent                = 206,
    multiStatus                   = 207,
    alreadyReported               = 208,
    imUsed                        = 226,

    multipleChoices               = 300,
    movedPermanently              = 301,
    found                         = 302,
    seeOther                      = 303,
    notModified                   = 304,
    useProxy                      = 305,
    temporaryRedirect             = 307,
    permanentRedirect             = 308,

    badRequest                    = 400,
    unauthorized                  = 401,
    paymentRequired               = 402,
    forbidden                     = 403,
    notFound                      = 404,
    methodNotAllowed              = 405,
    notAcceptable                 = 406,
    proxyAuthenticationRequired   = 407,
    requestTimeout                = 408,
    conflict                      = 409,
    gone                          = 410,
    lengthRequired                = 411,
    preconditionFailed            = 412,
    payloadTooLarge               = 413,
    uriTooLong                    = 414,
    unsupportedMediaType          = 415,
    rangeNotSatisfiable           = 416,
    expectationFailed             = 417,
    misdirectedRequest            = 421,
    unprocessableEntity           = 422,
    locked                        = 423,
    failedDependency              = 424,
    upgradeRequired               = 426,
    preconditionRequired          = 428,
    tooManyRequests               = 429,
    requestHeaderFieldsTooLarge   = 431,
    unavailableForLegalReasons    = 451,

    internalServerError           = 500,
    notImplemented                = 501,
    badGateway                    = 502,
    serviceUnavailable            = 503,
    gatewayTimeout                = 504,
    httpVersionNotSupported       = 505,
    variantAlsoNegotiates         = 506,
    insufficientStorage           = 507,
    loopDetected                  = 508,
    notExtended                   = 510,
    networkAuthenticationRequired = 511
};

//------------------------------------------------------------------------------
/** std::error_category used for reporting Websocket close reasons.
    @see WebsocketCloseErrc */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpStatusCategory : public std::error_category
{
public:
    /** Obtains the name of the category. */
    const char* name() const noexcept override;

    /** Obtains the explanatory string. */
    std::string message(int ev) const override;

    /** Compares `error_code` and and error condition for equivalence. */
    bool equivalent(const std::error_code& code,
                    int condition) const noexcept override;

private:
    CPPWAMP_HIDDEN HttpStatusCategory();

    friend HttpStatusCategory& httpStatusCategory();
};

//------------------------------------------------------------------------------
/** Obtains a reference to the static error category object for Websocket
    close reasons.
    @relates HttpStatusCategory */
//------------------------------------------------------------------------------
CPPWAMP_API HttpStatusCategory& httpStatusCategory();

//------------------------------------------------------------------------------
/** Creates an error code value from a WebsocketCloseErrc enumerator.
    @relates TransportCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_code make_error_code(HttpStatus errc);

//------------------------------------------------------------------------------
/** Creates an error condition value from a WebsocketCloseErrc enumerator.
    @relates TransportCategory */
//-----------------------------------------------------------------------------
CPPWAMP_API std::error_condition make_error_condition(HttpStatus errc);

} // namespace wamp


//------------------------------------------------------------------------------
#if !defined CPPWAMP_FOR_DOXYGEN
namespace std
{

template <>
struct CPPWAMP_API is_error_condition_enum<wamp::HttpStatus>
    : public true_type
{};

} // namespace std
#endif // CPPWAMP_FOR_DOXYGEN

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/httpstatus.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_HTTPSTATUS_HPP
