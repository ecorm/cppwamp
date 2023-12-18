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
class ConnectionInfoImpl
{
public:
    using Ptr = std::shared_ptr<ConnectionInfoImpl>;
    using ConstPtr = std::shared_ptr<const ConnectionInfoImpl>;
    using ServerSessionNumber = uint64_t;

    explicit ConnectionInfoImpl(Object transport, std::string endpoint)
        : transport_(std::move(transport)),
          endpoint_(std::move(endpoint))
    {}

    ~ConnectionInfoImpl() = default;

    ConnectionInfoImpl(ConnectionInfoImpl&&) = default;
    ConnectionInfoImpl& operator=(ConnectionInfoImpl&&) = default;

    const Object& transport() const {return transport_;}

    const std::string& endpoint() const {return endpoint_;}

    const std::string& server() const {return server_;}

    const std::string& target() const {return target_;}

    ServerSessionNumber serverSessionNumber() const
    {
        return serverSessionNumber_;
    }

    void setServer(const std::string& name, ServerSessionNumber n)
    {
        server_ = name;
        serverSessionNumber_ = n;
        transport_.emplace("server", name);
    }

    void setTarget(std::string target)
    {
        if (target.empty())
            return;
        target_ = std::move(target);
        transport_.emplace("target", target_);
        endpoint_ += target_;
    }

    ConnectionInfoImpl(const ConnectionInfoImpl&) = delete;
    ConnectionInfoImpl& operator=(const ConnectionInfoImpl&) = delete;

private:
    Object transport_;
    std::string endpoint_;
    std::string server_;
    std::string target_;
    ServerSessionNumber serverSessionNumber_ = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CONNECTIONINFOIMPL_HPP
