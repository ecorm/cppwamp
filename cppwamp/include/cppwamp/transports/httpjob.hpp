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
#include "httpserveroptions.hpp"
#include "websocketprotocol.hpp"
#include "../api.hpp"
#include "../traits.hpp"

namespace wamp
{

namespace internal
{
class HttpJobImplBase;
template <typename> class HttpJobImpl;
}

//------------------------------------------------------------------------------
class CPPWAMP_API HttpJob
{
public:
    using Url = boost::urls::url;

    explicit operator bool() const;

    const Url& target() const;

    std::string method() const;

    const std::string& body() const &;

    std::string&& body() &&;

    ErrorOr<std::string> field(const std::string& key) const;

    std::string fieldOr(const std::string& key, std::string fallback) const;

    const std::string& host() const;

    bool isUpgrade() const;

    bool isWebsocketUpgrade() const;

    const HttpServerOptions& blockOptions() const;

    void continueRequest();

    void respond(HttpResponse&& response);

    void deny(HttpDenial denial);

    void reject(HttpDenial denial);

    void reject(HttpDenial denial, std::error_code logErrorCode);

    template <typename TErrc,
             CPPWAMP_NEEDS(std::is_error_code_enum<TErrc>::value) = 0>
    void reject(HttpDenial denial, TErrc logErrc)
    {
        reject(std::move(denial), make_error_code(logErrc));
    }

    void fail(HttpDenial denial, std::error_code logErrorCode,
              const char* operation);

    template <typename TErrc,
             CPPWAMP_NEEDS(std::is_error_code_enum<TErrc>::value) = 0>
    void fail(HttpDenial denial, TErrc logErrc, const char* operation)
    {
        fail(std::move(denial), make_error_code(logErrc), operation);
    }

    void redirect(std::string location,
                  HttpStatus code = HttpStatus::temporaryRedirect);

    void upgradeToWebsocket(WebsocketOptions options,
                            const WebsocketServerLimits& limits);

private:
    HttpJob(std::shared_ptr<internal::HttpJobImplBase> impl);

    std::shared_ptr<internal::HttpJobImplBase> impl_;

    template <typename> friend class internal::HttpJobImpl;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/httpjob.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_HTTPJOB_HPP
