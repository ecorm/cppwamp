/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_HTTPENDPOINT_HPP
#define CPPWAMP_TRANSPORTS_HTTPENDPOINT_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for specifying HTTP server parameters and
           options. */
//------------------------------------------------------------------------------

#include <cassert>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include "../api.hpp"
#include "../utils/triemap.hpp"
#include "tcpprotocol.hpp"
#include "httpprotocol.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TOptions>
class CPPWAMP_API HttpAction
{};

//------------------------------------------------------------------------------
class CPPWAMP_API PolymorphicHttpActionInterface
{
public:
    virtual ~PolymorphicHttpActionInterface() = default;

    virtual void execute(const std::string& target) = 0;
};

//------------------------------------------------------------------------------
template <typename TOptions>
class CPPWAMP_API PolymorphicHttpAction : public PolymorphicHttpActionInterface
{
public:
    using Options = TOptions;

    PolymorphicHttpAction() = default;

    explicit PolymorphicHttpAction(Options options)
        : action_(std::move(options))
    {}

    virtual void execute(const std::string& target) override
    {
        action_.execute(target);
    };

private:
    HttpAction<Options> action_;
};

} // namespace internal

//------------------------------------------------------------------------------
/** Wrapper that type-erases a polymorphic HTTP action. */
//------------------------------------------------------------------------------
class CPPWAMP_API AnyHttpAction
{
public:
    /** Constructs an empty AnyCodec. */
    AnyHttpAction() = default;

    /** Converting constructor taking action options. */
    template <typename TOptions>
    AnyHttpAction( // NOLINT(google-explicit-constructor)
        TOptions o)
        : action_(std::make_shared<internal::PolymorphicHttpAction<TOptions>>(
            std::move(o)))
    {}

    /** Returns false if the AnyHttpAction is empty. */
    explicit operator bool() const {return action_ != nullptr;}

private:
    void execute(const std::string& target)
    {
        assert(action_ != nullptr);
        action_->execute(target);
    };

    std::shared_ptr<internal::PolymorphicHttpActionInterface> action_;
};


//------------------------------------------------------------------------------
/** Options for serving static files via HTTP. */
//------------------------------------------------------------------------------
class HttpServeStaticFile
{
public:
    explicit HttpServeStaticFile(std::string path) : path_(std::move(path)) {}

private:
    std::string path_;
};

//------------------------------------------------------------------------------
/** Options for upgrading an HTTP request to a Websocket connection. */
//------------------------------------------------------------------------------
class HttpWebsocketUpgrade
{
public:
    /** Specifies the maximum length permitted for incoming messages. */
    HttpWebsocketUpgrade& withMaxRxLength(std::size_t length)
    {
        maxRxLength_ = length;
        return *this;
    }

    /** Obtains the specified maximum incoming message length. */
    std::size_t maxRxLength() const {return maxRxLength_;}

private:
    std::size_t maxRxLength_ = 16*1024*1024;
};


namespace internal
{
//------------------------------------------------------------------------------
template <>
class HttpAction<HttpServeStaticFile>
{
public:
    HttpAction(HttpServeStaticFile options) : options_(options) {}

    void execute(const std::string& target)
    {
    };

private:
    HttpServeStaticFile options_;
};

//------------------------------------------------------------------------------
template <>
class HttpAction<HttpWebsocketUpgrade>
{
public:
    HttpAction(HttpWebsocketUpgrade options) : options_(options) {}

    void execute(const std::string& target)
    {
    };

private:
    HttpWebsocketUpgrade options_;
};
} // namespace internal

//------------------------------------------------------------------------------
/** Contains HTTP host address information, as well as other socket options. */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpEndpoint
{
public:
    /// URI and status code of an error page.
    struct ErrorPage
    {
        std::string uri;
        HttpStatus status;
    };

    /// Transport protocol tag associated with these settings.
    using Protocol = Http;

    /// Numeric port type
    using Port = uint_least16_t;

    /** Constructor taking a port number. */
    explicit HttpEndpoint(Port port);

    /** Constructor taking an address string, port number. */
    HttpEndpoint(std::string address, unsigned short port);

    /** Specifies the underlying TCP socket options to use. */
    HttpEndpoint& withSocketOptions(TcpOptions options);

    /** Specifies the maximum length permitted for incoming messages. */
    HttpEndpoint& withMaxRxLength(std::size_t length);

    /** Adds an action associated with an exact route. */
    HttpEndpoint& withExactRoute(std::string uri, AnyHttpAction action);

    /** Adds an action associated with a prefix match route. */
    HttpEndpoint& withPrefixRoute(std::string uri, AnyHttpAction action);

    /** Specifies the error page to show for the given HTTP response
        status code. */
    HttpEndpoint& withErrorPage(HttpStatus status, std::string uri);

    /** Specifies the error page to shown for the given HTTP response
        status code, with the original status code subsituted with the given
        status code. */
    HttpEndpoint& withErrorPage(HttpStatus status, std::string uri,
                                HttpStatus changedStatus);

    /** Obtains the endpoint address. */
    const std::string& address() const;

    /** Obtains the the port number. */
    Port port() const;

    /** Obtains the transport options. */
    const TcpOptions& options() const;

    /** Obtains the specified maximum incoming message length. */
    std::size_t maxRxLength() const;

    /** Generates a human-friendly string of the HTTP address/port. */
    std::string label() const;

    /** Finds the best matching action associated with the given route. */
    template <typename TStringLike>
    AnyHttpAction* findAction(const TStringLike& route) const
    {
        return doFindAction(route.data());
    }

    /** Finds the error page associated with the given HTTP status code. */
    const ErrorPage* findErrorPage(HttpStatus status) const;

private:
    AnyHttpAction* doFindAction(const char* route);

    utils::TrieMap<AnyHttpAction> actionsByExactKey_;
    utils::TrieMap<AnyHttpAction> actionsByPrefixKey_;
    std::map<HttpStatus, ErrorPage> errorPages_;
    std::string address_;
    TcpOptions options_;
    std::size_t maxRxLength_ = 16*1024*1024;
    Port port_ = 0;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/httpendpoint.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_HTTPENDPOINT_HPP
