/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023-2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_HTTPJOB_HPP
#define CPPWAMP_TRANSPORTS_HTTPJOB_HPP

#include <boost/url.hpp>
#include "../erroror.hpp"
#include "httpresponse.hpp"
#include "websocketprotocol.hpp"
#include "../api.hpp"

namespace wamp
{

namespace internal { class HttpJobImpl; }

//------------------------------------------------------------------------------
class CPPWAMP_API HttpJob
{
public:
    using Url = boost::urls::url;

    template <typename TBody>
    using Response = boost::beast::http::response<TBody>;

    const Url& target() const;

    std::string method() const;

    const std::string& body() const &;

    std::string&& body() &&;

    ErrorOr<std::string> field(const std::string& key) const;

    std::string fieldOr(const std::string& key, std::string fallback) const;

    const std::string& hostName() const;

    bool isUpgrade() const;

    bool isWebsocketUpgrade() const;

    const HttpEndpoint& settings() const;

    void continueRequest();

    void respond(HttpResponse&& response);

    void websocketUpgrade(WebsocketOptions options,
                          const WebsocketServerLimits& limits);

    void deny(HttpDenial denial);

private:
    HttpJob(std::shared_ptr<internal::HttpJobImpl> impl);

    std::shared_ptr<internal::HttpJobImpl> impl_;

    friend class internal::HttpJobImpl;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/httpjob.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_HTTPJOB_HPP
