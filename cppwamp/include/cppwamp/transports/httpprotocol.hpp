/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_HTTPPROTOCOL_HPP
#define CPPWAMP_TRANSPORTS_HTTPPROTOCOL_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains basic HTTP protocol definitions. */
//------------------------------------------------------------------------------

#include <string>
#include <system_error>
#include "../api.hpp"
#include "httpaction.hpp"
#include "socketendpoint.hpp"
#include "tcpprotocol.hpp"
#include "../utils/triemap.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Standard HTTP status codes. */
//------------------------------------------------------------------------------
enum class HttpStatus : unsigned
{
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


//------------------------------------------------------------------------------
/** Tag type associated with the HTTP transport. */
//------------------------------------------------------------------------------
struct CPPWAMP_API Http
{
    constexpr Http() = default;
};


//------------------------------------------------------------------------------
/** Contains HTTP host address information, as well as other socket options.
    Meets the requirements of @ref TransportSettings. */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpEndpoint
    : public SocketEndpoint<HttpEndpoint, Http, TcpOptions,
                            std::size_t, 16*1024*1024>
{
public:
    /// URI and status code of an error page.
    struct ErrorPage
    {
        std::string uri;
        HttpStatus status;
    };

    /// Transport protocol tag associated with these settings.
    using Protocol = Http;

    /// Numeric port type
    using Port = uint_least16_t;

    /** Constructor taking a port number. */
    explicit HttpEndpoint(Port port);

    /** Constructor taking an address string, port number. */
    HttpEndpoint(std::string address, unsigned short port);

    /** Adds an action associated with an exact route. */
    HttpEndpoint& addExactRoute(std::string uri, AnyHttpAction action);

    /** Adds an action associated with a prefix match route. */
    HttpEndpoint& addPrefixRoute(std::string uri, AnyHttpAction action);

    /** Specifies the custom agent string to use (default is
        Version::agentString). */
    HttpEndpoint& withAgent(std::string agent);

    /** Specifies the error page to show for the given HTTP response
        status code. */
    HttpEndpoint& withErrorPage(HttpStatus status, std::string uri);

    /** Specifies the error page to show for the given HTTP response
        status code, with the original status code substituted with the
        given status code. */
    HttpEndpoint& withErrorPage(HttpStatus status, std::string uri,
                                HttpStatus changedStatus);

    /** Obtains the custom agent string. */
    const std::string& agent() const;

    /** Generates a human-friendly string of the HTTP address/port. */
    std::string label() const;

    /** Finds the best matching action associated with the given route. */
    template <typename TStringLike>
    AnyHttpAction* findAction(const TStringLike& route) const
    {
        return doFindAction(route.data());
    }

    /** Finds the error page associated with the given HTTP status code. */
    const ErrorPage* findErrorPage(HttpStatus status) const;

private:
    using Base = SocketEndpoint<HttpEndpoint, Http, TcpOptions,
                                std::size_t, 16*1024*1024>;

    AnyHttpAction* doFindAction(const char* route);

    utils::TrieMap<AnyHttpAction> actionsByExactKey_;
    utils::TrieMap<AnyHttpAction> actionsByPrefixKey_;
    std::map<HttpStatus, ErrorPage> errorPages_;
    std::string agent_;
};

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
#include "../internal/httpprotocol.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_HTTPPROTOCOL_HPP
