/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_HTTPSERVEROPTIONS_HPP
#define CPPWAMP_TRANSPORTS_HTTPSERVEROPTIONS_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains basic HTTP protocol definitions. */
//------------------------------------------------------------------------------

#include <functional>
#include <string>
#include "../api.hpp"
#include "../transportlimits.hpp"
#include "httpstatus.hpp"
#include "../internal/polymorphichttpaction.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains limits for HTTP server blocks. */
// TODO: Keep-alive enable/disable
//------------------------------------------------------------------------------
class CPPWAMP_API HttpServerLimits
{
public:
    static const HttpServerLimits& defaults();

    HttpServerLimits& withRequestHeaderSize(std::size_t n);

    HttpServerLimits& withRequestBodySize(std::size_t n);

    HttpServerLimits& withRequestBodyIncrement(std::size_t n);

    HttpServerLimits& withResponseIncrement(std::size_t n);

    std::size_t requestHeaderSize() const;

    std::size_t requestBodySize() const;

    std::size_t requestBodyIncrement() const;

    std::size_t responseIncrement() const;

    void merge(const HttpServerLimits& limits);

private:
    template <typename T, typename U>
    void doMerge(T& member, T limit, U nullValue);

    std::size_t requestHeaderSize_ = 0;
    std::size_t requestBodySize_ = 0;
    std::size_t requestBodyIncrement_ = 0;
    std::size_t responseIncrement_ = 0;
};


//------------------------------------------------------------------------------
/** Contains timeouts for HTTP server blocks. */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpServerTimeouts
{
public:
    static const HttpServerTimeouts& defaults();

    HttpServerTimeouts& withRequestHeaderTimeout(Timeout t);

    HttpServerTimeouts& withRequestBodyTimeout(IncrementalTimeout t);

    HttpServerTimeouts& withResponseTimeout(IncrementalTimeout t);

    HttpServerTimeouts& withKeepaliveTimeout(Timeout t);

    HttpServerTimeouts& withLingerTimeout(Timeout t);

    Timeout requestHeaderTimeout() const;

    const IncrementalTimeout& requestBodyTimeout() const;

    const IncrementalTimeout& responseTimeout() const;

    Timeout keepaliveTimeout() const;

    Timeout lingerTimeout() const;

    void merge(const HttpServerTimeouts& limits);

private:
    template <typename T, typename U>
    void doMerge(T& member, T limit, U nullValue);

    IncrementalTimeout responseTimeout_;
    IncrementalTimeout requestBodyTimeout_;
    Timeout requestHeaderTimeout_ = unspecifiedTimeout;
    Timeout keepaliveTimeout_ = unspecifiedTimeout;
    Timeout lingerTimeout_ = unspecifiedTimeout;
};


//------------------------------------------------------------------------------
/** Contains HTTP error page information. */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpErrorPage
{
public:
    using Generator = std::function<std::string (HttpStatus status,
                                                 const std::string& what)>;

    HttpErrorPage();

    /** Specifies a file path or redirect URL to be associated with the given
        key HTTP status code, with the key status optionally substituted with
        the given status. */
    HttpErrorPage(HttpStatus key, std::string uri,
                  HttpStatus status = HttpStatus::none);

    /** Specifies a status code that substitutes the given key HTTP
        status code. */
    HttpErrorPage(HttpStatus key, HttpStatus status);

    /** Specifies a function that generates an HTML page for the given HTTP
        status code, with the key status optionally substituted with
        the given status. */
    HttpErrorPage(HttpStatus key, Generator generator,
                  HttpStatus status = HttpStatus::none);

    HttpErrorPage& withCharset(std::string charset);

    HttpStatus key() const;

    HttpStatus status() const;

    const std::string& uri() const;

    const std::string& charset() const;

    const Generator& generator() const;

    bool isRedirect() const;

private:
    std::string uri_;
    std::string charset_;
    Generator generator_;
    HttpStatus key_ = HttpStatus::none;
    HttpStatus status_ = HttpStatus::none;
};


//------------------------------------------------------------------------------
/** Function type used to associate MIME types with file extensions.
    It should return an empty string if the extension unknown. */
