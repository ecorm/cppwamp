/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <utility>
#include "config.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE Reason::Reason(String uri) : uri_(std::move(uri)) {}

CPPWAMP_INLINE const String& Reason::uri() const {return uri_;}

CPPWAMP_INLINE String& Reason::uri(internal::PassKey)
    {return uri_;}


//------------------------------------------------------------------------------
CPPWAMP_INLINE Error::Error() {}

CPPWAMP_INLINE Error::Error(String reason) : reason_(std::move(reason)) {}

CPPWAMP_INLINE Error::Error(const error::BadType& e)
    : reason_("wamp.error.invalid_argument")
{
    withArgs(String{e.what()});
}

CPPWAMP_INLINE Error::~Error() {}

CPPWAMP_INLINE const String& Error::reason() const {return reason_;}

CPPWAMP_INLINE Error::operator bool() const {return !reason_.empty();}

CPPWAMP_INLINE String& Error::reason(internal::PassKey) {return reason_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Challenge::Challenge() {}

CPPWAMP_INLINE Challenge::Challenge(String method)
    : method_(std::move(method)) {}

CPPWAMP_INLINE const String& Challenge::method() const {return method_;}

CPPWAMP_INLINE String& Challenge::method(internal::PassKey) {return method_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Authentication::Authentication() {}

CPPWAMP_INLINE Authentication::Authentication(String signature)
    : signature_(std::move(signature)) {}

CPPWAMP_INLINE const String& Authentication::signature() const
    {return signature_;}

CPPWAMP_INLINE String& Authentication::signature(internal::PassKey)
    {return signature_;}

} // namespace wamp
