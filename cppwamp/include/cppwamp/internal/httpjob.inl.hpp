/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023-2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/httpjob.hpp"
#include "../api.hpp"
#include "httpjobimpl.hpp"

namespace wamp
{

//******************************************************************************
// HttpJob
//******************************************************************************

CPPWAMP_INLINE HttpJob::operator bool() const {return impl_ != nullptr;}

CPPWAMP_INLINE const HttpJob::Url& HttpJob::target() const
{
    return impl_->target();
}

CPPWAMP_INLINE std::string HttpJob::method() const {return impl_->method();}

CPPWAMP_INLINE const std::string& HttpJob::body() const &
{
    return impl_->body();
}

CPPWAMP_INLINE std::string&& HttpJob::body() &&
{
    return std::move(impl_->body());
}

CPPWAMP_INLINE ErrorOr<std::string> HttpJob::field(const std::string& key) const
{
    return impl_->field(key);
}

CPPWAMP_INLINE std::string
HttpJob::fieldOr(const std::string& key, std::string fallback) const
{
    return impl_->fieldOr(key, std::move(fallback));
}

CPPWAMP_INLINE const std::string& HttpJob::hostName() const
{
    return impl_->hostName();
}

CPPWAMP_INLINE bool HttpJob::isUpgrade() const {return impl_->isUpgrade();}

CPPWAMP_INLINE bool HttpJob::isWebsocketUpgrade() const
{
    return impl_->isWebsocketUpgrade();
}

CPPWAMP_INLINE const HttpEndpoint& HttpJob::settings() const
{
    return impl_->settings();
}

CPPWAMP_INLINE void HttpJob::continueRequest() {impl_->continueRequest();}

CPPWAMP_INLINE void HttpJob::respond(HttpResponse&& response)
{
    impl_->respond(std::move(response));
}

CPPWAMP_INLINE void HttpJob::deny(HttpDenial denial)
{
    impl_->deny(std::move(denial));
}

CPPWAMP_INLINE void HttpJob::redirect(std::string location, HttpStatus code)
{
    deny(HttpDenial{code}.withFields({{"Location", std::move(location)}}));
}

CPPWAMP_INLINE void HttpJob::upgradeToWebsocket(
    WebsocketOptions options, const WebsocketServerLimits& limits)
{
    return impl_->upgradeToWebsocket(std::move(options), limits);
}

CPPWAMP_INLINE HttpJob::HttpJob(std::shared_ptr<internal::HttpJobImpl> impl)
    : impl_(std::move(impl))
{}

} // namespace wamp
