/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_UDSHOST_HPP
#define CPPWAMP_TRANSPORTS_UDSHOST_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for specifying Unix domain socket
           transport parameters and options. */
//------------------------------------------------------------------------------

#include <string>
#include "../api.hpp"
#include "../connector.hpp"
#include "../rawsockoptions.hpp"
#include "udsprotocol.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains a Unix Domain Socket path, as well as other socket options
    for a client connection.
    Meets the requirements of @ref TransportSettings.
    @see ConnectionWish */
//------------------------------------------------------------------------------
class CPPWAMP_API UdsHost
{
public:
    /// Transport protocol tag associated with these settings.
    using Protocol = Uds;

    /** Constructor taking a path name. */
    explicit UdsHost(std::string pathName);

    /** Specifies the socket options to use. */
    UdsHost& withSocketOptions(UdsOptions options);

    /** Specifies the maximum length permitted for incoming messages. */
    UdsHost& withMaxRxLength(RawsockMaxLength length);

    /** Couples a serialization format with these transport settings to
        produce a ConnectionWish that can be passed to Session::connect. */
    template <typename TFormat>
    ConnectionWish withFormat(TFormat) const
    {
        return ConnectionWish{*this, TFormat{}};
    }

    /** Obtains the path name. */
    const std::string& pathName() const;

    /** Obtains the transport options. */
    const UdsOptions& socketOptions() const;

    /** Obtains the specified maximum incoming message length. */
    RawsockMaxLength maxRxLength() const;

    /** Generates a human-friendly string of the UDS path. */
    std::string label() const;

private:
    std::string pathName_;
    UdsOptions options_;
    RawsockMaxLength maxRxLength_ = RawsockMaxLength::MB_16;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/udshost.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_UDSHOST_HPP
