/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_TCPSERVER_HPP
#define CPPWAMP_TRANSPORTS_TCPSERVER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for establishing TCP transports. */
//------------------------------------------------------------------------------

#include <memory>
#include "../api.hpp"
#include "../asiodefs.hpp"
#include "../listener.hpp"
#include "tcpprotocol.hpp"

// TODO: TLS Server Transport

namespace wamp
{

// Forward declaration
namespace internal { struct TcpListenerImpl; }

//------------------------------------------------------------------------------
/** Listener specialization that establishes a server-side TCP transport.
    Users do not need to use this class directly and should instead pass
    wamp::TcpEndpoint to wamp::Router::openServer via wamp::ServerOptions. */
//------------------------------------------------------------------------------
template <>
class CPPWAMP_API Listener<Tcp> : public Listening
{
public:
    /** Type containing the transport settings. */
    using Settings = TcpEndpoint;

    /** Constructor. */
    Listener(AnyIoExecutor e, IoStrand i, Settings s, CodecIdSet c,
             const std::string& server = {}, ServerLogger::Ptr l = {});

    /** Destructor. */
    ~Listener() override;

    void observe(Handler handler) override;

    void establish() override;

    void cancel() override;

    /** @name Non-copyable and non-movable */
    /// @{
    Listener(const Listener&) = delete;
    Listener(Listener&&) = delete;
    Listener& operator=(const Listener&) = delete;
    Listener& operator=(Listener&&) = delete;
    /// @}

private:
    std::unique_ptr<internal::TcpListenerImpl> impl_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/tcpserver.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_TCPSERVER_HPP