//------------------------------------------------------------------------------
using MimeTypeMapper = std::function<std::string (const std::string& ext)>;

//------------------------------------------------------------------------------
/** Common options for serving static files via HTTP. */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpFileServingOptions
{
public:
    /** Obtains the default file serving options. */
    static const HttpFileServingOptions& defaults();

    /** Obtains the default MIME type associated with the given file
        extension. */
    static std::string defaultMimeType(const std::string& extension);

    /** Specifies the document root path for serving files. */
    HttpFileServingOptions& withDocumentRoot(std::string root);

    /** Specifies the charset to use when not specified by the MIME
        type mapper. */
    HttpFileServingOptions& withCharset(std::string charset);

    /** Specifies the index file name. */
    HttpFileServingOptions& withIndexFileName(std::string name);

    /** Enables automatic directory listing. */
    HttpFileServingOptions& withAutoIndex(bool enabled = true);

    /** Specifies the mapping function for determining MIME type based on
        file extension. */
    HttpFileServingOptions& withMimeTypes(MimeTypeMapper f);

    /** Obtains the document root path for serving files. */
    const std::string& documentRoot() const;

    /** Obtains the charset to use when not specified by the MIME
        type mapper. */
    const std::string& charset() const;

    /** Obtains the index filename. */
    const std::string& indexFileName() const;

    /** Determines if automatic directory listing is enabled. */
    bool autoIndex() const;

    /** Determines if a mapping function was specified. */
    bool hasMimeTypeMapper() const;

    /** Obtains the MIME type associated with the given file extension. */
    std::string lookupMimeType(const std::string& extension) const;

    /** Applies the given options as defaults for members that are not set. */
    void merge(const HttpFileServingOptions& opts);

private:
    static char toLower(char c);

    std::map<HttpStatus, HttpErrorPage> errorPages_;
    std::string documentRoot_;
    std::string charset_;
    std::string indexFileName_;
    MimeTypeMapper mimeTypeMapper_;
    bool autoIndex_ = false;
    bool hasAutoIndex_ = false;
};

//------------------------------------------------------------------------------
/** Contains the settings for an HTTP server block ("virtual host"). */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpServerOptions
{
public:
    /** Obtains the default server options. */
    static const HttpServerOptions& defaults();

    /** Specifies the agent string to use for the HTTP response 'Server' field
        (default is Version::serverAgentString). */
    HttpServerOptions& withAgent(std::string agent);

    /** Specifies the default file serving options. */
    HttpServerOptions& withFileServingOptions(HttpFileServingOptions options);

    /** Specifies HTTP message limits. */
    HttpServerOptions& withLimits(HttpServerLimits limits);

    /** Specifies HTTP message timeouts. */
    HttpServerOptions& withTimeouts(HttpServerTimeouts timeouts);

    /** Specifies the error page to show for the given HTTP response
        status code. */
    HttpServerOptions& addErrorPage(HttpErrorPage page);

    /** Obtains the custom agent string. */
    const std::string& agent() const;

    /** Obtains default file serving options. */
    const HttpFileServingOptions& fileServingOptions() const;

    /** Accesses the default file serving options. */
    HttpFileServingOptions& fileServingOptions();

    /** Obtains the HTTP message limits. */
    const HttpServerLimits& limits() const;

    /** Accesses the HTTP message limits and timeouts. */
    HttpServerLimits& limits();

    /** Obtains the HTTP message timeouts. */
    const HttpServerTimeouts& timeouts() const;

    /** Accesses the HTTP message timeouts. */
    HttpServerTimeouts& timeouts();

    /** Finds the error page associated with the given HTTP status code. */
    const HttpErrorPage* findErrorPage(HttpStatus status) const;

    /** Merges the given options onto those that have not been set. */
    void merge(const HttpServerOptions& options);

private:
    std::map<HttpStatus, HttpErrorPage> errorPages_;
    HttpFileServingOptions fileServingOptions_;
    HttpServerTimeouts timeouts_;
    HttpServerLimits limits_;
    std::string agent_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/httpserveroptions.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_HTTPSERVEROPTIONS_HPP
