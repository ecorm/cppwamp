/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/httpserver.hpp"
#include "httplistener.hpp"

namespace wamp
{

//******************************************************************************
// HttpServeStaticFiles
//******************************************************************************

CPPWAMP_INLINE HttpServeStaticFiles::HttpServeStaticFiles(std::string route)
    : route_(std::move(route))
{}

/** @post `this->path() == path`
    @post `this->pathIsAlias() == false` */
CPPWAMP_INLINE HttpServeStaticFiles&
HttpServeStaticFiles::withRoot(std::string path)
{
    path_ = std::move(path);
    pathIsAlias_ = false;
    return *this;
}

/** @post `this->path() == path`
    @post `this->pathIsAlias() == true` */
CPPWAMP_INLINE HttpServeStaticFiles&
HttpServeStaticFiles::withAlias(std::string path)
{
    path_ = std::move(path);
    pathIsAlias_ = true;
    return *this;
}

CPPWAMP_INLINE HttpServeStaticFiles&
HttpServeStaticFiles::withIndexFileName(std::string name)
{
    indexFileName_ = std::move(name);
    return *this;
}

CPPWAMP_INLINE HttpServeStaticFiles&
HttpServeStaticFiles::withMimeTypes(MimeTypeMapper f)
{
    mimeTypeMapper_ = std::move(f);
    return *this;
}

CPPWAMP_INLINE const std::string& HttpServeStaticFiles::route() const
{
    return route_;
}

CPPWAMP_INLINE bool HttpServeStaticFiles::pathIsAlias() const
{
    return pathIsAlias_;
}

/** If empty, HttpEndpoint::documentRoot is used. */
CPPWAMP_INLINE const std::string& HttpServeStaticFiles::path() const
{
    return path_;
}

/** If empty, HttpEndpoint::indexFileName is used. */
CPPWAMP_INLINE const std::string& HttpServeStaticFiles::indexFileName() const
{
    return indexFileName_;
}

CPPWAMP_INLINE char HttpServeStaticFiles::toLower(char c)
{
    static constexpr unsigned offset = 'a' - 'A';
    if (c >= 'A' && c <= 'Z')
        c += offset;
    return c;
}

CPPWAMP_INLINE std::string
HttpServeStaticFiles::lookupMimeType(std::string extension) const
{
    for (auto& c: extension)
        c = toLower(c);
    return !mimeTypeMapper_ ? defaultMimeType(extension)
                            : mimeTypeMapper_(extension);
}

CPPWAMP_INLINE std::string
HttpServeStaticFiles::defaultMimeType(const std::string& extension) const
{
    static const std::map<std::string, std::string> table =
    {
        {"bmp",  "image/bmp"},
        {"css",  "text/css"},
        {"flv",  "video/x-flv"},
        {"gif",  "image/gif"},
        {"htm",  "text/html"},
        {"html", "text/html"},
        {"ico",  "image/vnd.microsoft.icon"},
        {"jpe",  "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"jpg",  "image/jpeg"},
        {"js",   "application/javascript"},
        {"json", "application/json"},
        {"php",  "text/html"},
        {"png",  "image/png"},
        {"svg",  "image/svg+xml"},
        {"svgz", "image/svg+xml"},
        {"swf",  "application/x-shockwave-flash"},
        {"tif",  "image/tiff"},
        {"tiff", "image/tiff"},
        {"txt",  "text/plain"},
        {"xml",  "application/xml"}
    };

    auto found = table.find(extension);
    if (found != table.end())
        return found->second;
    return "application/text";
}


//******************************************************************************
// HttpWebsocketUpgrade
//******************************************************************************

CPPWAMP_INLINE HttpWebsocketUpgrade::HttpWebsocketUpgrade(std::string route)
    : route_(std::move(route))
{}

CPPWAMP_INLINE HttpWebsocketUpgrade&
HttpWebsocketUpgrade::withMaxRxLength(std::size_t length)
{
    maxRxLength_ = length;
    return *this;
}

CPPWAMP_INLINE const std::string& HttpWebsocketUpgrade::route() const
{return route_;}

CPPWAMP_INLINE std::size_t HttpWebsocketUpgrade::maxRxLength() const
{
    return maxRxLength_;
}

//******************************************************************************
// Listener<Http>
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Http>::Listener(AnyIoExecutor e, IoStrand i, Settings s,
                                        CodecIdSet c, RouterLogger::Ptr l)
    : Listening(s.label()),
      impl_(std::make_shared<internal::HttpListener>(
            std::move(e), std::move(i), std::move(s), std::move(c),
            std::move(l)))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Http>::~Listener() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Http>::observe(Handler handler)
{
    impl_->observe(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Http>::establish() {impl_->establish();}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Http>::cancel() {impl_->cancel();}

namespace internal
{

//******************************************************************************
// HttpServeStaticFiles
//******************************************************************************

CPPWAMP_INLINE HttpAction<HttpServeStaticFiles>::HttpAction(
    HttpServeStaticFiles options)
    : options_(options)
{}

CPPWAMP_INLINE std::string HttpAction<HttpServeStaticFiles>::route() const
{
    return options_.route();
}

void CPPWAMP_INLINE
HttpAction<HttpServeStaticFiles>::execute(HttpJob& job)
{
    namespace http = boost::beast::http;

    if (!checkRequest(job))
        return;

    // Build file path and attempt to open the file
    const auto& req = job.request();
    auto path = buildPath(job.settings(), req.target());
    http::file_body::value_type body;
    if (!openFile(job, path, body))
        return;

    // Extract the file extension and lookup its MIME type
    auto mimeType = lookupMimeType(path);

    // Respond to HEAD request
    if (req.method() == http::verb::head)
    {
        http::response<http::empty_body> res{http::status::ok, req.version()};
        res.set(http::field::server, job.settings().agent());
        res.set(http::field::content_type, mimeType);
        res.content_length(body.size());
        res.keep_alive(req.keep_alive());
        return job.respond(std::move(res));
    }

    // Respond to GET request
    http::response<http::file_body> res{http::status::ok, req.version(),
                                        std::move(body)};
    res.set(http::field::server, job.settings().agent());
    res.set(http::field::content_type, mimeType);
    res.prepare_payload();
    res.keep_alive(req.keep_alive());
    return job.respond(std::move(res));
};

CPPWAMP_INLINE bool
HttpAction<HttpServeStaticFiles>::checkRequest(HttpJob& job) const
{
    namespace beast = boost::beast;
    namespace http = beast::http;

    // Check that request is not an HTTP upgrade request
    const auto& req = job.request();
    if (beast::websocket::is_upgrade(req))
    {
        job.balk(HttpStatus::badRequest, "Not a Websocket resource");
        return false;
    }

    // Check that request method is supported
    if (req.method() != http::verb::get || req.method() != http::verb::head)
    {
        job.balk(HttpStatus::methodNotAllowed,
                 std::string(req.method_string()) +
                     " method not allowed on static files");
        return false;
    }

    // Request path must be absolute and not contain ".."
    if (req.target().find("..") != beast::string_view::npos)
    {
        job.balk(HttpStatus::badRequest, "Invalid request-target");
        return false;
    }

    return true;
}

CPPWAMP_INLINE std::string
HttpAction<HttpServeStaticFiles>::buildPath(const HttpEndpoint& settings,
                                            const std::string& target) const
{
    std::string path;
    if (options_.path().empty())
    {
        path = httpStaticFilePath(settings.documentRoot(), target);
    }
    else
    {
        if (options_.pathIsAlias())
        {
            // Substitute route portion of target with alias path
            auto routeLength = options_.route().length();
            assert(target.length() >= routeLength);
            std::string base = options_.path();
            base += target.substr(target.length() - routeLength);
        }
        else
        {
            // Append target to root path
            path = httpStaticFilePath(options_.path(), target);
        }
    }

    if (target.back() == '/')
    {
        if (!options_.indexFileName().empty())
            path.append(options_.indexFileName());
        else
            path.append(settings.indexFileName());
    }
    return path;
}

template <typename TBody>
bool HttpAction<HttpServeStaticFiles>::openFile(
    HttpJob& job, const std::string& path, TBody& fileBody) const
{
    namespace beast = boost::beast;
    namespace http = beast::http;

    // Attempt to open the file
    beast::error_code netEc;
    fileBody.open(path.c_str(), beast::file_mode::scan, netEc);

    // Handle the case where the file doesn't exist
    if (netEc == beast::errc::no_such_file_or_directory)
    {
        job.balk(HttpStatus::notFound);
        return false;
    }

    // Handle an unknown error
    if (netEc)
    {
        auto ec = static_cast<std::error_code>(netEc);
        job.balk(
            HttpStatus::internalServerError,
            "An error occurred on the server while processing the request",
            false, {}, AdmitResult::failed(ec, "file open"));
        return false;
    }

    return true;
}

CPPWAMP_INLINE std::string
HttpAction<HttpServeStaticFiles>::lookupMimeType(const std::string& path) const
{
    std::string extension;
    const auto pos = path.rfind(".");
    if (pos != std::string::npos)
        extension = path.substr(pos);
    return options_.lookupMimeType(extension);
}


//******************************************************************************
// HttpWebsocketUpgrade
//******************************************************************************

CPPWAMP_INLINE HttpAction<HttpWebsocketUpgrade>::HttpAction(
    HttpWebsocketUpgrade options)
    : options_(options) {}

CPPWAMP_INLINE std::string HttpAction<HttpWebsocketUpgrade>::route() const
{
    return options_.route();
}

CPPWAMP_INLINE void HttpAction<HttpWebsocketUpgrade>::execute(HttpJob& job)
{
    const auto& req = job.request();
    if (!boost::beast::websocket::is_upgrade(req))
    {
        return job.balk(
            HttpStatus::upgradeRequired,
            "Upgrade field required for accessing Websocket resource",
            false,
            {{boost::beast::http::field::upgrade, "websocket"}});
    }

    job.websocketUpgrade();
};

} // namespace internal

} // namespace wamp
