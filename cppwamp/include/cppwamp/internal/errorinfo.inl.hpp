/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../errorinfo.hpp"
#include <utility>
#include "../api.hpp"

namespace wamp
{

CPPWAMP_INLINE Error::Error() : Error(in_place, {}, {}) {}

CPPWAMP_INLINE Error::Error(const error::BadType& e)
    : Error(WampErrc::invalidArgument)
{
    withArgs(String{e.what()});
}

CPPWAMP_INLINE Error::operator bool() const {return !uri().empty();}

CPPWAMP_INLINE const Uri& Error::uri() const &
{
    return message().as<String>(uriPos_);
}

CPPWAMP_INLINE Uri&& Error::uri() &&
{
    return std::move(message().as<String>(uriPos_));
}

/** @return WampErrc::unknown if the URI is unknown. */
CPPWAMP_INLINE WampErrc Error::errorCode() const {return errorUriToCode(uri());}

CPPWAMP_INLINE AccessActionInfo Error::info(bool isServer) const
{
    AccessAction action = {};
    if (message().kind() == internal::MessageKind::error)
    {
        action = isServer ? AccessAction::serverError
                          : AccessAction::clientError;
    }
    else
    {
        action = isServer ? AccessAction::serverAbort
                          : AccessAction::clientAbort;
    }
    return {action, requestId(), {}, options(), uri()};
}

CPPWAMP_INLINE Error::Error(in_place_t, Uri uri, Array args)
    : Base(in_place, 0, 0, Object{}, std::move(uri), std::move(args), Object{})
{}


CPPWAMP_INLINE Error::Error(internal::PassKey, internal::Message&& msg)
    : Base(std::move(msg))
{}

CPPWAMP_INLINE Error::Error(internal::PassKey, internal::MessageKind reqKind,
                            RequestId rid, WampErrc errc, Object opts)
    : Base(in_place, static_cast<Int>(reqKind), rid, std::move(opts),
           errorCodeToUri(errc))
{}

CPPWAMP_INLINE Error::Error(internal::PassKey, internal::MessageKind reqKind,
                            RequestId rid, std::error_code ec, Object opts)
    : Base(in_place, static_cast<Int>(reqKind), rid, std::move(opts),
           errorCodeToUri(ec))
{}

CPPWAMP_INLINE void Error::setRequestKind(internal::PassKey,
                                          internal::MessageKind reqKind)
{
    message().at(requestKindPos_) = static_cast<Int>(reqKind);
}

} // namespace wamp
