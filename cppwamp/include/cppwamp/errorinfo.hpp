/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ERRORINFO_HPP
#define CPPWAMP_ERRORINFO_HPP

#include <future>
#include <memory>
#include <string>
#include <vector>
#include "accesslogging.hpp"
#include "api.hpp"
#include "errorcodes.hpp"
#include "payload.hpp"
#include "wampdefs.hpp"
#include "./internal/passkey.hpp"
#include "./internal/wampmessage.hpp"

//------------------------------------------------------------------------------
/** @file
    @brief Provides data structures for information exchanged via WAMP
           error messages. */
//------------------------------------------------------------------------------

namespace wamp
{

//------------------------------------------------------------------------------
/** Provides the _reason_ URI, options, and payload arguments contained
    within WAMP `ERROR` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Error : public Payload<Error, internal::ErrorMessage>
{
public:
    /** Converting constructor taking a reason URI. */
    Error(Uri uri = {});

    /** Converting constructor taking an error code, attempting to convert
        it to a reason URI. */
    Error(std::error_code ec);

    /** Converting constructor taking a WampErrc, attempting to convert
        it to a reason URI. */
    Error(WampErrc errc);

    /** Constructor taking an error::BadType exception and
        interpreting it as a `wamp.error.invalid_argument` reason URI. */
    explicit Error(const error::BadType& e);

    /** Destructor. */
    virtual ~Error();

    /** Conversion to bool operator, returning false if the error is empty. */
    explicit operator bool() const;

    /** Obtains the reason URI. */
    const Uri& uri() const;

    /** Attempts to convert the reason URI to a known error code. */
    WampErrc errorCode() const;

    /** Obtains information for the access log. */
    AccessActionInfo info(bool isServer) const;

private:
    using Base = Payload<Error, internal::ErrorMessage>;

public:
    // Internal use only
    Error(internal::PassKey, internal::ErrorMessage&& msg);

    Error(internal::PassKey, internal::WampMsgType reqType,
          RequestId rid, std::error_code ec, Object opts = {});

    RequestId requestId(internal::PassKey) const;

    void setRequestId(internal::PassKey, RequestId rid);

    internal::ErrorMessage& errorMessage(internal::PassKey,
                                         internal::WampMsgType reqType,
                                         RequestId reqId);
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "./internal/errorinfo.ipp"
#endif

#endif // CPPWAMP_ERRORINFO_HPP
