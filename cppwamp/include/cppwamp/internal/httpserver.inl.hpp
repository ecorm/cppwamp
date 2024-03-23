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
#include "../exceptions.hpp"
#include "httplistener.hpp"
#include "timeformatting.hpp"

namespace wamp
{

//******************************************************************************
// HttpServeFiles
//******************************************************************************

CPPWAMP_INLINE HttpServeFiles::HttpServeFiles(std::string route)
    : route_(std::move(route))
{}

/** @post `this->alias() == alias`
    @post `this->hasAlias() == true` */
CPPWAMP_INLINE HttpServeFiles&
HttpServeFiles::withAlias(std::string alias)
{
    alias_ = std::move(alias);
    hasAlias_ = true;
    return *this;
}

CPPWAMP_INLINE HttpServeFiles&
HttpServeFiles::withOptions(HttpFileServingOptions options)
{
    options_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE const std::string& HttpServeFiles::route() const
{
    return route_;
}

CPPWAMP_INLINE bool HttpServeFiles::hasAlias() const {return hasAlias_;}

CPPWAMP_INLINE const std::string& HttpServeFiles::alias() const
{
    return alias_;
}

CPPWAMP_INLINE const HttpFileServingOptions& HttpServeFiles::options() const
{
    return options_;
}

CPPWAMP_INLINE void
HttpServeFiles::mergeOptions(const HttpFileServingOptions& fallback)
{
    options_.merge(fallback);
}


//******************************************************************************
// HttpRedirect
//******************************************************************************

CPPWAMP_INLINE HttpRedirect::HttpRedirect(std::string route)
    : route_(std::move(route)),
      status_(HttpStatus::temporaryRedirect)
{}

CPPWAMP_INLINE HttpRedirect& HttpRedirect::withScheme(std::string scheme)
{
    scheme_ = std::move(scheme);
    return *this;
}

CPPWAMP_INLINE HttpRedirect& HttpRedirect::withAuthority(std::string authority)
{
    authority_ = std::move(authority);
    return *this;
}

/** @details
    This property is applied after the authority property. */
CPPWAMP_INLINE HttpRedirect& HttpRedirect::withHost(std::string host)
{
    host_ = std::move(host);
    return *this;
}

/** @details
    This property is applied after the authority property. */
CPPWAMP_INLINE HttpRedirect& HttpRedirect::withPort(Port port)
{
    port_ = port;
    hasPort_ = true;
    return *this;
}

CPPWAMP_INLINE HttpRedirect& HttpRedirect::withAlias(std::string alias)
{
    alias_ = std::move(alias);
    hasAlias_ = true;
    return *this;
}

CPPWAMP_INLINE HttpRedirect& HttpRedirect::withStatus(HttpStatus s)
{
    using S = HttpStatus;
    bool statusOk = s == S::movedPermanently ||
                    s == S::found ||
                    s == S::seeOther ||
                    s == S::temporaryRedirect ||
                    s == S::permanentRedirect;
    CPPWAMP_LOGIC_CHECK(statusOk, "Invalid redirect status code");
    status_ = s;
    return *this;
}

CPPWAMP_INLINE const std::string& HttpRedirect::route() const {return route_;}

CPPWAMP_INLINE const std::string& HttpRedirect::scheme() const {return scheme_;}

CPPWAMP_INLINE const std::string& HttpRedirect::authority() const
{
    return authority_;
}

CPPWAMP_INLINE const std::string& HttpRedirect::host() const {return host_;}

CPPWAMP_INLINE bool HttpRedirect::hasPort() const {return hasPort_;}

CPPWAMP_INLINE HttpRedirect::Port HttpRedirect::port() const {return port_;}

CPPWAMP_INLINE bool HttpRedirect::hasAlias() const {return hasAlias_;}

CPPWAMP_INLINE const std::string& HttpRedirect::alias() const {return alias_;}

CPPWAMP_INLINE HttpStatus HttpRedirect::status() const {return status_;}


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
CPPWAMP_INLINE ErrorOr<Transporting::Ptr> Listener<Http>::take()
{
    return impl_->take();
}

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

        auto body = startDirectoryListing(job);
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
            body += r.text;

        finishDirectoryListing(body);

        HttpStringResponse page{HttpStatus::ok, std::move(body),
                                {{"Content-type", "text/html; charset=utf-8"}}};
        job.respond(std::move(page));
        return {};
    };

private:
    using DirectoryEntry = boost::filesystem::directory_entry;

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

