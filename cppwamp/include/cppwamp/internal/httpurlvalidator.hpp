/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_HTTPURLVALIDATOR_HPP
#define CPPWAMP_INTERNAL_HTTPURLVALIDATOR_HPP

#include <boost/beast/http/verb.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>
#include <boost/url/url_view.hpp>
#include <boost/url/grammar/error.hpp>
#include "../erroror.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class HttpUrlValidator
{
public:
    using Url = boost::urls::url;
    using UrlView = boost::urls::url_view;
    using StringView = boost::beast::string_view;
    using Verb = boost::beast::http::verb;

    static ErrorOr<Url> interpretAndNormalize(StringView target, Verb verb)
    {
        auto result = boost::urls::parse_uri_reference(target);
        if (result.has_error())
            return makeUnexpected(static_cast<std::error_code>(result.error()));

        if (!isValid(*result, verb))
        {
            auto ec = make_error_code(boost::urls::grammar::error::mismatch);
            return makeUnexpected(static_cast<std::error_code>(ec));
        }

        Url url{*result};
        url.normalize();
        return url;
    }

    static bool isValid(const UrlView& url, Verb verb)
    {
        using V = Verb;

        switch (verb)
        {
        case V::delete_: case V::get: case V::head:
        case V::post: case V::put: case V::trace:
            return isOriginFormUrl(url) ||  // RFC7230, section 5.3.1
                   isAbsoluteFormUrl(url);  // RFC7230, section 5.3.2

        case V::connect:
            return isAuthorityFormUrl(url); // RFC7230, section 5.3.3

        case V::options:
            return isAsteriskFormUrl(url);  // RFC7230, section 5.3.4

        default:
            break;
        }

        return true; // Caller needs to check for other, non-standard verbs.
    }

    static bool isOriginFormUrl(const UrlView& url)
    {
        /**
        origin-form   = absolute-path [ "?" query ]
        absolute-path = 1*( "/" segment )
        */
        return !url.has_scheme() &&
               !url.has_authority() &&
               url.is_path_absolute();
    }

    static bool isAbsoluteFormUrl(const UrlView& url)
    {
        /**
        absolute-form = absolute-URI ; defers to RFC3986
        absolute-URI  = scheme ":" hier-part [ "?" query ]
        hier-part     = "//" authority path-abempty
                         / path-absolute
                         / path-rootless
                         / path-empty
        path-abempty  = *( "/" segment ) ; begins with "/" or is empty
        path-absolute = "/" [ segment-nz *( "/" segment ) ] ; begins with "/"
                                                              but not "//"
        path-rootless = segment-nz *( "/" segment ) ; begins with a segment
        path-empty    = 0<pchar> ; zero characters

        // Initial logic expression:
        bool pabs = url.is_path_absolute();
        bool pempty = url.path().empty();
        bool is_absolute_form =
            url.has_scheme() &&
            (url.has_authority()
                ? (pabs || pempty)      // path-abempty
                : (pabs ||              // path-absolute
                  (!pabs && !pempty) || // path-rootless
                   pempty));            // path-empty

        // Simplifies to:
        bool is_absolute_form =
            url.has_scheme() &&
            (url.has_authority()
                 ? true  // Path can only be absolute or empty with an authority
                 : true); // Logical tautology

        // Which simplifies to:
        bool is_absolute_form = url.has_scheme();

        Proof of logical tautology:
            pabs || (!pabs && !pempty) || pempty
        Rearranging the OR terms and grouping:
            (pabs || pempty) || (!pabs && !pempty)
        Applying De Morgan's Theorem to the last OR term:
            (pabs || pempty) || !(pabs || pempty)
        Substituting A = (pabs || pempty):
            A || !A
        which is always true.
        */

        return url.has_scheme();
    }

    static bool isAuthorityFormUrl(const UrlView& url)
    {
        /**
        authority-form = authority ; defers to RFC3986
        authority      = [ userinfo "@" ] host [ ":" port ]
        */
        return !url.has_scheme() && url.has_authority() && url.path().empty();
    }

    static bool isAsteriskFormUrl(const UrlView& url)
    {
        // asterisk-form = "*"
        return url.buffer() == "*";
    }
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_HTTPURLVALIDATOR_HPP
