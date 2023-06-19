/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CONNECTIONINFOIMPL_HPP
#define CPPWAMP_INTERNAL_CONNECTIONINFOIMPL_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for authentication information. */
//------------------------------------------------------------------------------

#include <memory>
#include "../variant.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
/** Contains meta-data associated with a WAMP client session. */
//------------------------------------------------------------------------------
class ConnectionInfoImpl
{
public:
    using Ptr = std::shared_ptr<ConnectionInfoImpl>;
    using ConstPtr = std::shared_ptr<const ConnectionInfoImpl>;
    using ServerSessionNumber = uint64_t;

    static Ptr create(Object transport, String endpoint)
    {
        return Ptr(new ConnectionInfoImpl(std::move(transport),
                                          std::move(endpoint)));
    }

    ~ConnectionInfoImpl() = default;
    ConnectionInfoImpl(ConnectionInfoImpl&&) = default;
    ConnectionInfoImpl& operator=(ConnectionInfoImpl&&) = default;

    const Object& transport() const {return transport_;}

    const std::string& endpoint() const {return endpoint_;}

    const std::string& server() const {return server_;}

    ServerSessionNumber serverSessionNumber() const
    {
        return serverSessionNumber_;
    }

    void setServer(std::string server, ServerSessionNumber n)
    {
        server_ = std::move(server);
        serverSessionNumber_ = n;
        transport_["server"] = server_;
    }

    ConnectionInfoImpl(const ConnectionInfoImpl&) = delete;
    ConnectionInfoImpl& operator=(const ConnectionInfoImpl&) = delete;

private:
    explicit ConnectionInfoImpl(Object transport, std::string endpoint)
        : transport_(std::move(transport)),
          endpoint_(std::move(endpoint))
    {}

    Object transport_;
    std::string endpoint_;
    std::string server_;
    ServerSessionNumber serverSessionNumber_ = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CONNECTIONINFOIMPL_HPP
