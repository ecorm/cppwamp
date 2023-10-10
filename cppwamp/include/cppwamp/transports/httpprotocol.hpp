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
#include "socketendpoint.hpp"
#include "tcpprotocol.hpp"
#include "../utils/triemap.hpp"
#include "../internal/polymorphichttpaction.hpp"

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


namespace internal { class HttpJob; }

//------------------------------------------------------------------------------
/** Wrapper that type-erases a polymorphic HTTP action. */
//------------------------------------------------------------------------------
class CPPWAMP_API AnyHttpAction
{
public:
    /** Constructs an empty AnyHttpAction. */
    AnyHttpAction() = default;

    /** Converting constructor taking action options. */
    template <typename TOptions>
    AnyHttpAction(TOptions o) // NOLINT(google-explicit-constructor)
        : action_(std::make_shared<internal::PolymorphicHttpAction<TOptions>>(
            std::move(o)))
    {}

    /** Returns false if the AnyHttpAction is empty. */
    explicit operator bool() const {return action_ != nullptr;}

    template <typename OptionsType>
    bool is() const
    {
        using Derived = internal::PolymorphicHttpAction<OptionsType>;
        return std::dynamic_pointer_cast<Derived>(action_) != nullptr;
    }

private:
    // Template needed to break circular dependency with HttpJob
    template <typename THttpJob>
    void execute(THttpJob& job)
    {
        assert(action_ != nullptr);
        action_->execute(job);
    };

    std::shared_ptr<internal::PolymorphicHttpActionInterface> action_;

    friend class internal::HttpJob;
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
    // TODO: Custom error page generator

    /// URI and status code of an error page.
    struct ErrorPage
    {
        bool isRedirect() const {return static_cast<unsigned>(status) < 400;}

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

    /** Specifies the default document root path for serving files. */
    HttpEndpoint& withDocumentRoot(std::string root);

    /** Specifies the custom agent string to use (default is
        Version::agentString). */
    HttpEndpoint& withAgent(std::string agent);

    /** Specifies the error page to show for the given HTTP response
        status code. */
    HttpEndpoint& withErrorPage(HttpStatus status, std::string uri);

    /** Specifies the status code that substitutes the given HTTP response
        status code. */
    HttpEndpoint& withErrorPage(HttpStatus status, HttpStatus newStatus);

    /** Specifies the error page to show for the given HTTP response
        status code, with the original status code substituted with the
        given status code. */
    HttpEndpoint& withErrorPage(HttpStatus status, std::string uri,
                                HttpStatus newStatus);

    /** Specifies the total header size limit of HTTP requests. */
    HttpEndpoint& withHeaderLimit(uint32_t limit);

    /** Specifies the total body size limit of HTTP requests. */
    HttpEndpoint& withBodyLimit(uint32_t limit);

    /** Obtains the default document root path for serving files. */
    const std::string& documentRoot() const;

    /** Obtains the custom agent string, or Version::agentString() if none
        was specified. */
    const std::string& agent() const;

    /** Obtains the total header size limit of HTTP requests. */
    uint32_t headerLimit() const;

    /** Obtains the total body size limit of HTTP requests. */
    uint32_t bodyLimit() const;

    /** Generates a human-friendly string of the HTTP address/port. */
    std::string label() const;

    /** Finds the best matching action associated with the given route. */
    template <typename TStringLike>
    const AnyHttpAction* findAction(const TStringLike& route) const
    {
        return doFindAction(route.data());
    }

    /** Finds the error page associated with the given HTTP status code. */
    const ErrorPage* findErrorPage(HttpStatus status) const;

private:
    using Base = SocketEndpoint<HttpEndpoint, Http, TcpOptions,
                                std::size_t, 16*1024*1024>;

    const AnyHttpAction* doFindAction(const char* route) const;

    void setErrorPage(HttpStatus status, std::string uri, HttpStatus newStatus);

    utils::TrieMap<AnyHttpAction> actionsByExactKey_;
    utils::TrieMap<AnyHttpAction> actionsByPrefixKey_;
    std::map<HttpStatus, ErrorPage> errorPages_;
#ifdef _WIN32
    std::string documentRoot_ = "C:/web/html";
#else
    std::string documentRoot_ = "/var/wwww/html";
#endif
    std::string agent_;
    uint32_t headerLimit_ = 0;
    uint32_t bodyLimit_ = 0;
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
