/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_UDSENDPOINT_HPP
#define CPPWAMP_TRANSPORTS_UDSENDPOINT_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for specifying Unix domain socket
           transport parameters and options. */
//------------------------------------------------------------------------------

#include <string>
#include "../api.hpp"
#include "../rawsockoptions.hpp"
#include "udsprotocol.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains a Unix Domain Socket path, as well as other socket options for
    a server.
    Meets the requirements of @ref TransportSettings. */
//------------------------------------------------------------------------------
class CPPWAMP_API UdsEndpoint
{
public:
    /// Transport protocol tag associated with these settings.
    using Protocol = Uds;

    /** Constructor taking a path name. */
    explicit UdsEndpoint(std::string pathName);

    /** Specifies the socket options to use for the per-peer sockets. */
    UdsEndpoint& withSocketOptions(UdsOptions options);

    /** Specifies the socket options to use for the acceptor. */
    UdsEndpoint& withAcceptorOptions(UdsOptions options);

    /** Specifies the maximum length permitted for incoming messages. */
    UdsEndpoint& withMaxRxLength(RawsockMaxLength length);

    /** Enables/disables the deletion of existing file path before listening. */
    UdsEndpoint& withDeletePath(bool enabled = true);

    /** Specifies the acceptor's maximum number of pending connections. */
    UdsEndpoint& withBacklogCapacity(int capacity);

    /** Obtains the path name. */
    const std::string& pathName() const;

    /** Obtains the per-peer socket options. */
    const UdsOptions& socketOptions() const;

    /** Obtains the acceptor socket options. */
    const UdsOptions& acceptorOptions() const;

    /** Obtains the specified maximum incoming message length. */
    RawsockMaxLength maxRxLength() const;

    /** Returns true if automatic path deletion before listening is enabled. */
    bool deletePathEnabled() const;

    /** Obtains the acceptor's maximum number of pending connections. */
    int backlogCapacity() const;

    /** Generates a human-friendly string of the UDS path. */
    std::string label() const;

private:
    std::string pathName_;
    UdsOptions options_;
    UdsOptions acceptorOptions_;
    int backlogCapacity_;
    RawsockMaxLength maxRxLength_ = RawsockMaxLength::MB_16;
    bool deletePathEnabled_ = true;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/udsendpoint.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_UDSENDPOINT_HPP
