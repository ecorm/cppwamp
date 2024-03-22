/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_HTTPSERVER_HPP
#define CPPWAMP_TRANSPORTS_HTTPSERVER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for establishing HTTP server transports
           and services. */
//------------------------------------------------------------------------------

#include <memory>
#include "../api.hpp"
#include "../asiodefs.hpp"
#include "../listener.hpp"
#include "httpprotocol.hpp"
#include "httpserveroptions.hpp"
#include "websocketprotocol.hpp"

// TODO: HTTP compression
// TODO: HTTP range requests
// TODO: HTTP caching: If-Modified-Since and Last-Modified

namespace wamp
{

//------------------------------------------------------------------------------
/** Options for serving static files via HTTP. */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpServeFiles
{
public:
    /** Constructor. */
    explicit HttpServeFiles(std::string route);

    /** Specifies that the route portion of the target path should be
        substituted with the given alias. */
    HttpServeFiles& withAlias(std::string alias);

    /** Specifies the file serving options. */
    HttpServeFiles& withOptions(HttpFileServingOptions options);

    /** Obtains the route associated with this action. */
    const std::string& route() const;

    /** Determines if aliasing is enabled. */
    bool hasAlias() const;

    /** Obtains the alias path. */
    const std::string& alias() const;

    /** Obtains the file serving options. */
    const HttpFileServingOptions& options() const;

    /** Applies the given options as default for members that are not set. */
    void mergeOptions(const HttpFileServingOptions& fallback);

private:
    std::string route_;
    std::string alias_;
    HttpFileServingOptions options_;
    bool hasAlias_ = false;
};


//------------------------------------------------------------------------------
/** Options for redirecting an HTTP request. */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpRedirect
{
public:
    /** Constructor. */
    explicit HttpRedirect(std::string route);

    /** Specifies the scheme to use in the redirect location. */
    HttpRedirect& withScheme(std::string scheme);

    /** Specifies the authority to use in the redirect location. */
    HttpRedirect& withAuthority(std::string authority);

    /** Specifies that the route portion of the target path should be
        substituted with the given alias. */
    HttpRedirect& withAlias(std::string alias);

    /** Specifies the redirect status code. */
    HttpRedirect& withStatus(HttpStatus status);

    /** Obtains the route associated with this action. */
    const std::string& route() const;

    /** Determines if aliasing is enabled. */
    bool hasAlias() const;

    /** Obtains the scheme to use in the redirect location. */
    const std::string& scheme() const;

    /** Obtains the the authority to use in the redirect location. */
    const std::string& authority() const;

    /** Obtains the alias path. */
    const std::string& alias() const;

    /** Obtains the file serving options. */
    HttpStatus status() const;

private:
    std::string route_;
    std::string authority_;
    std::string scheme_;
    std::string alias_;
    HttpStatus status_;
    bool hasAlias_ = false;
};


//------------------------------------------------------------------------------
/** Options for upgrading an HTTP request to a Websocket connection. */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpWebsocketUpgrade
{
public:
    /** Constructor. */
    explicit HttpWebsocketUpgrade(std::string route);

    /** Specifies the Websocket options. */
    HttpWebsocketUpgrade& withOptions(WebsocketOptions options);

    /** Specifies the Websocket server limits. */
    HttpWebsocketUpgrade& withLimits(WebsocketServerLimits limits);

    /** Obtains the route associated with this action. */
    const std::string& route() const;

    /** Obtains the websocket options. */
    const WebsocketOptions options() const;

    /** Obtains the specified maximum incoming message length. */
    const WebsocketServerLimits& limits() const;

private:
    std::string route_;
    WebsocketOptions options_;
    WebsocketServerLimits limits_;
};


// Forward declarations
namespace internal {class HttpListener;}

//------------------------------------------------------------------------------
/** Listener specialization that implements an HTTP server.
    Users should not use this class directly and should instead pass
    wamp::HttpEndpoint to wamp::Router::openServer via wamp::ServerOptions. */
//------------------------------------------------------------------------------
template <>
class CPPWAMP_API Listener<Http> : public Listening
{
public:
    /** Type containing the transport settings. */
    using Settings = HttpEndpoint;

    /** Constructor. */
    Listener(AnyIoExecutor e, IoStrand i, Settings s, CodecIdSet c,
             RouterLogger::Ptr l = {});

    /** Destructor. */
    ~Listener() override;

    void observe(Handler handler) override;

    void establish() override;

    ErrorOr<Transporting::Ptr> take() override;

    void drop() override;

    void cancel() override;

    /** @name Non-copyable and non-movable */
    /// @{
    Listener(const Listener&) = delete;
    Listener(Listener&&) = delete;
    Listener& operator=(const Listener&) = delete;
    Listener& operator=(Listener&&) = delete;
    /// @}

private:
    std::shared_ptr<internal::HttpListener> impl_;
};


namespace internal { class HttpServeFilesImpl; }

//------------------------------------------------------------------------------
template <>
class HttpAction<HttpServeFiles>
{
public:
    explicit HttpAction(HttpServeFiles properties);

    ~HttpAction();

    std::string route() const;

    void initialize(const HttpServerOptions& options);

    void expect(HttpJob& job);

    void execute(HttpJob& job);

private:
    std::unique_ptr<internal::HttpServeFilesImpl> impl_;
};

//------------------------------------------------------------------------------
template <>
class HttpAction<HttpRedirect>
{
public:
    explicit HttpAction(HttpRedirect properties);

    std::string route() const;

    void initialize(const HttpServerOptions& options);

    void expect(HttpJob& job);

    void execute(HttpJob& job);

private:
    HttpRedirect properties_;
};

//------------------------------------------------------------------------------
template <>
class HttpAction<HttpWebsocketUpgrade>
{
public:
    HttpAction(HttpWebsocketUpgrade properties);

    std::string route() const;

    void initialize(const HttpServerOptions& options);

    void expect(HttpJob& job);

    void execute(HttpJob& job);

private:
    bool checkRequest(HttpJob& job);

    HttpWebsocketUpgrade properties_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/httpserver.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_HTTPSERVER_HPP
