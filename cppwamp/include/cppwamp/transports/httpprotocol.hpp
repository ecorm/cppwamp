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

#include <functional>
#include <string>
#include "../api.hpp"
#include "../transportlimits.hpp"
#include "../utils/triemap.hpp"
#include "httpstatus.hpp"
#include "socketendpoint.hpp"
#include "tcpprotocol.hpp"
#include "websocketprotocol.hpp"
#include "../internal/passkey.hpp"
#include "../internal/polymorphichttpaction.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Tag type associated with the HTTP transport. */
//------------------------------------------------------------------------------
struct CPPWAMP_API Http
{
    constexpr Http() = default;
};

class HttpEndpoint;


//------------------------------------------------------------------------------
/** Primary template for HTTP actions. */
//------------------------------------------------------------------------------
template <typename TOptions>
class HttpAction
{};


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

    /** Obtains the route associated with the action. */
    std::string route() const
    {
        return (action_ == nullptr) ? std::string{} : action_->route();
    }

private:
    std::shared_ptr<internal::PolymorphicHttpActionInterface> action_;

public: // Internal use only
    void initialize(internal::PassKey, const HttpEndpoint& settings)
    {
        action_->initialize(settings);
    }

    // Template needed to break circular dependency with HttpJob
    template <typename THttpJob>
    void expect(internal::PassKey, THttpJob& job)
    {
        assert(action_ != nullptr);
        action_->expect(job);
    }

    // Template needed to break circular dependency with HttpJob
    template <typename THttpJob>
    void execute(internal::PassKey, THttpJob& job)
    {
        assert(action_ != nullptr);
        action_->execute(job);
    };
};


//------------------------------------------------------------------------------
/** Contains limits for HTTP server transports.
    The size limits and timeouts inherited from WebsocketServerLimits apply
    only to Websocket transports resulting from an upgrade request. */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpServerLimits : public WebsocketServerLimits
{
public:
    HttpServerLimits& withHttpRequestHeaderSize(std::size_t n);

    HttpServerLimits& withHttpRequestBodySize(std::size_t n);

    HttpServerLimits& withHttpRequestBodyIncrement(std::size_t n);

    HttpServerLimits& withHttpResponseIncrement(std::size_t n);

    HttpServerLimits& withHttpRequestHeaderTimeout(Timeout t);

    HttpServerLimits& withHttpRequestBodyTimeout(IncrementalTimeout t);

    HttpServerLimits& withHttpResponseTimeout(IncrementalTimeout t);

    HttpServerLimits& withHttpKeepaliveTimeout(Timeout t);

    std::size_t httpRequestHeaderSize() const;

    std::size_t httpRequestBodySize() const;

    std::size_t httpRequestBodyIncrement() const;

    std::size_t httpResponseIncrement() const;

    Timeout httpRequestHeaderTimeout() const;

    const IncrementalTimeout& httpBodyTimeout() const;

    const IncrementalTimeout& httpResponseTimeout() const;

    Timeout httpKeepaliveTimeout() const;

    WebsocketServerLimits toWebsocket() const;

private:
    using Base = WebsocketServerLimits;

    Timeout requestHeaderTimeout_ = std::chrono::seconds(40);
        // Using Apache's maxinum RequestReadTimeout for headers

    IncrementalTimeout responseTimeout_ = {std::chrono::seconds{20}, 80*1024};
        // Using Apache's RequestReadTimeout, with 1/8 of ADSL2 5Mbps rate

    IncrementalTimeout requestBodyTimeout_ = {std::chrono::seconds{20}, 24*1024};
        // Using Apache's RequestReadTimeout, with ~1/4 of ADSL2 0.8Mbps rate

    Timeout keepaliveTimeout_ = std::chrono::seconds(75);
        // NGINX default: 75s, Apache default: 5s
        // Browser defaults: Firefox: 115s, IE: 60s, Chromium: never
        // Using NGINX's keepalive_timeout

    std::size_t requestBodySize_ = 1024*1024;
        // Default for Boost.Beast and NGINX

    std::size_t requestBodyIncrement_ = 4096; // Using Linux page size

    std::size_t responseIncrement_ = 4096; // Using Linux page size
};


//------------------------------------------------------------------------------
/** Contains HTTP error page information. */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpErrorPage
{
public:
    using Generator = std::function<std::string (HttpStatus status,
                                                 const std::string& what)>;

    HttpErrorPage();

    /** Specifies a file path or redirect URL to be associated with the given
        key HTTP status code, with the key status optionally substituted with
        the given status. */
    HttpErrorPage(HttpStatus key, std::string uri,
                  HttpStatus status = HttpStatus::none);

    /** Specifies a status code that substitutes the given key HTTP
        status code. */
    HttpErrorPage(HttpStatus key, HttpStatus status);

    /** Specifies a function that generates an HTML page for the given HTTP
        status code, with the key status optionally substituted with
        the given status. */
    HttpErrorPage(HttpStatus key, Generator generator,
                  HttpStatus status = HttpStatus::none);

    HttpErrorPage& withCharset(std::string charset);

    HttpStatus key() const;

    HttpStatus status() const;

    const std::string& uri() const;

    const std::string& charset() const;

    const Generator& generator() const;

    bool isRedirect() const;

private:
    std::string uri_;
    std::string charset_;
    Generator generator_;
    HttpStatus key_ = HttpStatus::none;
    HttpStatus status_ = HttpStatus::none;
};


