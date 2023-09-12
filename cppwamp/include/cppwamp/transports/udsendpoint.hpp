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

#include "../api.hpp"
#include "../rawsockoptions.hpp"
#include "socketendpoint.hpp"
#include "udsprotocol.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains a Unix Domain Socket server path, as well as other socket options.
    Meets the requirements of @ref TransportSettings. */
//------------------------------------------------------------------------------
class CPPWAMP_API UdsEndpoint
    : public SocketEndpoint<UdsEndpoint, Uds, UdsOptions, RawsockMaxLength,
                            RawsockMaxLength::MB_16>
{
public:
    /** Constructor taking a path name. */
    explicit UdsEndpoint(std::string pathName)
        : Base(std::move(pathName), 0)
    {}

    /** Enables/disables the deletion of existing file path before listening. */
    UdsEndpoint& withDeletePath(bool enabled = true)
    {
        deletePathEnabled_ = enabled;
        return *this;
    }

    /** Returns true if automatic path deletion before listening is enabled. */
    bool deletePathEnabled() const
    {
        return deletePathEnabled_;
    }

    /** Generates a human-friendly string of the UDS path. */
    std::string label() const
    {
        return "Unix domain socket path '" + address() + "'";
    }

private:
    using Base = SocketEndpoint<UdsEndpoint, Uds, UdsOptions, RawsockMaxLength,
                                RawsockMaxLength::MB_16>;

    using Base::port;

    bool deletePathEnabled_ = true;
};

} // namespace wamp

#endif // CPPWAMP_TRANSPORTS_UDSENDPOINT_HPP
