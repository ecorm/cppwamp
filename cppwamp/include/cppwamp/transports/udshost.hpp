/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_UDSHOST_HPP
#define CPPWAMP_TRANSPORTS_UDSHOST_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for specifying Unix domain socket client
           transport parameters and options. */
//------------------------------------------------------------------------------

#include "../api.hpp"
#include "../rawsockoptions.hpp"
#include "udsprotocol.hpp"
#include "sockethost.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains a Unix Domain Socket path, as well as other socket options
    for a client connection.
    Meets the requirements of @ref TransportSettings.
    @see ConnectionWish */
//------------------------------------------------------------------------------
class CPPWAMP_API UdsHost
    : public SocketHost<UdsHost, Uds, UdsOptions, RawsockMaxLength,
                        RawsockMaxLength::MB_16>
{
public:
    /** Constructor taking a path name. */
    explicit UdsHost(std::string pathName)
        : Base(std::move(pathName), "")
    {}

private:
    using Base = SocketHost<UdsHost, Uds, UdsOptions, RawsockMaxLength,
                            RawsockMaxLength::MB_16>;
    using Base::serviceName;
};

} // namespace wamp

#endif // CPPWAMP_TRANSPORTS_UDSHOST_HPP
