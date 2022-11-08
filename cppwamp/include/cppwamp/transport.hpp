/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORT_HPP
#define CPPWAMP_TRANSPORT_HPP

#include <functional>
#include <memory>
#include <system_error>
#include <vector>
#include "asiodefs.hpp"
#include "erroror.hpp"
#include "messagebuffer.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
struct TransportInfo
{
    int codecId;
    std::size_t maxTxLength;
    std::size_t maxRxLength;
};

//------------------------------------------------------------------------------
// Interface class for transports.
//------------------------------------------------------------------------------
class Transporting : public std::enable_shared_from_this<Transporting>
{
public:
    /// Shared pointer to a Transporting object.
    using Ptr = std::shared_ptr<Transporting>;

    /// Handler type used for message received events.
    using RxHandler = std::function<void (ErrorOr<MessageBuffer>)>;

    /// Handler type used for transmission error events.
    using TxErrorHandler = std::function<void (std::error_code)>;

    /// Handler type used for ping response events.
    using PingHandler = std::function<void (float)>;

    // Noncopyable
    Transporting(const Transporting&) = delete;
    Transporting& operator=(const Transporting&) = delete;

    /** Destructor. */
    virtual ~Transporting() {}

    /** Obtains information pertaining to this transport. */
    virtual TransportInfo info() const = 0;

    /** Returns true if the transport has been started. */
    virtual bool isStarted() const = 0;

    /** Starts the transport's I/O operations. */
    virtual void start(RxHandler rxHandler,
                       TxErrorHandler txHandler = nullptr) = 0;

    /** Sends the given serialized message via the transport. */
    virtual void send(MessageBuffer message) = 0;

    /** Sends the given serialized message, placing it at the top of the queue,
        then closes the underlying socket. */
    virtual void sendNowAndClose(MessageBuffer message) = 0;

    /** Stops I/O operations and closes the underlying socket. */
    virtual void close() = 0;

    /** Sends a transport-level ping message. */
    virtual void ping(MessageBuffer message, PingHandler handler) = 0;

protected:
    Transporting() = default;
};

} // namespace wamp

#endif // CPPWAMP_TRANSPORT_HPP
