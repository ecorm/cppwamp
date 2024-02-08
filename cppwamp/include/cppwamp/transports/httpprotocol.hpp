/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_HTTPPROTOCOL_HPP
#define CPPWAMP_TRANSPORTS_HTTPPROTOCOL_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains basic HTTP protocol definitions. */
//------------------------------------------------------------------------------

#include <string>
#include "../api.hpp"
#include "../utils/triemap.hpp"
#include "httpserveroptions.hpp"
#include "socketendpoint.hpp"
#include "tcpprotocol.hpp"
#include "../internal/passkey.hpp"
#include "../internal/polymorphichttpaction.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Tag type associated with the HTTP transport. */
//------------------------------------------------------------------------------
struct CPPWAMP_API Http
{
    constexpr Http() = default;
};


//------------------------------------------------------------------------------
/** Primary template for HTTP actions. */
//------------------------------------------------------------------------------
template <typename TOptions>
class HttpAction
{};


class HttpJob;

//------------------------------------------------------------------------------
/** Wrapper that type-erases a polymorphic HTTP action. */
//------------------------------------------------------------------------------
class CPPWAMP_API AnyHttpAction
{
public:
    /** Constructs an empty AnyHttpAction. */
    AnyHttpAction();

    /** Converting constructor taking action options. */
    template <typename TOptions>
    AnyHttpAction(TOptions o) // NOLINT(google-explicit-constructor)
        : action_(std::make_shared<internal::PolymorphicHttpAction<TOptions>>(
            std::move(o)))
    {}

    /** Returns false if the AnyHttpAction is empty. */
    explicit operator bool() const;

    /** Obtains the route associated with the action. */
    std::string route() const;

private:
    std::shared_ptr<internal::PolymorphicHttpActionInterface> action_;

public: // Internal use only
    void initialize(internal::PassKey, const HttpServerOptions& options);

    void expect(internal::PassKey, HttpJob& job);

    void execute(internal::PassKey, HttpJob& job);
};


//------------------------------------------------------------------------------
/** Contains the settings of an HTTP server block ("virtual host"). */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpServerBlock
{
public:
    /** Constructor taking a host name. */
    explicit HttpServerBlock(std::string hostName = {});

    /** Specifies the server options at the block level. */
    HttpServerBlock& withOptions(HttpServerOptions options);

    /** Adds an action associated with an exact route. */
    HttpServerBlock& addExactRoute(AnyHttpAction action);

    /** Adds an action associated with a prefix match route. */
    HttpServerBlock& addPrefixRoute(AnyHttpAction action);

    /** Host name for this server block. */
    const std::string& hostName() const;

    /** Obtains the server options. */
    const HttpServerOptions& options() const;

    /** Accesses the server options. */
    HttpServerOptions& options();

    /** Finds the best matching action associated with the given target. */
    template <typename TStringLike>
    AnyHttpAction* findAction(const TStringLike& target)
    {
        return doFindAction(target.data());
    }

private:
    AnyHttpAction* doFindAction(const char* route);

    utils::TrieMap<AnyHttpAction> actionsByExactKey_;
    utils::TrieMap<AnyHttpAction> actionsByPrefixKey_;
    HttpServerOptions options_;
    std::string hostName_;

public: // Internal use only
    void initialize(internal::PassKey, const HttpServerOptions& parentOptions);
};


//------------------------------------------------------------------------------
/** Contains limits for the HTTP server listener. */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpListenerLimits
{
public:
    HttpListenerLimits& withBacklogCapacity(int capacity);

    int backlogCapacity() const;

private:
    int backlogCapacity_ = 0; // Use Asio's default by default
};


//------------------------------------------------------------------------------
/** Contains HTTP server address information, as well as other socket options.
    Meets the requirements of @ref TransportSettings. */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpEndpoint
    : public SocketEndpoint<HttpEndpoint, Http, TcpOptions, HttpListenerLimits>
{
public:
    /// Transport protocol tag associated with these settings.
    using Protocol = Http;

    /// Numeric port type
    using Port = uint_least16_t;

    /** Constructor taking a port number. */
    explicit HttpEndpoint(Port port);

    /** Constructor taking an address string, port number. */
    HttpEndpoint(std::string address, unsigned short port);

    /** Specifies the default server block options. */
    HttpEndpoint& withOptions(HttpServerOptions options);

    /** Adds a server block. */
    HttpEndpoint& addBlock(HttpServerBlock block);

    /** Obtains the endpoint-level server options. */
    const HttpServerOptions& options() const;

    /** Accesses the endpoint-level server options. */
    HttpServerOptions& options();

    /** Finds the best-matching server block for the given host name. */
    HttpServerBlock* findBlock(std::string hostName);

    /** Generates a human-friendly string of the HTTP address/port. */
    std::string label() const;

private:
    using Base = SocketEndpoint<HttpEndpoint, Http, TcpOptions,
                                HttpListenerLimits>;

    static void toLowercase(std::string& str);

    std::map<std::string, HttpServerBlock> serverBlocks_;
    HttpServerOptions options_;

public: // Internal use only
    void initialize(internal::PassKey);
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/httpprotocol.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_HTTPPROTOCOL_HPP
