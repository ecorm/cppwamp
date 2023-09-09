/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/httpprotocol.hpp"
#include <array>
#include "../api.hpp"

namespace wamp
{

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
    }
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

} // namespace wamp
