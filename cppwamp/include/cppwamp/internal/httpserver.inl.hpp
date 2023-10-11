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
// HttpServeStaticFile
//******************************************************************************

CPPWAMP_INLINE HttpServeStaticFile::HttpServeStaticFile(
    std::string documentRoot)
    : documentRoot_(std::move(documentRoot))
{}

CPPWAMP_INLINE HttpServeStaticFile&
HttpServeStaticFile::withMimeTypes(MimeTypeMapper f)
{
    mimeTypeMapper_ = std::move(f);
    return *this;
}

CPPWAMP_INLINE std::string HttpServeStaticFile::documentRoot() const
{
    return documentRoot_;
}

CPPWAMP_INLINE char HttpServeStaticFile::toLower(char c)
{
    static constexpr unsigned offset = 'a' - 'A';
    if (c >= 'A' && c <= 'Z')
        c += offset;
    return c;
}

CPPWAMP_INLINE std::string
HttpServeStaticFile::lookupMimeType(std::string extension)
{
    for (auto& c: extension)
        c = toLower(c);
    return !mimeTypeMapper_ ? defaultMimeType(extension)
                            : mimeTypeMapper_(extension);
}

CPPWAMP_INLINE std::string
HttpServeStaticFile::defaultMimeType(const std::string& extension)
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

CPPWAMP_INLINE HttpWebsocketUpgrade&
HttpWebsocketUpgrade::withMaxRxLength(std::size_t length)
{
    maxRxLength_ = length;
    return *this;
}

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
// HttpServeStaticFile
//******************************************************************************

CPPWAMP_INLINE HttpAction<HttpServeStaticFile>::HttpAction(
    HttpServeStaticFile options)
    : options_(options)
{}

void CPPWAMP_INLINE
HttpAction<HttpServeStaticFile>::execute(HttpJob& job)
{
    namespace beast = boost::beast;
    namespace http = beast::http;


    const auto& req = job.request();
    if (beast::websocket::is_upgrade(req))
        return job.balk(HttpStatus::badRequest, "Not a Websocket resource");

    if (req.method() != http::verb::get || req.method() != http::verb::head)
    {
        return job.balk(HttpStatus::methodNotAllowed,
                        std::string(req.method_string()) +
                            " method not allowed on static files");
    }

    // Request path must be absolute and not contain ".."
    if (req.target().find("..") != beast::string_view::npos)
        return job.balk(HttpStatus::badRequest, "Invalid request-target");

    // Build the path to the requested file
    std::string root = options_.documentRoot();
    if (root.empty())
        root = job.settings().documentRoot();
    std::string path = httpStaticFilePath(options_.documentRoot(),
                                          req.target());
    if (req.target().back() == '/')
        path.append("index.html");

    // Attempt to open the file
    beast::error_code ec;
    http::file_body::value_type body;
    body.open(path.c_str(), beast::file_mode::scan, ec);

    // Handle the case where the file doesn't exist
    if (ec == beast::errc::no_such_file_or_directory)
        return job.balk(HttpStatus::notFound);

    // Handle an unknown error
    if (ec)
    {
        // TODO: Log problem
        return job.balk(
            HttpStatus::internalServerError,
            "An error occurred on the server while processing the request");
    }

    // Extract the file extension and lookup it's MIME type
    std::string extension;
    const auto pos = path.rfind(".");
    if (pos != std::string::npos)
        extension = path.substr(pos);
    auto mimeType = options_.lookupMimeType(extension);

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


//******************************************************************************
// HttpWebsocketUpgrade
//******************************************************************************

CPPWAMP_INLINE HttpAction<HttpWebsocketUpgrade>::HttpAction(
    HttpWebsocketUpgrade options)
    : options_(options) {}

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

    // TODO: Hand off upgrade to new WebsocketServerTransport
};

} // namespace internal

} // namespace wamp
