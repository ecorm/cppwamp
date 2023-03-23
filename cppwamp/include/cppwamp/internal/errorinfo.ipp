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

CPPWAMP_INLINE Error::Error(Uri uri) : Base(std::move(uri)) {}

CPPWAMP_INLINE Error::Error(std::error_code ec) : Base(errorCodeToUri(ec)) {}

CPPWAMP_INLINE Error::Error(WampErrc errc) : Base(errorCodeToUri(errc)) {}

CPPWAMP_INLINE Error::Error(const error::BadType& e)
    : Error(WampErrc::invalidArgument)
{
    withArgs(String{e.what()});
}

CPPWAMP_INLINE Error::~Error() {}

CPPWAMP_INLINE Error::operator bool() const {return !uri().empty();}

CPPWAMP_INLINE const Uri& Error::uri() const {return message().uri();}

/** @return WampErrc::unknown if the URI is unknown. */
CPPWAMP_INLINE WampErrc Error::errorCode() const {return errorUriToCode(uri());}

CPPWAMP_INLINE AccessActionInfo Error::info(bool isServer) const
{
    auto action = isServer ? AccessAction::serverError
                           : AccessAction::clientError;
    return {action, message().requestId(), {}, options(), uri()};
}

CPPWAMP_INLINE Error::Error(internal::PassKey, internal::ErrorMessage&& msg)
    : Base(std::move(msg))
{}

CPPWAMP_INLINE Error::Error(internal::PassKey, internal::MessageKind reqKind,
                            RequestId rid, std::error_code ec, Object opts)
    : Base(reqKind, rid, errorCodeToUri(ec), std::move(opts))
{}

CPPWAMP_INLINE RequestId Error::requestId(internal::PassKey) const
{
    return message().requestId();
}

CPPWAMP_INLINE void Error::setRequestId(internal::PassKey, RequestId rid)
{
    message().setRequestId(rid);
}

CPPWAMP_INLINE internal::ErrorMessage&
Error::errorMessage(internal::PassKey, internal::MessageKind reqKind,
                    RequestId reqId)
{
    auto& msg = message();
    msg.setRequestInfo(reqKind, reqId);
    return msg;
}

} // namespace wamp
