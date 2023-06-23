/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ERRORINFO_HPP
#define CPPWAMP_ERRORINFO_HPP

#include <memory>
#include <string>
#include <vector>
#include "accesslogging.hpp"
#include "api.hpp"
#include "errorcodes.hpp"
#include "payload.hpp"
#include "tagtypes.hpp"
#include "wampdefs.hpp"
#include "internal/message.hpp"
#include "internal/passkey.hpp"

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
class CPPWAMP_API Error : public Payload<Error, internal::MessageKind::error>
{
public:
    /** Default constructor. */
    Error();

    /** Converting constructor taking a reason URI and optional positional
        payload arguments. */
    template <typename... Ts>
    Error(Uri uri, Ts&&... args);

    /** Converting constructor taking an error code, attempting to convert
        it to a reason URI, as well as optional positional payload arguments. */
    template <typename... Ts>
    Error(std::error_code ec, Ts&&... args);

    /** Converting constructor taking a WampErrc, attempting to convert
        it to a reason URI, as well as optional positional payload arguments. */
    template <typename... Ts>
    Error(WampErrc errc, Ts&&... args);

    /** Constructor taking an error::BadType exception and
        interpreting it as a `wamp.error.invalid_argument` reason URI. */
    explicit Error(const error::BadType& e);

    /** Copy constructor. */
    Error(const Error&) = default;

    /** Move constructor. */
    Error(Error&&) = default;

    /** Destructor. */
    virtual ~Error() = default;

    /** Copy assignment. */
    Error& operator=(const Error&) = default;

    /** Move assignment. */
    Error& operator=(Error&&) = default;

    /** Conversion to bool operator, returning false if the error is empty. */
    explicit operator bool() const;

    /** Obtains the reason URI. */
    const Uri& uri() const &;

    /** Moves the reason URI. */
    Uri&& uri() &&;

    /** Attempts to convert the reason URI to a known error code. */
    WampErrc errorCode() const;

    /** Obtains information for the access log. */
    AccessActionInfo info(bool isServer) const;

private:
    static constexpr unsigned requestKindPos_ = 1;
    static constexpr unsigned uriPos_         = 4;

    using Base = Payload<Error, internal::MessageKind::error>;

    explicit Error(in_place_t, Uri uri, Array args);

public:
    // Internal use only
    template <typename C>
    static Error fromRequest(internal::PassKey, const C& command,
                             std::error_code ec)
    {
        return Error{internal::PassKey{}, C::messageKind({}),
                     command.requestId({}), ec};
    }

    template <typename C>
    static Error fromRequest(internal::PassKey, const C& command,
                             WampErrc errc)
    {
        return Error{internal::PassKey{}, C::messageKind({}),
                     command.requestId({}), errc};
    }

    Error(internal::PassKey, internal::Message&& msg);

    Error(internal::PassKey, internal::MessageKind reqKind,
          RequestId rid, WampErrc errc, Object opts = {});

    Error(internal::PassKey, internal::MessageKind reqKind,
          RequestId rid, std::error_code ec, Object opts = {});

    template <typename C>
    AccessActionInfo info(internal::PassKey, const C& command)
    {
        auto actionInfo = info(true);
        auto uriPos = internal::MessageKindTraits<C::messageKind({})>::uriPos();
        if (uriPos != 0)
        {
            actionInfo.target =
                command.message({}).template as<String>(uriPos);
        }
        return actionInfo;
    }

    void setRequestKind(internal::PassKey, internal::MessageKind reqKind);
};


/******************************************************************************/
// Error inline member function definitions
/******************************************************************************/

template <typename... Ts>
Error::Error(Uri uri, Ts&&... args)
    : Error(in_place, std::move(uri), Array{std::forward<Ts>(args)...})
{}

template <typename... Ts>
Error::Error(std::error_code ec, Ts&&... args)
    : Error(in_place, errorCodeToUri(ec), Array{std::forward<Ts>(args)...})
{}

template <typename... Ts>
Error::Error(WampErrc errc, Ts&&... args)
    : Error(in_place, errorCodeToUri(errc), Array{std::forward<Ts>(args)...})
{}

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/errorinfo.inl.hpp"
#endif

#endif // CPPWAMP_ERRORINFO_HPP
