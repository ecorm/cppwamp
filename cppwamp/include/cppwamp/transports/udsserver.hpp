/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_UDSSERVER_HPP
#define CPPWAMP_TRANSPORTS_UDSSERVER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for establishing Unix domain socket server
           transports. */
//------------------------------------------------------------------------------

#include <memory>
#include "../api.hpp"
#include "../asiodefs.hpp"
#include "../listener.hpp"
#include "udsprotocol.hpp"

namespace wamp
{

// Forward declaration
namespace internal { struct UdsListenerImpl; }


//------------------------------------------------------------------------------
/** Listener specialization that establishes a server-side TCP transport.
    Users do not need to use this class directly and should instead pass
    wamp::UdsEndpoint to wamp::Router::openServer via wamp::ServerOptions. */
//------------------------------------------------------------------------------
template <>
class CPPWAMP_API Listener<Uds> : public Listening
{
public:
    /** Type containing the transport settings. */
    using Settings = UdsEndpoint;

    /** Constructor. */
    Listener(AnyIoExecutor e, IoStrand i, Settings s, CodecIdSet c);

    /** Move constructor. */
    Listener(Listener&&) noexcept;

    /** Destructor. */
    ~Listener() override;

    /** Move assignment. */
    Listener& operator=(Listener&&) noexcept;

    void observe(Handler handler) override;

    void establish() override;

    void cancel() override;

    /** @name Non-copyable and non-movable */
    /// @{
    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;
    /// @}

private:
    std::unique_ptr<internal::UdsListenerImpl> impl_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/udsserver.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_UDSSERVER_HPP
