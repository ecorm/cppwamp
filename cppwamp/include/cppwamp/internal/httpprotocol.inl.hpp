/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/httpprotocol.hpp"
#include <cctype>
#include <utility>
#include "../api.hpp"
#include "../exceptions.hpp"
#include "../version.hpp"

namespace wamp
{

//******************************************************************************
// HttpServerLimits
//******************************************************************************

CPPWAMP_INLINE const HttpServerLimits& HttpServerLimits::defaults()
{
    using std::chrono::seconds;

    static const auto limits = HttpServerLimits{}
        .withRequestHeaderSize(8192)    // Default for Boost.Beast and NGINX
        .withRequestBodySize(1024*1024) // Default for Boost.Beast and NGINX
        .withRequestBodyIncrement(4096) // Using Linux page size
        .withResponseIncrement(4096);   // Using Linux page size

    return limits;
}

CPPWAMP_INLINE HttpServerLimits&
HttpServerLimits::withRequestHeaderSize(std::size_t n)
{
    requestHeaderSize_ = n;
    return *this;
}

CPPWAMP_INLINE HttpServerLimits&
HttpServerLimits::withRequestBodySize(std::size_t n)
{
    requestBodySize_ = n;
    return *this;
}

CPPWAMP_INLINE HttpServerLimits&
HttpServerLimits::withRequestBodyIncrement(std::size_t n)
{
    requestBodyIncrement_ = n;
    return *this;
}

/** @note Boost.Beast will clamp this to `BOOST_BEAST_FILE_BUFFER_SIZE=4096`
          for `file_body` responses. */
CPPWAMP_INLINE HttpServerLimits&
HttpServerLimits::withResponseIncrement(std::size_t n)
{
    responseIncrement_ = n;
    return *this;
}

CPPWAMP_INLINE std::size_t HttpServerLimits::requestHeaderSize() const
{
    return requestHeaderSize_;
}

CPPWAMP_INLINE std::size_t HttpServerLimits::requestBodySize() const
{
    return requestBodySize_;
}

CPPWAMP_INLINE std::size_t
HttpServerLimits::requestBodyIncrement() const
{
    return requestBodyIncrement_;
}

CPPWAMP_INLINE std::size_t HttpServerLimits::responseIncrement() const
{
    return responseIncrement_;
}

CPPWAMP_INLINE void
HttpServerLimits::merge(const HttpServerLimits& limits)
{
    doMerge(requestHeaderSize_,    limits.requestHeaderSize_,    0);
    doMerge(requestBodySize_,      limits.requestBodySize_,      0);
    doMerge(requestBodyIncrement_, limits.requestBodyIncrement_, 0);
    doMerge(responseIncrement_,    limits.responseIncrement_,    0);
}

template <typename T, typename U>
void HttpServerLimits::doMerge(T& member, T limit, U nullValue)
{
    if (member == nullValue)
        member = limit;
}


//******************************************************************************
// HttpServerTimeouts
//******************************************************************************

CPPWAMP_INLINE const HttpServerTimeouts& HttpServerTimeouts::defaults()
{
    using std::chrono::seconds;

    static const auto limits = HttpServerTimeouts{}
        // Using Apache's maxinum RequestReadTimeout for headers
        .withRequestHeaderTimeout(seconds{40})

        // Using Apache's RequestReadTimeout, with 1/8 of ADSL2 5Mbps rate
        .withResponseTimeout({seconds{20}, 80*1024})

        // Using Apache's RequestReadTimeout, with ~1/4 of ADSL2 0.8Mbps rate
        .withRequestBodyTimeout({seconds{20}, 24*1024})

        // NGINX's keepalive_timeout of 75s
        // Apache default: 5s
        // Browser defaults: Firefox: 115s, IE: 60s, Chromium: never
        .withKeepaliveTimeout(seconds{75});

    return limits;
}

CPPWAMP_INLINE HttpServerTimeouts&
HttpServerTimeouts::withRequestHeaderTimeout(Timeout t)
{
    keepaliveTimeout_ = internal::checkTimeout(t);
    return *this;
}

CPPWAMP_INLINE HttpServerTimeouts&
HttpServerTimeouts::withRequestBodyTimeout(IncrementalTimeout t)
{
    requestBodyTimeout_ = t.validate();
    return *this;
}

CPPWAMP_INLINE HttpServerTimeouts&
HttpServerTimeouts::withResponseTimeout(IncrementalTimeout t)
{
    responseTimeout_ = t.validate();
    return *this;
}

CPPWAMP_INLINE HttpServerTimeouts&
HttpServerTimeouts::withKeepaliveTimeout(Timeout t)
{
    keepaliveTimeout_ = internal::checkTimeout(t);
    return *this;
}

CPPWAMP_INLINE Timeout HttpServerTimeouts::requestHeaderTimeout() const
{
    return requestHeaderTimeout_;
}

CPPWAMP_INLINE const IncrementalTimeout&
HttpServerTimeouts::requestBodyTimeout() const
{
    return requestBodyTimeout_;
}

CPPWAMP_INLINE const IncrementalTimeout&
HttpServerTimeouts::responseTimeout() const
{
    return responseTimeout_;
}

CPPWAMP_INLINE Timeout HttpServerTimeouts::keepaliveTimeout() const
{
    return keepaliveTimeout_;
}

CPPWAMP_INLINE Timeout HttpServerTimeouts::lingerTimeout() const
{
    return lingerTimeout_;
}

CPPWAMP_INLINE void
HttpServerTimeouts::merge(const HttpServerTimeouts& limits)
{
    if (!responseTimeout_.isSpecified())
        responseTimeout_ = limits.responseTimeout_;
    if (!requestBodyTimeout_.isSpecified())
        responseTimeout_ = limits.requestBodyTimeout_;
    doMerge(requestHeaderTimeout_, limits.requestHeaderTimeout_, unspecifiedTimeout);
    doMerge(keepaliveTimeout_,     limits.keepaliveTimeout_,     unspecifiedTimeout);
}

template <typename T, typename U>
void HttpServerTimeouts::doMerge(T& member, T limit, U nullValue)
{
    if (member == nullValue)
        member = limit;
}


//******************************************************************************
// HttpErrorPage
//******************************************************************************

CPPWAMP_INLINE HttpErrorPage::HttpErrorPage() = default;

/** @pre `static_cast<unsigned>(key) >= 400`
    @pre `!uri.empty`
    @pre `static_cast<unsigned>(status) >= 300` for absolute URI
    @pre `static_cast<unsigned>(status) < 400` for absolute URI
    @pre `static_cast<unsigned>(status) >= 400` for relative URI */
CPPWAMP_INLINE HttpErrorPage::HttpErrorPage(HttpStatus key, std::string uri,
                                            HttpStatus status)
    : uri_(std::move(uri)),
      key_(key),
      status_(status)
{
    CPPWAMP_LOGIC_CHECK(static_cast<unsigned>(key) >= 400,
                        "'key' must be an error code");
    CPPWAMP_LOGIC_CHECK(!uri_.empty(), "'uri' cannot be empty");

    if (status_ == HttpStatus::none)
    {
        status_ = (uri_.front() == '/') ? key : HttpStatus::movedPermanently;
        return;
    }

    auto n = static_cast<unsigned>(status);
    if (uri_.front() == '/')
    {
        CPPWAMP_LOGIC_CHECK(
            n >= 400, "'status' must be an error code for relative URI");
    }
    else
    {
        CPPWAMP_LOGIC_CHECK(
            n >= 300 && n < 400,
            "'status' must be a redirect code for absolute URI");
    }
}

/** @pre `static_cast<unsigned>(status) >= 400`
    @pre `static_cast<unsigned>(newStatus) >= 400` */
CPPWAMP_INLINE HttpErrorPage::HttpErrorPage(HttpStatus key, HttpStatus status)
    : key_(key),
      status_(status)
{
    CPPWAMP_LOGIC_CHECK(static_cast<unsigned>(key) >= 400,
                        "'key' must be an error code");
    CPPWAMP_LOGIC_CHECK(static_cast<unsigned>(key) >= 400,
                        "'status' must be an error code");
}

CPPWAMP_INLINE HttpErrorPage::HttpErrorPage(HttpStatus key, Generator generator,
                                            HttpStatus status)
    : generator_(std::move(generator)),
      key_(key),
      status_(status)
{
    if (status_ == HttpStatus::none)
        status_ = key;

    CPPWAMP_LOGIC_CHECK(static_cast<unsigned>(key) >= 400,
                        "'key' must be an error code");

    CPPWAMP_LOGIC_CHECK(static_cast<unsigned>(key) >= 400,
                        "'status' must be an error code");
}

CPPWAMP_INLINE HttpErrorPage& HttpErrorPage::withCharset(std::string charset)
{
    charset_ = charset;
    return *this;
}

CPPWAMP_INLINE HttpStatus HttpErrorPage::key() const {return key_;}

CPPWAMP_INLINE HttpStatus HttpErrorPage::status() const {return status_;}

CPPWAMP_INLINE const std::string& HttpErrorPage::uri() const {return uri_;}

CPPWAMP_INLINE const std::string& HttpErrorPage::charset() const
{
    return charset_;
}

CPPWAMP_INLINE const HttpErrorPage::Generator& HttpErrorPage::generator() const
{
    return generator_;
}

CPPWAMP_INLINE bool HttpErrorPage::isRedirect() const
{
    auto n = static_cast<unsigned>(status_);
    return (n >= 300) && (n < 400);
}


//******************************************************************************
// HttpFileServingOptions
//******************************************************************************

CPPWAMP_INLINE const HttpFileServingOptions& HttpFileServingOptions::defaults()
{
    static const auto options = HttpFileServingOptions{}
            .withIndexFileName("index.html")
            .withAutoIndex(false)
#ifdef _WIN32
            .withDocumentRoot("C:/web/html");
#else
            .withDocumentRoot("/var/wwww/html");
#endif

    return options;
}

CPPWAMP_INLINE std::string
HttpFileServingOptions::defaultMimeType(const std::string& extension)
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

/** `/var/www/html` (or `C:/web/html` on Windows) is the default if
    unspecified and uninherited. */
CPPWAMP_INLINE HttpFileServingOptions&
HttpFileServingOptions::withDocumentRoot(std::string documentRoot)
{
    CPPWAMP_LOGIC_CHECK(!documentRoot.empty(), "Document root cannot be empty");
    documentRoot_ = std::move(documentRoot);
    return *this;
}

CPPWAMP_INLINE HttpFileServingOptions&
HttpFileServingOptions::withCharset(std::string charset)
{
    charset_ = std::move(charset);
    return *this;
}

/** `index.html` is the default if unspecified and uninherited. */
CPPWAMP_INLINE HttpFileServingOptions&
HttpFileServingOptions::withIndexFileName(std::string name)
{
    CPPWAMP_LOGIC_CHECK(!name.empty(), "Index filename cannot be empty");
    indexFileName_ = std::move(name);
    return *this;
}

CPPWAMP_INLINE HttpFileServingOptions&
HttpFileServingOptions::withAutoIndex(bool enabled)
{
    autoIndex_ = enabled;
    hasAutoIndex_ = true;
    return *this;
}

CPPWAMP_INLINE HttpFileServingOptions&
HttpFileServingOptions::withMimeTypes(MimeTypeMapper f)
{
    mimeTypeMapper_ = std::move(f);
    return *this;
}

CPPWAMP_INLINE const std::string& HttpFileServingOptions::documentRoot() const
{
    return documentRoot_;
}

CPPWAMP_INLINE const std::string& HttpFileServingOptions::charset() const
{
    return charset_;
}

CPPWAMP_INLINE const std::string& HttpFileServingOptions::indexFileName() const
{
    return indexFileName_;
}

CPPWAMP_INLINE bool HttpFileServingOptions::autoIndex() const
{
    return autoIndex_;
}

CPPWAMP_INLINE bool HttpFileServingOptions::hasMimeTypeMapper() const
{
    return mimeTypeMapper_ != nullptr;
}

CPPWAMP_INLINE std::string
HttpFileServingOptions::lookupMimeType(const std::string& extension) const
{
    std::string ext{extension};
    for (auto& c: ext)
        c = toLower(c);
    return hasMimeTypeMapper() ? mimeTypeMapper_(extension)
                               : defaultMimeType(extension);
}

CPPWAMP_INLINE void
HttpFileServingOptions::merge(const HttpFileServingOptions& opts)
{
    if (documentRoot_.empty())
        documentRoot_ = opts.documentRoot_;
    if (charset_.empty())
        charset_ = opts.charset_;
    if (indexFileName_.empty())
        indexFileName_ = opts.indexFileName_;
    if (mimeTypeMapper_ == nullptr)
        mimeTypeMapper_ = opts.mimeTypeMapper_;
    if (!hasAutoIndex_)
        autoIndex_ = opts.autoIndex_;
}

CPPWAMP_INLINE char HttpFileServingOptions::toLower(char c)
{
    static constexpr unsigned offset = 'a' - 'A';
    if (c >= 'A' && c <= 'Z')
        c += offset;
    return c;
}


//******************************************************************************
// HttpServerOptions
//******************************************************************************

CPPWAMP_INLINE const HttpServerOptions& HttpServerOptions::defaults()
{
    static const auto options = HttpServerOptions{}
        .withFileServingOptions(HttpFileServingOptions::defaults())
        .withLimits(HttpServerLimits::defaults())
        .withAgent(Version::serverAgentString());

    return options;
}

CPPWAMP_INLINE HttpServerOptions& HttpServerOptions::withAgent(std::string agent)
{
    agent_ = std::move(agent);
    return *this;
}

CPPWAMP_INLINE HttpServerOptions&
HttpServerOptions::withFileServingOptions(HttpFileServingOptions options)
{
    fileServingOptions_ = std::move(options);
    fileServingOptions_.merge(HttpFileServingOptions::defaults());
    return *this;
}

CPPWAMP_INLINE HttpServerOptions&
HttpServerOptions::withLimits(HttpServerLimits limits)
{
    limits_ = limits;
    return *this;
}

CPPWAMP_INLINE HttpServerOptions&
HttpServerOptions::withTimeouts(HttpServerTimeouts timeouts)
{
    timeouts_ = timeouts;
    return *this;
}

CPPWAMP_INLINE HttpServerOptions&
HttpServerOptions::addErrorPage(HttpErrorPage page)
{
    auto key = page.key();
    errorPages_[key] = std::move(page);
    return *this;
}

CPPWAMP_INLINE const std::string& HttpServerOptions::agent() const
{
    return agent_;
}

CPPWAMP_INLINE const HttpFileServingOptions&
HttpServerOptions::fileServingOptions() const
{
    return fileServingOptions_;
}

CPPWAMP_INLINE const HttpServerLimits& HttpServerOptions::limits() const
{
    return limits_;
}

CPPWAMP_INLINE HttpServerLimits& HttpServerOptions::limits() {return limits_;}

CPPWAMP_INLINE const HttpServerTimeouts& HttpServerOptions::timeouts() const
{
    return timeouts_;
}

CPPWAMP_INLINE HttpServerTimeouts& HttpServerOptions::timeouts()
{
    return timeouts_;
}

CPPWAMP_INLINE const HttpErrorPage*
HttpServerOptions::findErrorPage(HttpStatus status) const
{
    auto found = errorPages_.find(status);
    return found == errorPages_.end() ? nullptr : &(found->second);
}

CPPWAMP_INLINE void
HttpServerOptions::merge(const HttpServerOptions& options)
{
    fileServingOptions_.merge(options.fileServingOptions());
    limits_.merge(options.limits());
    if (agent_.empty())
        agent_ = options.agent();
}


//******************************************************************************
// HttpServerBlock
//******************************************************************************

CPPWAMP_INLINE HttpServerBlock::HttpServerBlock(std::string hostName)
    : hostName_(std::move(hostName))
{}

CPPWAMP_INLINE HttpServerBlock&
HttpServerBlock::withOptions(HttpServerOptions options)
{
    options_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE HttpServerBlock&
HttpServerBlock::addExactRoute(AnyHttpAction action)
{
    auto key = action.route();
    actionsByExactKey_[std::move(key)] = std::move(action);
    return *this;
}

CPPWAMP_INLINE HttpServerBlock&
HttpServerBlock::addPrefixRoute(AnyHttpAction action)
{
    auto key = action.route();
    actionsByPrefixKey_[std::move(key)] = std::move(action);
    return *this;
}

CPPWAMP_INLINE const std::string& HttpServerBlock::hostName() const
{
    return hostName_;
}

CPPWAMP_INLINE const HttpServerOptions& HttpServerBlock::options() const
{
    return options_;
}

CPPWAMP_INLINE HttpServerOptions& HttpServerBlock::options() {return options_;}

CPPWAMP_INLINE AnyHttpAction* HttpServerBlock::doFindAction(const char* target)
{
    {
        auto found = actionsByExactKey_.find(target);
        if (found != actionsByExactKey_.end())
            return &(found.value());
    }

    {
        auto found = actionsByPrefixKey_.longest_prefix(target);
        if (found != actionsByPrefixKey_.end())
            return &(found.value());
    }

    return nullptr;
}

CPPWAMP_INLINE void HttpServerBlock::initialize(
    internal::PassKey, const HttpServerOptions& parentOptions)
{
    options_.merge(parentOptions);
    for (auto& a: actionsByExactKey_)
        a.initialize({}, options_);
    for (auto& a: actionsByPrefixKey_)
        a.initialize({}, options_);
}


//******************************************************************************
// HttpListenerLimits
//******************************************************************************

HttpListenerLimits& HttpListenerLimits::withBacklogCapacity(int capacity)
{
    backlogCapacity_ = capacity;
    return *this;
}

int HttpListenerLimits::backlogCapacity() const {return backlogCapacity_;}


//******************************************************************************
// HttpEndpoint
//******************************************************************************

CPPWAMP_INLINE HttpEndpoint::HttpEndpoint(Port port)
    : HttpEndpoint("", port)
{}

CPPWAMP_INLINE HttpEndpoint::HttpEndpoint(std::string address,
                                          unsigned short port)
    : Base(std::move(address), port)

{
    mutableAcceptorOptions().withReuseAddress(true);
}

CPPWAMP_INLINE HttpEndpoint&
HttpEndpoint::withOptions(HttpServerOptions options)
{
    options_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE HttpEndpoint& HttpEndpoint::addBlock(HttpServerBlock block)
{
    auto key = block.hostName();
    toLowercase(key);
    serverBlocks_[std::move(key)] = std::move(block);
    return *this;
}

CPPWAMP_INLINE const HttpServerOptions& HttpEndpoint::options() const
{
    return options_;
}

CPPWAMP_INLINE HttpServerOptions& HttpEndpoint::options() {return options_;}

CPPWAMP_INLINE HttpServerBlock* HttpEndpoint::findBlock(std::string hostName)
{
    toLowercase(hostName);
    auto found = serverBlocks_.find(hostName);
    if (found != serverBlocks_.end())
        return &found->second;

    static const std::string empty;
    found = serverBlocks_.find(empty);
    if (found != serverBlocks_.end())
        return &found->second;

    return nullptr;
}

CPPWAMP_INLINE std::string HttpEndpoint::label() const
{
    auto portString = std::to_string(port());
    if (address().empty())
        return "HTTP Port " + portString;
    return "HTTP " + address() + ':' + portString;
}

CPPWAMP_INLINE void HttpEndpoint::toLowercase(std::string& str)
{
    for (auto& c: str)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

CPPWAMP_INLINE void HttpEndpoint::initialize(internal::PassKey)
{
    options_.merge(HttpServerOptions::defaults());
    for (auto& kv: serverBlocks_)
        kv.second.initialize({}, options_);
}

} // namespace wamp
