/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/httpserver.hpp"
#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <boost/filesystem.hpp>
#include "httplistener.hpp"
#include "timeformatting.hpp"

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
HttpServeStaticFiles::withAutoIndex(bool enabled)
{
    autoIndex_ = enabled;
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

CPPWAMP_INLINE bool HttpServeStaticFiles::autoIndex() const
{
    return autoIndex_;
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
        {".bmp",  "image/bmp"},
        {".css",  "text/css"},
        {".flv",  "video/x-flv"},
        {".gif",  "image/gif"},
        {".htm",  "text/html"},
        {".html", "text/html"},
        {".ico",  "image/vnd.microsoft.icon"},
        {".jpe",  "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".jpg",  "image/jpeg"},
        {".js",   "application/javascript"},
        {".json", "application/json"},
        {".php",  "text/html"},
        {".png",  "image/png"},
        {".svg",  "image/svg+xml"},
        {".svgz", "image/svg+xml"},
        {".swf",  "application/x-shockwave-flash"},
        {".tif",  "image/tiff"},
        {".tiff", "image/tiff"},
        {".txt",  "text/plain"},
        {".xml",  "application/xml"}
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

struct HttpAction<HttpServeStaticFiles>::HttpServeStaticFilesImpl
{
    using Options = HttpServeStaticFiles;

    static void execute(HttpJob& job, const Options& options)
    {
        namespace http = boost::beast::http;

        if (!checkRequest(job))
            return;

        // Build file path and attempt to open the file
        const auto& req = job.request();
        Path path;
        FileBody body;
        bool found = false;
        bool hasFilename = buildPath(job.settings(), options, req.target(),
                                     path);
        if (!openFile(job, path, body, found))
            return;

        if (!found)
        {
            if (options.autoIndex() && !hasFilename)
                return listDirectory(job, options, path.remove_filename());
            return job.balk(HttpStatus::notFound);
        }

        // Extract the file extension and lookup its MIME type
        auto mimeType = options.lookupMimeType(path.extension().string());

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

private:
    using Path = boost::filesystem::path;
    using FileBody = boost::beast::http::file_body::value_type;
    using StringBody = boost::beast::http::string_body;
    using StringResponse = boost::beast::http::response<StringBody>;

    static bool checkRequest(HttpJob& job)
    {
        namespace beast = boost::beast;
        namespace http = beast::http;

        // Check that request is not an HTTP upgrade request
        const auto& req = job.request();
        if (beast::websocket::is_upgrade(req))
        {
            job.balk(HttpStatus::badRequest, "Not a Websocket resource", true);
            return false;
        }

        // Check that request method is supported
        if (req.method() != http::verb::get && req.method() != http::verb::head)
        {
            job.balk(HttpStatus::methodNotAllowed,
                     std::string(req.method_string()) +
                         " method not allowed on static files.");
            return false;
        }

        // Request path must be absolute and not contain ".."
        if (req.target().find("..") != beast::string_view::npos)
        {
            job.balk(HttpStatus::badRequest, "Invalid request-target.");
            return false;
        }

        return true;
    }

    static bool buildPath(const HttpEndpoint& settings, const Options& options,
                          const std::string& target, Path& path)
    {
        if (options.path().empty())
        {
            path = settings.documentRoot();
            path /= target;
        }
        else
        {
            if (options.pathIsAlias())
            {
                // Substitute route portion of target with alias path
                auto routeLength = options.route().length();
                assert(target.length() >= routeLength);
                const auto& alias = options.path();
                std::string base = alias;
                path = base + target.substr(target.length() - routeLength);
            }
            else
            {
                // Append target to root path
                path = options.path();
                path /= target;
            }
        }

        // path::filename differs between Boost.Filesystem v3 and v4
        Path filename = path.filename();
        bool hasFilename = !filename.empty() && !filename.filename_is_dot();

        // Append the default index filename if the path corresponds to
        // a directory.
        if (!hasFilename)
        {
            if (!options.indexFileName().empty())
                path /= options.indexFileName();
            else
                path /= settings.indexFileName();
        }

        return hasFilename;
    }

    static bool openFile(HttpJob& job, const Path& path, FileBody& fileBody,
                         bool& found)
    {
        namespace beast = boost::beast;
        namespace http = beast::http;

        beast::error_code netEc;
        fileBody.open(path.c_str(), beast::file_mode::scan, netEc);

        if (netEc == beast::errc::no_such_file_or_directory)
        {
            found = false;
            return true;
        }

        if (netEc)
        {
            auto ec = static_cast<std::error_code>(netEc);
            job.balk(
                HttpStatus::internalServerError,
                "An error occurred on the server while processing the request.",
                false, {}, AdmitResult::failed(ec, "file open"));
            found = false;
            return false;
        }

        found = true;
        return true;
    }

    static void listDirectory(HttpJob& job, const Options& options,
                              const Path& directory)
    {
        namespace fs = boost::filesystem;

        auto status = fs::status(directory);
        if (!fs::exists(status) || !fs::is_directory(status))
            return job.balk(HttpStatus::notFound);

        auto page = startDirectoryListing(job);

        boost::system::error_code sysEc;
        for (const auto& entry : fs::directory_iterator(directory, sysEc))
        {
            if (sysEc)
                break;
            sysEc = addDirectoryEntry(page.body(), entry);
            if (sysEc)
                break;
        }

        if (sysEc)
        {
            auto ec = static_cast<std::error_code>(sysEc);
            return job.balk(
                HttpStatus::internalServerError,
                "An error occurred on the server while processing the request.",
                false, {}, AdmitResult::failed(ec, "list directory"));
        }

        finishDirectoryListing(page);
        job.respond(std::move(page));
    }

    static StringResponse startDirectoryListing(HttpJob& job)
    {
        namespace http = boost::beast::http;

        auto req = job.request();
        std::string dir{req.target()};

        StringResponse res{
            http::status::ok,
            req.version(),
            "<html>\n"
            "<head><title>Index of " + dir + "</title></head>\n"
            "<body>\n"
            "<h1>Index of " + dir + "</h1>\n"
            "<hr>\n"
            "<pre>\n"};

        res.base().set(http::field::server, job.settings().agent());
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        return res;
    }

    static std::error_code addDirectoryEntry(
        std::string& body, const boost::filesystem::directory_entry& entry)
    {
        namespace fs = boost::filesystem;

        static constexpr unsigned lineWidth = 79;
        static constexpr unsigned sizeWidth = 19; // Up to 2^63
        static constexpr unsigned timestampWidth = 16; // YYYY-MM-DD HH:MM
        static constexpr unsigned nameWidth =
            lineWidth - sizeWidth - timestampWidth - 2;

        auto status = entry.status();
        std::ostringstream oss;
        oss.imbue(std::locale::classic());
        bool isDirectory = fs::is_directory(status);

        // Name column
        // TODO: Hyperlinks
        auto name = entry.path().filename().string();
        if (name.length() > nameWidth)
        {
            name.resize(nameWidth - 3);
            name += "..>";
        }
        if (isDirectory)
            name += "/";
        oss << std::left << std::setfill(' ') << std::setw(nameWidth) << name;

        // Timestamp column
        boost::system::error_code sysEc;
        auto time = fs::last_write_time(entry.path(), sysEc);
        if (sysEc)
            return sysEc;
        outputFileTimestamp(time, oss);

        // Size column
        oss << std::right << std::setw(sizeWidth);
        if (isDirectory)
        {
            oss << '-';
        }
        else
        {
            auto size = fs::file_size(entry.path(), sysEc);
            if (sysEc)
                return sysEc;
            oss << size;
        }

        body += oss.str();
        return {};
    }

    static void finishDirectoryListing(StringResponse& page)
    {
        page.body() += "</pre>\n"
                       "<hr>\n"
                       "</body>\n"
                       "</html>";
        page.prepare_payload();
    }
};

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
    HttpServeStaticFilesImpl::execute(job, options_);
};


//******************************************************************************
// HttpWebsocketUpgrade
//******************************************************************************

CPPWAMP_INLINE HttpAction<HttpWebsocketUpgrade>::HttpAction(
    HttpWebsocketUpgrade options)
    : options_(options)
{}

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
            "Upgrade field required for accessing Websocket resource.",
            false,
            {{boost::beast::http::field::upgrade, "websocket"}});
    }

    job.websocketUpgrade();
};

} // namespace internal

} // namespace wamp
