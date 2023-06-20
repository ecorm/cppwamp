/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TCP_HPP
#define CPPWAMP_TCP_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for creating TCP transport connectors. */
//------------------------------------------------------------------------------

#include <memory>
#include <set>
#include <utility>
#include "api.hpp"
#include "asiodefs.hpp"
#include "connector.hpp"
#include "listener.hpp"
#include "tcpendpoint.hpp"
#include "tcphost.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Connector specialization that establishes a client-side TCP transport.
    Users do not need to use this class directly and should use
    ConnectionWish instead. */
//------------------------------------------------------------------------------
template <>
class CPPWAMP_API Connector<Tcp> : public Connecting
{
public:
    /** Type containing the transport settings. */
    using Settings = TcpHost;

    /** Constructor. */
    Connector(IoStrand i, Settings s, int codecId);

    /** Move constructor. */
    Connector(Connector&&);

    /** Destructor. */
    ~Connector() override;

    /** Move assignment. */
    Connector& operator=(Connector&&);

    /** Starts establishing the transport connection, emitting a
        Transportable::Ptr via the given handler if successful. */
    void establish(Handler&& handler) override;

    /** Cancels transport connection in progress, emitting an error code
        via the handler passed to the establish method. */
    void cancel() override;

    /** @name Non-copyable */
    /// @{
    Connector(const Connector&) = delete;
    Connector& operator=(const Connector&) = delete;
    /// @}

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

//------------------------------------------------------------------------------
/** Listener specialization that establishes a server-side TCP transport.
    Users do not need to use this class directly and should use
    ConnectionWish instead. */
//------------------------------------------------------------------------------
template <>
class CPPWAMP_API Listener<Tcp> : public Listening
{
public:
    /** Type containing the transport settings. */
    using Settings = TcpEndpoint;

    /** Collection type used for codec IDs. */
    using CodecIds = std::set<int>;

    /** Constructor. */
    Listener(IoStrand i, Settings s, CodecIds codecIds);

    /** Move constructor. */
    Listener(Listener&&);

    /** Destructor. */
    ~Listener() override;

    /** Move assignment. */
    Listener& operator=(Listener&&);

    void establish(Handler&& handler) override;

    void cancel() override;

    /** @name Non-copyable */
    /// @{
    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;
    /// @}

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/tcp.inl.hpp"
#endif

#endif // CPPWAMP_TCP_HPP
