/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORT_HPP
#define CPPWAMP_TRANSPORT_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <system_error>
#include <vector>
#include "connectioninfo.hpp"
#include "erroror.hpp"
#include "messagebuffer.hpp"
#include "timeout.hpp"
#include "internal/random.hpp"
#include "internal/passkey.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains information pertaining to a transport. */
//------------------------------------------------------------------------------
class TransportInfo
{
public:
    /** Default constructor. */
    TransportInfo() = default;

    /** Constructor taking information. */
    TransportInfo(int codecId, std::size_t maxTxLength, std::size_t maxRxLength,
                  Timeout heartbeatInterval = {})
        : codecId_(codecId),
          maxTxLength_(maxTxLength),
          maxRxLength_(maxRxLength),
          heartbeatInterval_(heartbeatInterval)
    {
        static std::mutex theMutex;
        static internal::DefaultPRNG64 theGenerator;
        std::lock_guard<std::mutex> guard{theMutex};
        transportId_ = theGenerator();
    }

    /** Obtains the random transport instance ID. */
    uint64_t transportId() const {return transportId_;}

    /** Obtains the codec numeric ID. */
    int codecId() const {return codecId_;}

    /** Obtains the maximum allowable transmit message length. */
    std::size_t maxTxLength() const {return maxTxLength_;}

    /** Obtains the maximum allowable receive message length. */
    std::size_t maxRxLength() const {return maxRxLength_;}

    /** Obtains the keep-alive heartbeat interval period. */
    Timeout heartbeatInterval() const {return heartbeatInterval_;}

private:
    uint64_t transportId_ = 0;
    int codecId_ = 0;
    std::size_t maxTxLength_ = 0;
    std::size_t maxRxLength_ = 0;
    Timeout heartbeatInterval_ = {};
};

//------------------------------------------------------------------------------
/** Interface class for transports. */
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

    /** @name Non-copyable and non-movable. */
    /// @{
    Transporting(const Transporting&) = delete;
    Transporting(Transporting&&) = delete;
    Transporting& operator=(const Transporting&) = delete;
    Transporting& operator=(Transporting&&) = delete;
    /// @}

    /** Destructor. */
    virtual ~Transporting() = default;

    /** Obtains information pertaining to this transport. */
    const TransportInfo& info() const {return info_;}

    /** Obtains connection information. */
    const ConnectionInfo& connectionInfo() const {return connectionInfo_;}

    /** Returns true if the transport is running. */
    virtual bool isRunning() const = 0;

    /** Starts the transport's I/O operations. */
    virtual void start(RxHandler rxHandler, TxErrorHandler txHandler) = 0;

    /** Sends the given serialized message via the transport. */
    virtual void send(MessageBuffer message) = 0;

    /** Sends the given serialized message, placing it at the top of the queue,
        then closes the underlying socket. */
    virtual void sendNowAndStop(MessageBuffer message) = 0;

    /** Stops I/O operations and closes the underlying socket. */
    virtual void stop() = 0;

protected:
    /** Constructor. */
    explicit Transporting(TransportInfo ti, ConnectionInfo ci)
        : info_(ti),
          connectionInfo_(std::move(ci))
    {}

    /** Should be called be derived classes when the transport disconnects. */
    void clearConnectionInfo() {connectionInfo_ = {};}

private:
    TransportInfo info_;
    ConnectionInfo connectionInfo_;
};

} // namespace wamp

#endif // CPPWAMP_TRANSPORT_HPP
