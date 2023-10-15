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
#include <set>
#include <utility>
#include "../api.hpp"
#include "../asiodefs.hpp"
#include "../listener.hpp"
#include "httpprotocol.hpp"

// TODO: HTTPS

namespace wamp
{

namespace internal { class HttpJob; }

//------------------------------------------------------------------------------
/** Options for serving static files via HTTP. */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpServeStaticFiles
{
public:
    using MimeTypeMapper = std::function<std::string (const std::string&)>;

    /** Constructor. */
    explicit HttpServeStaticFiles(std::string route);

    /** Specifies the root path. */
    HttpServeStaticFiles& withRoot(std::string path);

    /** Specifies the alias path. */
    HttpServeStaticFiles& withAlias(std::string path);

    /** Specifies the index file name. */
    HttpServeStaticFiles& withIndexFileName(std::string name);

    /** Specifies the mapping function for determining MIME type based on
        file extension. */
    HttpServeStaticFiles& withMimeTypes(MimeTypeMapper f);

    /** Obtains the route associated with this action. */
    const std::string& route() const;

    /** Determines if the path corresponds to a root or alias. */
    bool pathIsAlias() const;

    /** Obtains the root or alias path. */
    const std::string& path() const;

    /** Obtains the index file name. */
    const std::string& indexFileName() const;

    // TODO: Automatic directory listing (aka 'autoindex')

    /** Obtains the MIME type associated with the given path. */
    std::string lookupMimeType(std::string extension) const;

private:
    static char toLower(char c);

    std::string defaultMimeType(const std::string& extension) const;

    std::string route_;
    std::string path_;
    std::string indexFileName_;
    MimeTypeMapper mimeTypeMapper_;
    bool pathIsAlias_ = false;
};


//------------------------------------------------------------------------------
/** Options for upgrading an HTTP request to a Websocket connection. */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpWebsocketUpgrade
{
public:
    /** Constructor. */
    explicit HttpWebsocketUpgrade(std::string route);

    /** Specifies the maximum length permitted for incoming messages. */
    HttpWebsocketUpgrade& withMaxRxLength(std::size_t length);

    /** Obtains the route associated with this action. */
    const std::string& route() const;

    /** Obtains the specified maximum incoming message length. */
    std::size_t maxRxLength() const;

private:
    std::string route_;
    std::size_t maxRxLength_ = 16*1024*1024;
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

//------------------------------------------------------------------------------
template <>
class HttpAction<HttpServeStaticFiles>
{
public:
    explicit HttpAction(HttpServeStaticFiles options);

    std::string route() const;

    void execute(HttpJob& job);

private:
    bool checkRequest(HttpJob& job) const;

    std::string buildPath(const HttpEndpoint& settings,
                          const std::string& target) const;

    template <typename TBody>
    bool openFile(HttpJob& job, const std::string& path, TBody& fileBody) const;

    std::string lookupMimeType(const std::string& path) const;

    HttpServeStaticFiles options_;
};

//------------------------------------------------------------------------------
template <>
class HttpAction<HttpWebsocketUpgrade>
{
public:
    HttpAction(HttpWebsocketUpgrade options);

    std::string route() const;

    void execute(HttpJob& job);

private:
    HttpWebsocketUpgrade options_;
};

} // namespace internal

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/httpserver.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_HTTPSERVER_HPP