//------------------------------------------------------------------------------
/** Function type used to associate MIME types with file extensions.
    It should return an empty string if the extension unknown. */
//------------------------------------------------------------------------------
using MimeTypeMapper = std::function<std::string (const std::string& ext)>;


//------------------------------------------------------------------------------
/** Common options for serving static files via HTTP. */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpFileServingOptions
{
public:
    using Parent = HttpFileServingOptions;

    static std::string defaultMimeType(const std::string& extension);

    /** Specifies the document root path for serving files. */
    HttpFileServingOptions& withDocumentRoot(std::string root);

    /** Specifies the charset to use when not specified by the MIME
        type mapper. */
    HttpFileServingOptions& withCharset(std::string charset);

    /** Specifies the index file name. */
    HttpFileServingOptions& withIndexFileName(std::string name);

    /** Enables automatic directory listing. */
    HttpFileServingOptions& withAutoIndex(bool enabled = true);

    /** Specifies the mapping function for determining MIME type based on
        file extension. */
    HttpFileServingOptions& withMimeTypes(MimeTypeMapper f);

    /** Obtains the document root path for serving files. */
    const std::string& documentRoot() const;

    /** Obtains the charset to use when not specified by the MIME
        type mapper. */
    const std::string& charset() const;

    /** Obtains the index filename. */
    const std::string& indexFileName() const;

    /** Determines if automatic directory listing is enabled. */
    bool autoIndex() const;

    /** Determines if a mapping function was specified. */
    bool hasMimeTypeMapper() const;

    /** Obtains the MIME type associated with the given file extension. */
    std::string lookupMimeType(const std::string& extension) const;

    /** Applies the given options as defaults for members that are not set. */
    void applyFallback(const HttpFileServingOptions& opts);

private:
    static char toLower(char c);

    std::string documentRoot_;
    std::string charset_;
    std::string indexFileName_;
    MimeTypeMapper mimeTypeMapper_;
    bool hasAutoIndex_ = false;
    bool autoIndex_ = false;
};


//------------------------------------------------------------------------------
/** Contains HTTP host address information, as well as other socket options.
    Meets the requirements of @ref TransportSettings. */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpEndpoint
    : public SocketEndpoint<HttpEndpoint, Http, TcpOptions, HttpServerLimits>
{
public:
    /// Transport protocol tag associated with these settings.
    using Protocol = Http;

    /// Numeric port type
    using Port = uint_least16_t;

    /** Constructor taking a port number. */
    explicit HttpEndpoint(Port port);

    /** Constructor taking an address string, port number. */
    HttpEndpoint(std::string address, unsigned short port);

    /** Specifies the default index file name. */
    HttpEndpoint& withIndexFileName(std::string name);

    /** Specifies the agent string to use for the HTTP response 'Server' field
        (default is Version::serverAgentString). */
    HttpEndpoint& withAgent(std::string agent);

    /** Specifies the default file serving options. */
    HttpEndpoint& withFileServingOptions(HttpFileServingOptions options);

    /** Specifies transport limits. */
    HttpEndpoint& withLimits(HttpServerLimits limits);

    /** Specifies the error page to show for the given HTTP response
        status code. */
    HttpEndpoint& addErrorPage(HttpErrorPage page);

    /** Adds an action associated with an exact route. */
    HttpEndpoint& addExactRoute(AnyHttpAction action);

    /** Adds an action associated with a prefix match route. */
    HttpEndpoint& addPrefixRoute(AnyHttpAction action);

    /** Obtains default file serving options. */
    const HttpFileServingOptions& fileServingOptions() const;

    /** Obtains the custom agent string. */
    const std::string& agent() const;

    /** Obtains the transport limits. */
    const HttpServerLimits& limits() const;

    /** Accesses the transport limits. */
    HttpServerLimits& limits();

    /** Generates a human-friendly string of the HTTP address/port. */
    std::string label() const;

    /** Finds the best matching action associated with the given target. */
    template <typename TStringLike>
    AnyHttpAction* findAction(const TStringLike& target)
    {
        return doFindAction(target.data());
    }

    /** Finds the error page associated with the given HTTP status code. */
    const HttpErrorPage* findErrorPage(HttpStatus status) const;

    /** Converts to settings for use in Websocket upgrade requests. */
    WebsocketEndpoint toWebsocket(WebsocketOptions options,
                                  WebsocketServerLimits limits) const
    {
        return WebsocketEndpoint{address(), port()}
            .withOptions(std::move(options))
            .withLimits(limits);
    }

private:
    using Base = SocketEndpoint<HttpEndpoint, Http, TcpOptions,
                                HttpServerLimits>;

    static const HttpFileServingOptions& defaultFileServingOptions();

    AnyHttpAction* doFindAction(const char* route);

    // TODO: HTTP "virtual" server names and aliases
    utils::TrieMap<AnyHttpAction> actionsByExactKey_;
    utils::TrieMap<AnyHttpAction> actionsByPrefixKey_;
    std::map<HttpStatus, HttpErrorPage> errorPages_;
    HttpFileServingOptions fileServingOptions_;
    std::string agent_;
    HttpServerLimits limits_;

public: // Internal use only
    void initialize(internal::PassKey)
    {
        for (auto& a: actionsByExactKey_)
            a.initialize({}, *this);
        for (auto& a: actionsByPrefixKey_)
            a.initialize({}, *this);
    }
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/httpprotocol.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_HTTPPROTOCOL_HPP