        HttpResponse res{HttpStatus::movedPermanently,
                         {{"Location", path + '/'}}};
        job.respond(std::move(res));
        return false;
    }

    static std::string startDirectoryListing(HttpJob& job)
    {
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

        return body;
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
        auto link = fs::path{job.target().buffer()} / name;
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

    static void finishDirectoryListing(std::string& body)
    {
        body += "</pre>\r\n"
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
// HttpServeFilesImpl
//******************************************************************************

class HttpServeFilesImpl
{
public:
    using Properties = HttpServeFiles;

    HttpServeFilesImpl(const Properties& properties)
        : properties_(properties)
    {}

    const Properties& properties() const {return properties_;}

    void init(const HttpServerOptions& options)
    {
        properties_.mergeOptions(options.fileServingOptions());
    }

    void expect(HttpJob job)
    {
        if (checkRequest(job))
            job.continueRequest();
    }

    void execute(HttpJob job)
    {
        namespace fs = boost::filesystem;

        if (!checkRequest(job))
            return;

        buildPath(job);
        FileStatus status;
        if (!stat(job, absolutePath_, status))
            return;
        if (!fs::exists(status))
            return job.deny(HttpDenial{HttpStatus::notFound}.withHtmlEnabled());

        bool isDirectory = fs::is_directory(status);
        if (isDirectory)
        {
            absolutePath_ /= indexFileName();
            if (!stat(job, absolutePath_, status))
                return;

            if (!fs::exists(status))
            {
                if (!autoIndex())
                {
                    return job.deny(HttpDenial{HttpStatus::notFound}
                                        .withHtmlEnabled());
                }
                absolutePath_.remove_filename();
                auto ec = internal::HttpServeDirectoryListing::list(
                    job, absolutePath_);
                check(job, ec, "list directory");
                return;
            }

            // Fall through if directory/index.html exists.
        }

        HttpFile file;
        auto ec = file.open(absolutePath_.c_str());
        if (!check(job, ec, "file open"))
            return;

        if (job.method() == "HEAD")
            return respondToHeadRequest(job, file);
        respondToGetRequest(job, file);
    };

private:
    using Path           = boost::filesystem::path;
    using FileStatus     = boost::filesystem::file_status;

    bool checkRequest(HttpJob& job)
    {
        // Check that request method is supported
        auto method = job.method();
        if (method != "GET" && method != "HEAD")
        {
            method += " method not allowed on static files.";
            job.deny(HttpDenial{HttpStatus::methodNotAllowed}
                         .withMessage(method));
            return false;
        }

        // Check that request is not an upgrade request
        if (job.isUpgrade())
        {
            job.deny(HttpDenial{HttpStatus::badRequest}
                         .withMessage("Not a protocol upgrade resource"));
            return false;
        }

        // Reject target paths that contain dot-dot segments which can allow
        // filesystem access outside the document root.
        for (const auto& segment: job.target().segments())
        {
            if (segment == "..")
            {
                job.deny(HttpDenial{HttpStatus::badRequest}
                             .withMessage("Invalid target path")
                             .withHtmlEnabled());
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

    void respondToHeadRequest(HttpJob& job, HttpFile& file)
    {
        HttpResponse response{
            HttpStatus::ok,
            {{"Content-type", buildMimeType()},
             {"Content-length", std::to_string(file.size())}}};
        job.respond(std::move(response));
    }

    void respondToGetRequest(HttpJob& job, HttpFile& file)
    {
        HttpFileResponse response{HttpStatus::ok, std::move(file),
                                  {{"Content-type", buildMimeType()}}};
        job.respond(std::move(response));
    }

    bool check(HttpJob& job, boost::system::error_code sysEc,
               const char* operation)
    {
        return check(job, static_cast<std::error_code>(sysEc), operation);
    }

    bool check(HttpJob& job, std::error_code ec, const char* operation)
    {
        if (!ec)
            return true;
        fail(job, ec, operation);
        return false;
    }

    void fail(HttpJob& job, std::error_code ec, const char* operation)
    {
        job.fail(
            HttpDenial{HttpStatus::internalServerError}
                .withMessage("An error occurred on the server "
                             "while processing the request.")
                .withHtmlEnabled(),
            ec, operation);
    }

    HttpServeFiles properties_;
    Path absolutePath_;
};

} // namespace internal


//******************************************************************************
// HttpAction<HttpServeFiles>
//******************************************************************************

CPPWAMP_INLINE
HttpAction<HttpServeFiles>::HttpAction(HttpServeFiles properties)
    : impl_(new internal::HttpServeFilesImpl(std::move(properties)))
{}

CPPWAMP_INLINE HttpAction<HttpServeFiles>::~HttpAction() = default;

CPPWAMP_INLINE std::string HttpAction<HttpServeFiles>::route() const
{
    return impl_->properties().route();
}

CPPWAMP_INLINE void
HttpAction<HttpServeFiles>::initialize(const HttpServerOptions& options)
{
    impl_->init(options);
}

CPPWAMP_INLINE void HttpAction<HttpServeFiles>::expect(HttpJob& job)
{
    impl_->expect(std::move(job));
}

CPPWAMP_INLINE void HttpAction<HttpServeFiles>::execute(HttpJob& job)
{
    impl_->execute(std::move(job));
}


//******************************************************************************
// HttpAction<HttpRedirect>
//******************************************************************************

CPPWAMP_INLINE
HttpAction<HttpRedirect>::HttpAction(HttpRedirect properties)
    : properties_(properties)
{}

CPPWAMP_INLINE std::string HttpAction<HttpRedirect>::route() const
{
    return properties_.route();
}

CPPWAMP_INLINE void
HttpAction<HttpRedirect>::initialize(const HttpServerOptions&)
{}

CPPWAMP_INLINE void HttpAction<HttpRedirect>::expect(HttpJob& job)
{
    return execute(job);
}

CPPWAMP_INLINE void HttpAction<HttpRedirect>::execute(HttpJob& job)
{
    boost::urls::url location = job.target();

    try
    {
        if (!properties_.scheme().empty())
            location.set_scheme(properties_.scheme());

        if (!properties_.authority().empty())
            location.set_encoded_authority(properties_.authority());
        else
            location.set_encoded_authority(job.host());

        if (!properties_.host().empty())
            location.set_host(properties_.host());

        if (properties_.hasPort())
            location.set_port_number(properties_.port());

        if (properties_.hasAlias())
        {
            // Substitute the route portion of the target with the alias
            auto routeLen = properties_.route().length();
            auto path = job.target().encoded_path();
            assert(path.length() >= routeLen);
            std::string newPath = properties_.alias();
            newPath += path.substr(routeLen, std::string::npos);
            location.set_encoded_path(newPath);
        }
    }
    catch (const boost::system::system_error& e)
    {
        job.fail(HttpDenial{HttpStatus::internalServerError}.withHtmlEnabled(),
                 e.code(), "HttpRedirect");
        return;
    }

    job.redirect(location.buffer(), properties_.status());
};


//******************************************************************************
// HttpAction<HttpWebsocketUpgrade>
//******************************************************************************

CPPWAMP_INLINE
HttpAction<HttpWebsocketUpgrade>::HttpAction(HttpWebsocketUpgrade properties)
    : properties_(properties)
{}

CPPWAMP_INLINE std::string HttpAction<HttpWebsocketUpgrade>::route() const
{
    return properties_.route();
}

CPPWAMP_INLINE void
HttpAction<HttpWebsocketUpgrade>::initialize(const HttpServerOptions&)
{}

CPPWAMP_INLINE void HttpAction<HttpWebsocketUpgrade>::expect(HttpJob& job)
{
    if (checkRequest(job))
        job.continueRequest();
}

CPPWAMP_INLINE void HttpAction<HttpWebsocketUpgrade>::execute(HttpJob& job)
{
    if (checkRequest(job))
        job.upgradeToWebsocket(properties_.options(), properties_.limits());
};

CPPWAMP_INLINE bool HttpAction<HttpWebsocketUpgrade>::checkRequest(HttpJob& job)
{
    if (!job.isWebsocketUpgrade())
    {
        auto denial = HttpDenial{HttpStatus::upgradeRequired}
            .withMessage("This service requires use of the Websocket protocol.")
            .withFields({{"Connection", "Upgrade"}, {"Upgrade", "websocket"}});
        job.reject(std::move(denial));
        return false;
    }

    return true;
}

} // namespace wamp
