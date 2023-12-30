/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_HTTPSERVER_HPP
#define CPPWAMP_TRANSPORTS_HTTPSERVER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for establishing HTTP transports. */
//------------------------------------------------------------------------------

#include <memory>
#include "../api.hpp"
#include "../asiodefs.hpp"
#include "../listener.hpp"
#include "httpprotocol.hpp"

// TODO: HTTPS
// TODO: HTTP compression
// TODO: HTTP chunked transfer
// TODO: HTTP multi-part chunked transfer

namespace wamp
{

namespace internal { class HttpJob; }

//------------------------------------------------------------------------------
/** Options for serving static files via HTTP. */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpServeStaticFiles
{
public:
    /** Constructor. */
    explicit HttpServeStaticFiles(std::string route);

    /** Specifies that the route portion of the target path should be
        substituted with the given alias. */
    HttpServeStaticFiles& withAlias(std::string alias);

    /** Specifies the file serving options. */
    HttpServeStaticFiles& withOptions(HttpFileServingOptions options);

    /** Obtains the route associated with this action. */
    const std::string& route() const;

    /** Determines if aliasing is enabled. */
    bool hasAlias() const;

    /** Obtains the alias path. */
    const std::string& alias() const;

    /** Obtains the file serving options. */
    const HttpFileServingOptions& options() const;

    /** Applies the given options as default for members that are not set. */
    void applyFallbackOptions(const HttpFileServingOptions& fallback);

private:
    std::string route_;
    std::string alias_;
    HttpFileServingOptions options_;
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
/** Listener specialization that implememts an HTTP server.
    Users do not  to use this class directly and should instead pass
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

    Transporting::Ptr take() override;

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


namespace internal
{

class HttpServeStaticFilesImpl;

//------------------------------------------------------------------------------
template <>
class HttpAction<HttpServeStaticFiles>
{
public:
    explicit HttpAction(HttpServeStaticFiles properties);

    ~HttpAction();

    std::string route() const;

    void initialize(const HttpEndpoint& settings);

    void execute(HttpJob& job);

private:
    std::unique_ptr<HttpServeStaticFilesImpl> impl_;
};

//------------------------------------------------------------------------------
template <>
class HttpAction<HttpWebsocketUpgrade>
{
public:
    HttpAction(HttpWebsocketUpgrade properties);

    std::string route() const;

    void initialize(const HttpEndpoint& settings);

    void execute(HttpJob& job);

private:
    HttpWebsocketUpgrade properties_;
};

} // namespace internal

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/httpserver.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_HTTPSERVER_HPP
