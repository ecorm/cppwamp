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
    receiveLimit_ = length;
    return *this;
}

CPPWAMP_INLINE const std::string& HttpWebsocketUpgrade::route() const
{return route_;}

CPPWAMP_INLINE std::size_t HttpWebsocketUpgrade::receiveLimit() const
{
    return receiveLimit_;
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
CPPWAMP_INLINE Transporting::Ptr Listener<Http>::take() {return impl_->take();}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Http>::drop() {impl_->drop();}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Http>::cancel() {impl_->cancel();}

namespace internal
{

//******************************************************************************
// HttpServeStaticFiles
//******************************************************************************

struct HttpServeStaticFilesImpl
{
    using Options = HttpServeStaticFiles;

    HttpServeStaticFilesImpl(HttpJob& job, const Options& options)
        : job_(job),
          options_(options)
    {}

    void execute()
    {
        namespace fs = boost::filesystem;
        namespace http = boost::beast::http;

        if (!checkRequest())
            return;

        buildPath();
        FileStatus status;
        if (!stat(absolutePath_, status))
            return;
        if (!fs::exists(status))
            return job_.balk(HttpStatus::notFound);

        bool isDirectory = fs::is_directory(status);
        if (isDirectory)
        {
            absolutePath_ /= indexFileName();
            if (!stat(absolutePath_, status))
                return;

            if (!fs::exists(status))
            {
                if (!options_.autoIndex())
                    return job_.balk(HttpStatus::notFound);
                absolutePath_.remove_filename();
                return listDirectory();
            }
        }

        FileBody body;
        if (!openFile(body))
            return;

        auto ext = absolutePath_.extension().string();
        auto mimeType = options_.lookupMimeType(ext);

        if (job_.request().method() == http::verb::head)
            return respondToHeadRequest(body, mimeType);
        respondToGetRequest(body, mimeType);
    };

private:
    using Path           = boost::filesystem::path;
    using FileStatus     = boost::filesystem::file_status;
    using FileBody       = boost::beast::http::file_body::value_type;
    using StringBody     = boost::beast::http::string_body;
    using StringResponse = boost::beast::http::response<StringBody>;

    bool checkRequest()
    {
        namespace beast = boost::beast;
        namespace http = beast::http;

        // HttpJob has already checked that target does not contain
        // dot-dot parent paths.

        // Check that request is not an HTTP upgrade request
        const auto& req = job_.request();
        if (beast::websocket::is_upgrade(req))
        {
            job_.balk(HttpStatus::badRequest, "Not a Websocket resource", true);
            return false;
        }

        // Check that request method is supported
        if (req.method() != http::verb::get && req.method() != http::verb::head)
        {
            job_.balk(HttpStatus::methodNotAllowed,
                     std::string(req.method_string()) +
                         " method not allowed on static files.");
            return false;
        }

        return true;
    }

    void buildPath()
    {
        if (options_.path().empty())
        {
            absolutePath_ = Path{job_.settings().documentRoot()} /
                            job_.target().relative_path();
        }
        else
        {
            if (options_.pathIsAlias())
            {
                // Substitute route portion of target with alias path
                auto routeLen = options_.route().length();
                auto targetStr = job_.target().generic_string();
                assert(targetStr.length() >= routeLen);
                const auto& alias = options_.path();
                auto targetTailLen = targetStr.length() - routeLen;
                absolutePath_.assign(alias + targetStr.substr(targetTailLen));
            }
            else
            {
                // Append target to root path
                absolutePath_ = Path{options_.path()} /
                                job_.target().relative_path();
            }
        }
    }

    bool stat(const Path& path, bool& result)
    {
        boost::system::error_code sysEc;
        result = boost::filesystem::exists(path, sysEc);
        if (sysEc == boost::system::errc::no_such_file_or_directory)
            return true;
        if (!check(sysEc, "file exists query"))
            return false;
        return true;
    }

    bool stat(const Path& path, FileStatus& status)
    {
        namespace fs = boost::filesystem;

        boost::system::error_code sysEc;
        status = fs::status(path, sysEc);

        if (sysEc == boost::system::errc::no_such_file_or_directory)
        {
            status = FileStatus{fs::file_type::file_not_found};
            return true;
        }

        return check(sysEc, "file stat");
    }

    const std::string& indexFileName() const
    {
        return options_.indexFileName().empty()
            ? job_.settings().indexFileName()
            : options_.indexFileName();
    }

    bool openFile(FileBody& fileBody)
    {
        namespace beast = boost::beast;

        beast::error_code netEc;
        fileBody.open(absolutePath_.c_str(), beast::file_mode::scan, netEc);
        return check(netEc, "file open");
    }

    void listDirectory()
    {
        namespace fs = boost::filesystem;

        auto page = startDirectoryListing();

        boost::system::error_code sysEc;
        for (const auto& entry : fs::directory_iterator(absolutePath_, sysEc))
        {
            if (sysEc)
                break;
            sysEc = addDirectoryEntry(page.body(), entry);
            if (sysEc)
                break;
        }

        if (!check(sysEc, "list directory"))
            return;

        finishDirectoryListing(page);
        job_.respond(std::move(page));
    }

    StringResponse startDirectoryListing()
    {
        namespace http = boost::beast::http;

        const auto& req = job_.request();
        auto dir = job_.target();
        auto dirString = dir.generic_string();

        std::string body{
            "<html>\n"
            "<head><title>Index of " + dirString + "</title></head>\n"
            "<body>\n"
            "<h1>Index of " + dirString + "</h1>\n"
            "<hr>\n"
            "<pre>\n"};

        if (dir.filename_is_dot()) // filesystem v3
        {
            dir.remove_filename();
            dir.remove_filename();
        }
        else // filesystem v4
        {
            dir = dir.parent_path();
            dir = dir.parent_path();
        }

        if (!dir.empty())
        {
            if (dir.has_parent_path())
                dir.concat("/");
            body += "<a href=\"" + dir.generic_string() + "\">"
                    "../</a>\n";
        }

        StringResponse res{http::status::ok, req.version(), std::move(body)};
        res.base().set(http::field::server, job_.settings().agent());
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        return res;
    }

    std::error_code addDirectoryEntry(
        std::string& body,
        const boost::filesystem::directory_entry& entry) const
    {
        namespace fs = boost::filesystem;

        auto status = entry.status();
        std::ostringstream oss;
        oss.imbue(std::locale::classic());
        bool isDirectory = fs::is_directory(status);

        // Name column
        auto name = entry.path().filename().string();
        if (isDirectory)
            name += "/";
        auto link = fs::path{job_.request().target()} / name;
        oss << "<a href=\"" << link.generic_string() << "\">";
        if (name.length() > autoindexNameWidth_)
        {
            name.resize(autoindexNameWidth_ - 3);
            name += "..>";
        }

        const auto paddingLength = autoindexNameWidth_ - name.length() + 1;
        oss << name << "</a>" << std::string(paddingLength, ' ');

        // Timestamp column
        boost::system::error_code sysEc;
        auto time = fs::last_write_time(entry.path(), sysEc);
        if (sysEc)
            return sysEc;
        outputFileTimestamp(time, oss);

        // Size column
        oss << " " << std::right << std::setfill(' ')
            << std::setw(autoindexSizeWidth_);
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

        oss << "\n";
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

    void respondToHeadRequest(FileBody& body, std::string& mimeType)
    {
        namespace http = boost::beast::http;

        const auto& req = job_.request();
        http::response<http::empty_body> res{http::status::ok, req.version()};
        res.set(http::field::server, job_.settings().agent());
        res.set(http::field::content_type, std::move(mimeType));
        res.content_length(body.size());
        res.keep_alive(req.keep_alive());
        job_.respond(std::move(res));
    }

    void respondToGetRequest(FileBody& body, std::string& mimeType)
    {
        namespace http = boost::beast::http;

        const auto& req = job_.request();
        http::response<http::file_body> res{http::status::ok, req.version(),
                                            std::move(body)};
        res.set(http::field::server, job_.settings().agent());
        res.set(http::field::content_type, std::move(mimeType));
        res.prepare_payload();
        res.keep_alive(req.keep_alive());
        job_.respond(std::move(res));
    }

    bool check(boost::system::error_code sysEc, const char* operation)
    {
        if (!sysEc)
            return true;
        fail(static_cast<std::error_code>(sysEc), operation);
        return false;
    }

    void fail(std::error_code ec, const char* operation)
    {
        job_.balk(
            HttpStatus::internalServerError,
            "An error occurred on the server while processing the request.",
            false, {}, AdmitResult::failed(ec, operation));
    }
    static constexpr unsigned autoindexLineWidth_ = 79;
    static constexpr unsigned autoindexSizeWidth_ = 19; // Up to 2^63
    static constexpr unsigned autoindexTimestampWidth_ = 16; // YYYY-MM-DD HH:MM
    static constexpr unsigned autoindexNameWidth_ =
        autoindexLineWidth_ - autoindexSizeWidth_ -
        autoindexTimestampWidth_ - 2;

    Path absolutePath_;
    HttpJob& job_;
    const Options& options_;
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
    HttpServeStaticFilesImpl{job, options_}.execute();
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
