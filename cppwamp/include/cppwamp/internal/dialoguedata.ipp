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

CPPWAMP_INLINE Reason::Reason(String uri) : uri_(std::move(uri)) {}

CPPWAMP_INLINE const String& Reason::uri() const {return uri_;}

CPPWAMP_INLINE String& Reason::uri(internal::PassKey)
    {return uri_;}

CPPWAMP_INLINE Error::Error(String reason) : reason_(std::move(reason)) {}

CPPWAMP_INLINE const String& Error::reason() const {return reason_;}

CPPWAMP_INLINE String& Error::reason(internal::PassKey)
    {return reason_;}

} // namespace wamp
