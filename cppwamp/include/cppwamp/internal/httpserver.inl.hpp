/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/httpserver.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <tuple>
#include <vector>
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

/** @post `this->alias() == alias`
    @post `this->hasAlias() == true` */
CPPWAMP_INLINE HttpServeStaticFiles&
HttpServeStaticFiles::withAlias(std::string alias)
{
    alias_ = std::move(alias);
    hasAlias_ = true;
    return *this;
}

CPPWAMP_INLINE HttpServeStaticFiles&
HttpServeStaticFiles::withOptions(HttpFileServingOptions options)
{
    options_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE const std::string& HttpServeStaticFiles::route() const
{
    return route_;
}

CPPWAMP_INLINE bool HttpServeStaticFiles::hasAlias() const {return hasAlias_;}

CPPWAMP_INLINE const std::string& HttpServeStaticFiles::alias() const
{
    return alias_;
}

CPPWAMP_INLINE const HttpFileServingOptions&
HttpServeStaticFiles::options() const
{
    return options_;
}

CPPWAMP_INLINE void HttpServeStaticFiles::applyFallbackOptions(
    const HttpFileServingOptions& fallback)
{
    options_.applyFallback(fallback);
}


//******************************************************************************
// HttpWebsocketUpgrade
//******************************************************************************

CPPWAMP_INLINE HttpWebsocketUpgrade::HttpWebsocketUpgrade(std::string route)
    : route_(std::move(route))
{}

CPPWAMP_INLINE HttpWebsocketUpgrade&
HttpWebsocketUpgrade::withOptions(WebsocketOptions options)
{
    options_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE HttpWebsocketUpgrade&
HttpWebsocketUpgrade::withLimits(WebsocketServerLimits limits)
{
    limits_ = limits;
    return *this;
}

CPPWAMP_INLINE const std::string& HttpWebsocketUpgrade::route() const
{
    return route_;
}

CPPWAMP_INLINE const WebsocketOptions HttpWebsocketUpgrade::options() const
{
    return options_;
}

CPPWAMP_INLINE const WebsocketServerLimits& HttpWebsocketUpgrade::limits() const
{
    return limits_;
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
// HttpServeDirectoryListing
//******************************************************************************

class HttpServeDirectoryListing
{
public:
    using Path = boost::filesystem::path;

    static boost::system::error_code list(HttpJob& job, Path absolutePath)
    {
        namespace fs = boost::filesystem;

        if (!checkTrailingSlashInDirectoryPath(job))
            return {};

        auto page = startDirectoryListing(job);
        boost::system::error_code sysEc;
        std::vector<Row> rows;
        Row row;

        for (const auto& entry : fs::directory_iterator(absolutePath, sysEc))
        {
            if (sysEc)
                break;
            sysEc = computeRow(job, entry, row);
            rows.emplace_back(std::move(row));
            if (sysEc)
                break;
        }

        if (sysEc)
            return sysEc;

        std::sort(rows.begin(), rows.end());
        for (const auto& r: rows)
            page.body() += r.text;

        finishDirectoryListing(page);
        job.respond(std::move(page));
        return {};
    };

private:
    using DirectoryEntry = boost::filesystem::directory_entry;
    using StringBody     = boost::beast::http::string_body;
    using StringResponse = boost::beast::http::response<StringBody>;
    using EmptyBody      = boost::beast::http::empty_body;
    using EmptyResponse  = boost::beast::http::response<EmptyBody>;

    struct Row
    {
        std::string text;
        bool isFile;

        bool operator<(const Row& rhs) const
        {
            return std::tie(isFile, text) < std::tie(rhs.isFile, rhs.text);
        }
    };

    static bool checkTrailingSlashInDirectoryPath(HttpJob& job)
    {
        auto path = job.target().path();
        bool pathIsMissingTrailingSlash = !path.empty() && path.back() != '/';
        if (!pathIsMissingTrailingSlash)
            return true;

        namespace http = boost::beast::http;
        EmptyResponse res{http::status::moved_permanently,
                          job.header().version()};
        res.base().set(http::field::location, path + '/');
        job.respond(std::move(res));
        return false;
    }

    static StringResponse startDirectoryListing(HttpJob& job)
    {
        namespace http = boost::beast::http;

        // Remove empty segments not removed by boost::urls::url::normalize
        std::string dirString;
        const auto& segments = job.target().segments();
        if (!segments.empty())
        {
            for (auto segment: segments)
            {
                if (!segment.empty())
                {
                    dirString += '/';
                    dirString += segment;
                }
            }
        }

        std::string body{
            "<html>\r\n"
            "<head><title>Index of " + dirString + "</title></head>\r\n"
            "<body>\r\n"
            "<h1>Index of " + dirString + "/</h1>\r\n"
            "<hr>\r\n"
            "<pre>\r\n"};

        if (!dirString.empty())
        {
            boost::filesystem::path path{dirString};
            auto parent = path.parent_path().string();
            if (parent.back() != '/')
                parent += '/';
            body += "<a href=\"" + parent + "\">../</a>\r\n";
        }

        StringResponse res{http::status::ok, job.header().version(),
                           std::move(body)};
        res.set(http::field::content_type, "text/html; charset=utf-8");
        return res;
    }

    static std::error_code computeRow(const HttpJob& job,
                                      const DirectoryEntry& entry,
                                      Row& row)
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
        auto link = fs::path{job.header().target()} / name;
        oss << "<a href=\"" << link.generic_string() << "\">";
        auto nameLength = countUtf8CodePoints(name);
        if (nameLength > autoindexNameWidth_)
        {
            name = trimUtf8(name, autoindexNameWidth_ - 3);
            name += "..>";
            nameLength = autoindexNameWidth_;
        }

        const auto paddingLength = autoindexNameWidth_ - nameLength + 1;
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

        oss << "\r\n";
        row.text = oss.str();
        row.isFile = !isDirectory;
        return {};
    }

    static std::size_t countUtf8CodePoints(const std::string& utf8)
    {
        std::size_t count = 0;
        for (auto c: utf8)
            count += !isUtf8MultiByteContinuation(c);
        return count;
    }

    static std::string trimUtf8(std::string& utf8, std::size_t limit)
    {
        std::string result;
        std::size_t count = 0;

        for (auto c: utf8)
        {
            count += !isUtf8MultiByteContinuation(c);
            if (count == (limit + 1))
                break;
            result += c;
        }

        return result;
    }

    static bool isUtf8MultiByteContinuation(char c)
    {
        return (c & 0xc0) == 0x80;
    }

    static void finishDirectoryListing(StringResponse& page)
    {
        page.body() += "</pre>\r\n"
                       "<hr>\r\n"
                       "</body>\r\n"
                       "</html>";
    }

    static constexpr unsigned autoindexLineWidth_ = 79;
    static constexpr unsigned autoindexSizeWidth_ = 19; // Up to 2^63
    static constexpr unsigned autoindexTimestampWidth_ = 16; // YYYY-MM-DD HH:MM
    static constexpr unsigned autoindexNameWidth_ =
        autoindexLineWidth_ - autoindexSizeWidth_ -
        autoindexTimestampWidth_ - 2;
};

//******************************************************************************
// HttpServeStaticFiles
//******************************************************************************

class HttpServeStaticFilesImpl
{
public:
    using Properties = HttpServeStaticFiles;

    HttpServeStaticFilesImpl(const Properties& properties)
        : properties_(properties)
    {}

    const Properties& properties() const {return properties_;}

    void init(const HttpEndpoint& settings)
    {
        properties_.applyFallbackOptions(settings.fileServingOptions());
    }

    void expect(HttpJob& job)
    {
        if (checkRequest(job))
            job.continueRequest();
    }

    void execute(HttpJob& job)
    {
        namespace fs = boost::filesystem;

        if (!checkRequest(job))
            return;

        buildPath(job);
        FileStatus status;
        if (!stat(job, absolutePath_, status))
            return;
        if (!fs::exists(status))
            return job.balk(HttpStatus::notFound);

        bool isDirectory = fs::is_directory(status);
        if (isDirectory)
        {
            absolutePath_ /= indexFileName();
            if (!stat(job, absolutePath_, status))
                return;

            if (!fs::exists(status))
            {
                if (!autoIndex())
                    return job.balk(HttpStatus::notFound);
                absolutePath_.remove_filename();
                auto ec = HttpServeDirectoryListing::list(job, absolutePath_);
                check(job, ec, "list directory");
                return;
            }

            // Fall through if directory/index.html exists.
        }

        FileBody body;
        if (!openFile(job, body))
            return;

        if (job.header().method() == boost::beast::http::verb::head)
            return respondToHeadRequest(job, body);
        respondToGetRequest(job, body);
    };

private:
    using Path           = boost::filesystem::path;
    using FileStatus     = boost::filesystem::file_status;
    using FileBody       = boost::beast::http::file_body::value_type;
    using StringBody     = boost::beast::http::string_body;
    using StringResponse = boost::beast::http::response<StringBody>;
    using EmptyBody      = boost::beast::http::empty_body;
    using EmptyResponse  = boost::beast::http::response<EmptyBody>;

    bool checkRequest(HttpJob& job)
    {
        // Check that request is not an HTTP upgrade request
        const auto& hdr = job.header();
        if (boost::beast::websocket::is_upgrade(hdr))
        {
            job.balk(HttpStatus::badRequest, "Not a Websocket resource", true);
            return false;
        }

        // Check that request method is supported
        using Verb = boost::beast::http::verb;
        if (hdr.method() != Verb::get && hdr.method() != Verb::head)
        {
            job.balk(HttpStatus::methodNotAllowed,
                     std::string(hdr.method_string()) +
                         " method not allowed on static files.");
            return false;
        }

        // Reject proxying requests
        // TODO: Check if absolute URI domain matches server name or alias
        const auto& target = job.target();
        if (target.has_scheme() || target.has_authority())
        {
            job.balk(HttpStatus::badRequest, "Not configured for proxying",
                     true);
            return false;
        }

        // Reject target paths that contain dot-dot segments which can allow
        // access filesystem access outside the document root.
        for (const auto& segment: target.segments())
        {
            if (segment == "..")
            {
                job.balk(HttpStatus::badRequest, "Invalid target path", true);
                return false;
            }
        }

        return true;
    }

    void buildPath(const HttpJob& job)
    {
        absolutePath_ = properties_.options().documentRoot();
        if (!properties_.hasAlias())
        {
            absolutePath_ /= job.target().path();
            return;
        }

        // Substitute the route portion of the target with the alias
        auto routeLen = properties_.route().length();
        auto targetStr = job.target().buffer();
        assert(targetStr.length() >= routeLen);
        std::string path = properties_.alias();
        path += targetStr.substr(routeLen, std::string::npos);
        absolutePath_ /= path;
    }

    bool stat(HttpJob& job, const Path& path, FileStatus& status)
    {
        namespace fs = boost::filesystem;

        boost::system::error_code sysEc;
        status = fs::status(path, sysEc);

        if (sysEc == boost::system::errc::no_such_file_or_directory)
        {
            status = FileStatus{fs::file_type::file_not_found};
            return true;
        }

        return check(job, sysEc, "file stat");
    }

    bool autoIndex() const {return properties_.options().autoIndex();}

    std::string buildMimeType() const
    {
        auto ext = absolutePath_.extension().string();
        auto mime = properties_.options().lookupMimeType(ext);
        const auto& defaultCharset = properties_.options().charset();

        if (!defaultCharset.empty() &&
            mime.find("charset") == std::string::npos)
        {
            mime += "; charset=";
            mime += defaultCharset;
        }

        return mime;
    }

    const std::string& indexFileName() const
    {
        return properties_.options().indexFileName();
    }

    bool openFile(HttpJob& job, FileBody& fileBody)
    {
        namespace beast = boost::beast;

        beast::error_code netEc;
        fileBody.open(absolutePath_.c_str(), beast::file_mode::scan, netEc);
        return check(job, netEc, "file open");
    }

    void respondToHeadRequest(HttpJob& job, FileBody& body)
    {
        namespace http = boost::beast::http;

        http::response<http::empty_body> res{http::status::ok,
                                             job.header().version()};
        res.set(http::field::content_type, buildMimeType());
        res.content_length(body.size());
        job.respond(std::move(res));
    }

    void respondToGetRequest(HttpJob& job, FileBody& body)
    {
        namespace http = boost::beast::http;

        http::response<http::file_body> res{
            http::status::ok, job.header().version(), std::move(body)};
        res.set(http::field::content_type, buildMimeType());
        job.respond(std::move(res));
    }

    bool check(HttpJob& job, boost::system::error_code sysEc,
               const char* operation)
    {
        if (!sysEc)
            return true;
        fail(job, static_cast<std::error_code>(sysEc), operation);
        return false;
    }

    void fail(HttpJob& job, std::error_code ec, const char* operation)
    {
        job.balk(
            HttpStatus::internalServerError,
            "An error occurred on the server while processing the request.",
            false, AdmitResult::failed(ec, operation));
    }

    HttpServeStaticFiles properties_;
    Path absolutePath_;
};


//******************************************************************************
// HttpAction<HttpServeStaticFiles>
//******************************************************************************

CPPWAMP_INLINE
HttpAction<HttpServeStaticFiles>::HttpAction(HttpServeStaticFiles properties)
    : impl_(new HttpServeStaticFilesImpl(std::move(properties)))
{}

CPPWAMP_INLINE HttpAction<HttpServeStaticFiles>::~HttpAction() = default;

CPPWAMP_INLINE std::string HttpAction<HttpServeStaticFiles>::route() const
{
    return impl_->properties().route();
}

CPPWAMP_INLINE void
HttpAction<HttpServeStaticFiles>::initialize(const HttpEndpoint& settings)
{
    impl_->init(settings);
}

CPPWAMP_INLINE void HttpAction<HttpServeStaticFiles>::expect(HttpJob& job)
{
    impl_->expect(job);
}

CPPWAMP_INLINE void HttpAction<HttpServeStaticFiles>::execute(HttpJob& job)
{
    impl_->execute(job);
}


//******************************************************************************
// HttpWebsocketUpgrade
//******************************************************************************

CPPWAMP_INLINE HttpAction<HttpWebsocketUpgrade>::HttpAction(
    HttpWebsocketUpgrade properties)
    : properties_(properties)
{}

CPPWAMP_INLINE std::string HttpAction<HttpWebsocketUpgrade>::route() const
{
    return properties_.route();
}

CPPWAMP_INLINE void
HttpAction<HttpWebsocketUpgrade>::initialize(const HttpEndpoint& settings) {}

CPPWAMP_INLINE void HttpAction<HttpWebsocketUpgrade>::expect(HttpJob& job)
{
    if (checkRequest(job))
        job.continueRequest();
}

CPPWAMP_INLINE void HttpAction<HttpWebsocketUpgrade>::execute(HttpJob& job)
{
    if (checkRequest(job))
        job.websocketUpgrade(properties_.options(), properties_.limits());
};

CPPWAMP_INLINE bool HttpAction<HttpWebsocketUpgrade>::checkRequest(HttpJob& job)
{
    if (!boost::beast::websocket::is_upgrade(job.header()))
    {
        job.balk(
            HttpStatus::upgradeRequired,
            "This service requires use of the Websocket protocol.",
            true,
            AdmitResult::rejected(TransportErrc::badHandshake),
            {
                {boost::beast::http::field::connection, "Upgrade"},
                {boost::beast::http::field::upgrade, "websocket"}
            });
        return false;
    }

    return true;
}

} // namespace internal

} // namespace wamp
