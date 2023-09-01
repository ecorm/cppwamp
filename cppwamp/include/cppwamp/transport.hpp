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
/** Enumerates the possible transport states. */
//------------------------------------------------------------------------------
enum class TransportState
{
    initial,
    accepting,
    ready,
    running,
    stopped,
    failed
};


//------------------------------------------------------------------------------
/** Interface class for transports. */
//------------------------------------------------------------------------------
class Transporting : public std::enable_shared_from_this<Transporting>
{
public:
    /// Enumerates the possible transport states
    using State = TransportState;

    /// Shared pointer to a Transporting object.
    using Ptr = std::shared_ptr<Transporting>;

    /// Handler type used for server handshake completion events.
    using AcceptHandler = std::function<void (ErrorOr<int> codecId)>;

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

    /** Obtains the current transport state. */
    State state() const {return state_;}

    /** Obtains information pertaining to this transport. */
    const TransportInfo& info() const {return info_;}

    /** Obtains connection information. */
    const ConnectionInfo& connectionInfo() const {return connectionInfo_;}

    /** Starts the server handshake procedure.
        @pre this->state() == Transporting::State::initial
        @post this->state() == Transporting::State::accepting */
    void accept(AcceptHandler handler)
    {
        assert(state_ == State::initial);
        onAccept(std::move(handler));
        state_ = State::accepting;
    }

    /** Starts the transport's I/O operations.
        @pre this->state() == Transporting::State::initial
        @post this->state() == Transporting::State::running */
    void start(RxHandler rxHandler, TxErrorHandler txHandler)
    {
        assert(state_ == State::ready);
        onStart(std::move(rxHandler), std::move(txHandler));
        state_ = State::running;
    }

    /** Sends the given serialized message via the transport.
        @pre this->state() != Transporting::State::initial */
    void send(MessageBuffer message)
    {
        assert(state_ != State::initial);
        if (state_ == State::running)
            onSend(std::move(message));
    }

    /** Sends the given serialized message, placing it at the top of the queue,
        then closes the underlying socket.
        @pre this->state() != Transporting::State::initial
        @post this->state() == Transporting::State::stopped */
    virtual void sendNowAndStop(MessageBuffer message)
    {
        assert(state_ != Transporting::State::initial);
        if (state_ == State::running)
            onSendNowAndStop(std::move(message));
        state_ = State::stopped;
    }

    // TODO: Asynchronous Session::disconnect for transports with
    // closing handhakes (e.g. Websocket)
    /** Stops I/O operations and closes the underlying socket.
        @post this->state() == Transporting::State::stopped */
    virtual void stop()
    {
        if (state_ == State::accepting)
            onCancelAccept();
        if (state_ == State::running)
            onStop();
        state_ = State::stopped;
    }

protected:
    /** Constructor for client transports. */
    explicit Transporting(TransportInfo ti, ConnectionInfo ci)
        : info_(ti),
          connectionInfo_(std::move(ci)),
          state_(State::ready)
    {}

    /** Constructor for server transports. */
    explicit Transporting() = default;

    /** Must be overridden by server transports to initiate the handshake. */
    virtual void onAccept(AcceptHandler handler)
    {
        assert(false && "Not a server transport");
    }

    /** Must be overridden by server transports to cancel a handshake. */
    virtual void onCancelAccept()
    {
        assert(false && "Not a server transport");
    }

    /** Must be overridden to start the transport's I/O operations. */
    virtual void onStart(RxHandler rxHandler, TxErrorHandler txHandler) = 0;

    /** Must be overridden to send the given serialized message. */
    virtual void onSend(MessageBuffer message) = 0;

    /** Must be overriden to send the given serialized message ASAP and then
        disconnect. */
    virtual void onSendNowAndStop(MessageBuffer message) = 0;

    /** Must be overriden to stop I/O operations and disconnect. */
    virtual void onStop() = 0;

    /** Should be called by derived server classes after transport details
        have been negotiated successfully. */
    void completeAccept(TransportInfo ti)
    {
        info_ = ti;
        state_ = State::ready;
    }

    /** Should be called by derived server classes upon handshake failure. */
    void failAccept()
    {
        state_ = State::stopped;
    }

    /** Should be called by derived classes when the transport disconnects. */
    void shutdown(bool failed = false)
    {
        connectionInfo_ = {};
        if (state_ != State::failed && state_ != State::stopped)
            state_ = failed ? State::failed : State::stopped;
    }

private:
    TransportInfo info_;
    ConnectionInfo connectionInfo_;
    State state_ = State::initial;
};

} // namespace wamp

#endif // CPPWAMP_TRANSPORT_HPP
