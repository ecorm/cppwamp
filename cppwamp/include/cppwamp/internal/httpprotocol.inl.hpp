/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/httpprotocol.hpp"
#include <utility>
#include "../api.hpp"
#include "../exceptions.hpp"
#include "../version.hpp"

namespace wamp
{

//******************************************************************************
// HttpStatusCategory
//******************************************************************************

CPPWAMP_INLINE const char* HttpStatusCategory::name() const noexcept
{
    return "wamp::HttpStatusCategory";
}

CPPWAMP_INLINE std::string HttpStatusCategory::message(int ev) const
{
    using S = HttpStatus;
    switch (static_cast<HttpStatus>(ev))
    {
        case S::continueRequest:               return "100 Continue";
        case S::switchingProtocols:            return "101 Switching Protocols";
        case S::processing:                    return "102 Processing";
        case S::ok:                            return "200 OK";
        case S::created:                       return "201 Created";
        case S::accepted:                      return "202 Accepted";
        case S::nonAuthoritativeInformation:   return "203 Non-Authoritative Information";
        case S::noContent:                     return "204 No Content";
        case S::resetContent:                  return "205 Reset Content";
        case S::partialContent:                return "206 Partial Content";
        case S::multiStatus:                   return "207 Multi-Status";
        case S::alreadyReported:               return "208 Already Reported";
        case S::imUsed:                        return "226 IM Used";
        case S::multipleChoices:               return "300 Multiple Choices";
        case S::movedPermanently:              return "301 Moved Permanently";
        case S::found:                         return "302 Found";
        case S::seeOther:                      return "303 See Other";
        case S::notModified:                   return "304 Not Modified";
        case S::useProxy:                      return "305 Use Proxy";
        case S::temporaryRedirect:             return "307 Temporary Redirect";
        case S::permanentRedirect:             return "308 Permanent Redirect";
        case S::badRequest:                    return "400 Bad Request";
        case S::unauthorized:                  return "401 Unauthorized";
        case S::paymentRequired:               return "402 Payment Required";
        case S::forbidden:                     return "403 Forbidden";
        case S::notFound:                      return "404 Not Found";
        case S::methodNotAllowed:              return "405 Method Not Allowed";
        case S::notAcceptable:                 return "406 Not Acceptable";
        case S::proxyAuthenticationRequired:   return "407 Proxy Authentication Required";
        case S::requestTimeout:                return "408 Request Timeout";
        case S::conflict:                      return "409 Conflict";
        case S::gone:                          return "410 Gone";
        case S::lengthRequired:                return "411 Length Required";
        case S::preconditionFailed:            return "412 Precondition Failed";
        case S::payloadTooLarge:               return "413 Payload Too Large";
        case S::uriTooLong:                    return "414 URI Too Long";
        case S::unsupportedMediaType:          return "415 Unsupported Media Type";
        case S::rangeNotSatisfiable:           return "416 Range Not Satisfiable";
        case S::expectationFailed:             return "417 Expectation Failed";
        case S::misdirectedRequest:            return "421 Misdirected Request";
        case S::unprocessableEntity:           return "422 Unprocessable Entity";
        case S::locked:                        return "423 Locked";
        case S::failedDependency:              return "424 Failed Dependency";
        case S::upgradeRequired:               return "426 Upgrade Required";
        case S::preconditionRequired:          return "428 Precondition Required";
        case S::tooManyRequests:               return "429 Too Many Requests";
        case S::requestHeaderFieldsTooLarge:   return "431 Request Header Fields Too Large";
        case S::unavailableForLegalReasons:    return "451 Unavailable For Legal Reasons";
        case S::internalServerError:           return "500 Internal Server Error";
        case S::notImplemented:                return "501 Not Implemented";
        case S::badGateway:                    return "502 Bad Gateway";
        case S::serviceUnavailable:            return "503 Service Unavailable";
        case S::gatewayTimeout:                return "504 Gateway Timeout";
        case S::httpVersionNotSupported:       return "505 HTTP Version Not Supported";
        case S::variantAlsoNegotiates:         return "506 Variant Also Negotiates";
        case S::insufficientStorage:           return "507 Insufficient Storage";
        case S::loopDetected:                  return "508 Loop Detected";
        case S::notExtended:                   return "510 Not Extended";
        case S::networkAuthenticationRequired: return "511 Network Authentication Required";
        default: break;
    }

    return std::to_string(ev) + " Unknown Error";
}

CPPWAMP_INLINE bool HttpStatusCategory::equivalent(
    const std::error_code& code, int condition) const noexcept
{
    return (code.category() == HttpStatusCategory()) &&
           (code.value() == condition);
}

CPPWAMP_INLINE HttpStatusCategory::HttpStatusCategory() = default;

CPPWAMP_INLINE HttpStatusCategory& httpStatusCategory()
{
    static HttpStatusCategory instance;
    return instance;
}

CPPWAMP_INLINE std::error_code make_error_code(HttpStatus errc)
{
    return {static_cast<int>(errc), httpStatusCategory()};
}

CPPWAMP_INLINE std::error_condition
make_error_condition(HttpStatus errc)
{
    return {static_cast<int>(errc), httpStatusCategory()};
}


//******************************************************************************
// HttpServerLimits
//******************************************************************************

CPPWAMP_INLINE HttpServerLimits&
HttpServerLimits::withHttpRequestHeaderSize(std::size_t n)
{
    Base::withHttpRequestHeaderSize(n);
    return *this;
}

CPPWAMP_INLINE HttpServerLimits&
HttpServerLimits::withHttpRequestBodySize(std::size_t n)
{
    requestBodySize_ = n;
    return *this;
}

CPPWAMP_INLINE HttpServerLimits&
HttpServerLimits::withHttpRequestBodyIncrement(std::size_t n)
{
    requestBodyIncrement_ = n;
    return *this;
}

/** @note Boost.Beast will clamp this to `BOOST_BEAST_FILE_BUFFER_SIZE=4096`
          for `file_body` responses. */
CPPWAMP_INLINE HttpServerLimits&
HttpServerLimits::withHttpResponseIncrement(std::size_t n)
{
    responseIncrement_ = n;
    return *this;
}

CPPWAMP_INLINE HttpServerLimits&
HttpServerLimits::withHttpRequestHeaderTimeout(Timeout t)
{
    keepaliveTimeout_ = internal::checkTimeout(t);
    return *this;
}

CPPWAMP_INLINE HttpServerLimits&
HttpServerLimits::withHttpRequestBodyTimeout(IncrementalTimeout t)
{
    requestBodyTimeout_ = t.validate();
    return *this;
}

CPPWAMP_INLINE HttpServerLimits&
HttpServerLimits::withHttpResponseTimeout(IncrementalTimeout t)
{
    responseTimeout_ = t.validate();
    return *this;
}

CPPWAMP_INLINE HttpServerLimits&
HttpServerLimits::withHttpKeepaliveTimeout(Timeout t)
{
    keepaliveTimeout_ = internal::checkTimeout(t);
    return *this;
}

CPPWAMP_INLINE std::size_t HttpServerLimits::httpRequestHeaderSize() const
{
    return Base::httpRequestHeaderSize();
}

CPPWAMP_INLINE std::size_t HttpServerLimits::httpRequestBodySize() const
{
    return requestBodySize_;
}

CPPWAMP_INLINE std::size_t HttpServerLimits::httpRequestBodyIncrement() const
{
    return requestBodyIncrement_;
}

CPPWAMP_INLINE std::size_t HttpServerLimits::httpResponseIncrement() const
{
    return responseIncrement_;
}

CPPWAMP_INLINE Timeout HttpServerLimits::httpRequestHeaderTimeout() const
{
    return requestHeaderTimeout_;
}

CPPWAMP_INLINE const IncrementalTimeout&
HttpServerLimits::httpBodyTimeout() const
{
    return requestBodyTimeout_;
}

CPPWAMP_INLINE const IncrementalTimeout&
HttpServerLimits::httpResponseTimeout() const
{
    return responseTimeout_;
}

CPPWAMP_INLINE Timeout HttpServerLimits::httpKeepaliveTimeout() const
{
    return keepaliveTimeout_;
}

CPPWAMP_INLINE WebsocketServerLimits HttpServerLimits::toWebsocket() const
{
    // Intentionally slice
    return static_cast<WebsocketServerLimits>(*this);
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
HttpFileServingOptions::applyFallback(const HttpFileServingOptions& opts)
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
// HttpEndpoint
//******************************************************************************

CPPWAMP_INLINE HttpEndpoint::HttpEndpoint(Port port)
    : HttpEndpoint("", port)
{}

CPPWAMP_INLINE HttpEndpoint::HttpEndpoint(std::string address,
                                          unsigned short port)
    : Base(std::move(address), port),
      fileServingOptions_(defaultFileServingOptions()),
      agent_(Version::serverAgentString())

{
    mutableAcceptorOptions().withReuseAddress(true);
    fileServingOptions_.withIndexFileName("index.html")
                       .withAutoIndex(false);
#ifdef _WIN32
    fileServingOptions_.withDocumentRoot("C:/web/html");
#else
    fileServingOptions_.withDocumentRoot("/var/wwww/html");
#endif
}

CPPWAMP_INLINE HttpEndpoint&
HttpEndpoint::withFileServingOptions(HttpFileServingOptions options)
{
    fileServingOptions_ = std::move(options);
    fileServingOptions_.applyFallback(defaultFileServingOptions());
    return *this;
}

CPPWAMP_INLINE HttpEndpoint& HttpEndpoint::withAgent(std::string agent)
{
    agent_ = std::move(agent);
    return *this;
}

CPPWAMP_INLINE HttpEndpoint& HttpEndpoint::withLimits(HttpServerLimits limits)
{
    limits_ = limits;
    return *this;
}

/** @pre `static_cast<unsigned>(page) >= 400`
    @pre `!uri.empty` */
CPPWAMP_INLINE HttpEndpoint& HttpEndpoint::addErrorPage(HttpErrorPage page)
{
    auto key = page.key();
    errorPages_[key] = std::move(page);
    return *this;
}

CPPWAMP_INLINE HttpEndpoint& HttpEndpoint::addExactRoute(AnyHttpAction action)
{
    auto key = action.route();
    actionsByExactKey_[std::move(key)] = std::move(action);
    return *this;
}

CPPWAMP_INLINE HttpEndpoint& HttpEndpoint::addPrefixRoute(AnyHttpAction action)
{
    auto key = action.route();
    actionsByPrefixKey_[std::move(key)] = std::move(action);
    return *this;
}

CPPWAMP_INLINE const HttpFileServingOptions&
HttpEndpoint::fileServingOptions() const
{
    return fileServingOptions_;
}

CPPWAMP_INLINE const std::string& HttpEndpoint::agent() const {return agent_;}

CPPWAMP_INLINE const HttpServerLimits& HttpEndpoint::limits() const
{
    return limits_;
}

CPPWAMP_INLINE HttpServerLimits& HttpEndpoint::limits() {return limits_;}

CPPWAMP_INLINE std::string HttpEndpoint::label() const
{
    auto portString = std::to_string(port());
    if (address().empty())
        return "HTTP Port " + portString;
    return "HTTP " + address() + ':' + portString;
}

CPPWAMP_INLINE const HttpErrorPage*
HttpEndpoint::findErrorPage(HttpStatus status) const
{
    auto found = errorPages_.find(status);
    return found == errorPages_.end() ? nullptr : &(found->second);
}

CPPWAMP_INLINE const HttpFileServingOptions&
HttpEndpoint::defaultFileServingOptions()
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

CPPWAMP_INLINE AnyHttpAction* HttpEndpoint::doFindAction(const char* target)
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

} // namespace wamp
