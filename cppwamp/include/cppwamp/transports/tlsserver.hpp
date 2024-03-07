/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_TLSSERVER_HPP
#define CPPWAMP_TRANSPORTS_TLSSERVER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for establishing TCP transports. */
//------------------------------------------------------------------------------

#include <memory>
#include "../api.hpp"
#include "../asiodefs.hpp"
#include "../listener.hpp"
#include "tlsprotocol.hpp"

namespace wamp
{

// Forward declaration
namespace internal { class TlsListener; }

//------------------------------------------------------------------------------
/** Listener specialization that establishes a server-side TCP transport.
    Users do not need to use this class directly and should instead pass
    wamp::TcpEndpoint to wamp::Router::openServer via wamp::ServerOptions. */
//------------------------------------------------------------------------------
template <>
class CPPWAMP_API Listener<Tls> : public Listening
{
public:
    /** Type containing the transport settings. */
    using Settings = TlsEndpoint;

    /** Constructor. */
    Listener(AnyIoExecutor e, IoStrand i, Settings s, CodecIdSet c,
             RouterLogger::Ptr l = {});

    /** Destructor. */
    ~Listener() override;

    void observe(Handler handler) override;

    void establish() override;

    Transporting::Ptr take() override;

    void drop() override;

    void cancel() override;

    /** @name Non-copyable and non-movable */
    /// @{
    Listener(const Listener&) = delete;
    Listener(Listener&&) = delete;
    Listener& operator=(const Listener&) = delete;
    Listener& operator=(Listener&&) = delete;
    /// @}

private:
    std::shared_ptr<internal::TlsListener> impl_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/tlsserver.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_TLSSERVER_HPP
