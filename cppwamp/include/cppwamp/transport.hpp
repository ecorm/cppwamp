/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORT_HPP
#define CPPWAMP_TRANSPORT_HPP

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <system_error>
#include <vector>
#include "anyhandler.hpp"
#include "asiodefs.hpp"
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
    initial,   /// Initial state of a server transport
    accepting, /// The server transport is performing its handshake
    shedding,  /// Server is handshaking but will ultimately shed connection
    ready,     /// Transport handshake is complete (initial for client)
    running,   /// Sending and receiving of messages is enabled
    stopping,  /// Transport is performing its closing handshake
    stopped    /// Handshake cancelled or transport has been stopped
};


//------------------------------------------------------------------------------
/** Base class for transports. */
//------------------------------------------------------------------------------
class Transporting : public std::enable_shared_from_this<Transporting>
{
public:
    /// Enumerates the possible transport states
    using State = TransportState;

    /// Shared pointer to a Transporting object.
    using Ptr = std::shared_ptr<Transporting>;

    /// Handler type used for message received events.
    using RxHandler = std::function<void (ErrorOr<MessageBuffer>)>;

    /// Handler type used for transmission error events.
    using TxErrorHandler = std::function<void (std::error_code)>;

    /// Handler type used for server handshake completion.
    using AdmitHandler = AnyCompletionHandler<void (ErrorOr<int> codecId)>;

    /// Handler type used for transport close completion.
    using CloseHandler = AnyCompletionHandler<void (ErrorOr<bool>)>;

    /** @name Non-copyable and non-movable. */
    /// @{
    Transporting(const Transporting&) = delete;
    Transporting(Transporting&&) = delete;
    Transporting& operator=(const Transporting&) = delete;
    Transporting& operator=(Transporting&&) = delete;
    /// @}

    /** Destructor. */
    virtual ~Transporting() = default;

    /** Obtains the execution strand associated with this transport. */
    const IoStrand& strand() const {return strand_;}

    /** Obtains the current transport state. */
    State state() const {return state_;}

    /** Obtains information pertaining to this transport. */
    const TransportInfo& info() const {return info_;}

    /** Obtains connection information. */
    const ConnectionInfo& connectionInfo() const {return connectionInfo_;}

    /** Starts the server handshake procedure to admit a new client connection.
        @pre this->state() == TransportState::initial
        @post this->state() == TransportState::accepting */
    void admit(AdmitHandler handler)
    {
        admit(unspecifiedTimeout, std::move(handler));
    }

    /** Starts the server handshake procedure with the given timeout.
        @copydetails Transporting::admit(AdmitHandler) */
    void admit(Timeout timeout, AdmitHandler handler)
    {
        assert(state_ == State::initial);
        onAccept(timeout, std::move(handler));
        state_ = State::accepting;
    }

    /** Starts the server handshake procedure, but ultimately refuses the
        client connection due to server connection limit.
        Either an TransportErrc::saturated error will be emitted via the
        handler, or some other error due a handshake failure.
        @pre this->state() == TransportState::initial
        @post this->state() == TransportState::shedding */
    void shed(AdmitHandler handler)
    {
        shed(unspecifiedTimeout, std::move(handler));
    }

    /** Starts refusal handshake with the given timeout.
        @copydetails Transporting::shed(AdmitHandler) */
    void shed(Timeout timeout, AdmitHandler handler)
    {
        struct Dispatched
        {
            Ptr self;
            Timeout timeout;
            AdmitHandler handler;
            void operator()() {self->onShed(timeout, std::move(handler));}
        };

        assert(state_ == State::initial);
        state_ = State::shedding;
        boost::asio::dispatch(Dispatched{shared_from_this(), timeout,
                                         std::move(handler)});
    }

    /** Starts the transport's I/O operations.
        @pre this->state() == TransportState::initial
        @post this->state() == TransportState::running */
    void start(RxHandler rxHandler, TxErrorHandler txHandler)
    {
        assert(state_ == State::ready);
        onStart(std::move(rxHandler), std::move(txHandler));
        state_ = State::running;
    }

    /** Sends the given serialized message via the transport.
        @pre this->state() != TransportState::initial */
    void send(MessageBuffer message)
    {
        assert(state_ != State::initial);
        if (state_ == State::running)
            onSend(std::move(message));
    }

    /** Set the post-abort closing handshake timeout period. */
    void setAbortTimeout(Timeout abortTimeout)
    {
        onSetAbortTimeout(abortTimeout);
    }

    /** Sends the given serialized ABORT message, placing it at the top of the
        queue, then gracefully closes the underlying socket.
        @pre this->state() != TransportState::initial
        @post this->state() == TransportState::stopped */
    void sendAbort(MessageBuffer abortMessage)
    {
        assert(state_ != TransportState::initial);
        if (state_ == State::running)
            onSendAbort(std::move(abortMessage));
        state_ = State::stopped;
    }

    /** Stops I/O operations and gracefully closes the underlying socket.
        Emits `true` upon successful completion if the transport was
        handshaking, ready, or running. Emits `false` if the transport was
        not in a valid state to be closed. */
    void close(CloseHandler handler)
    {
        switch (state_)
        {
            case State::ready:
            case State::running:
                onClose(std::move(handler));
                break;

            case State::accepting:
            case State::shedding:
                onCancelHandshake();
                // Fall through
            default:
                post(std::move(handler), false);
                state_ = State::stopped;
                break;
        }
    }

    /** Stops I/O operations and abrubtly closes the underlying socket.
        @post `this->state() == TransportState::stopped` */
    void stop()
    {
        switch (state_)
        {
        case State::accepting:
        case State::shedding:
            onCancelHandshake();
            break;

        case State::ready:
        case State::running:
            onStop();
            break;

        default:
            break;
        }

        state_ = State::stopped;
    }

protected:
    Transporting(IoStrand strand, ConnectionInfo ci, TransportInfo ti = {})
        : strand_(std::move(strand)),
          info_(ti),
          connectionInfo_(std::move(ci))
    {
        if (ti.codecId() != 0)
            state_ = State::ready;
    }

    /** Must be overridden by server transports to initiate the handshake. */
    virtual void onAccept(Timeout, AdmitHandler)
    {
        assert(false && "Not a server transport");
    }

    /** May be overridden by server transports to shed the connection
        due to overload. */
    virtual void onShed(Timeout timeout, AdmitHandler handler)
    {
        // state_ will be State::shedding when the following is called.
        onAccept(timeout, std::move(handler));
    }

    /** Must be overridden by server transports to cancel a handshake. */
    virtual void onCancelHandshake()
    {
        assert(false && "Not a server transport");
    }

    /** Must be overridden to start the transport's I/O operations. */
    virtual void onStart(RxHandler rxHandler, TxErrorHandler txHandler) = 0;

    /** Must be overridden to send the given serialized message. */
    virtual void onSend(MessageBuffer message) = 0;

    /** May be overriden to set the post-abort closing handshake
        timeout period. */
    virtual void onSetAbortTimeout(Timeout) {}

    /** Must be overriden to send the given serialized ABORT message ASAP and
        then close gracefully. */
    virtual void onSendAbort(MessageBuffer abortMessage) = 0;

    /** May be overriden to stop I/O operations and gracefully close. */
    virtual void onClose(CloseHandler handler)
    {
        onStop();
        post(std::move(handler), true);
    }

    /** Must be overriden to stop I/O operations and abtruptly disconnect. */
    virtual void onStop() = 0;

    /** Should be called by derived server classes after transport details
        have been negotiated successfully. */
    void completeAccept(TransportInfo ti)
    {
        info_ = ti;
        state_ = State::ready;
    }

    /** Should be called by derived classes when the transport
        fails/disconnects. */
    void shutdown()
    {
        connectionInfo_ = {};
        state_ = State::stopped;
    }

    template <typename F, typename... Ts>
    void post(F&& handler, Ts&&... args)
    {
        postAny(strand_, std::forward<F>(handler), std::forward<Ts>(args)...);
    }

private:
    IoStrand strand_;
    TransportInfo info_;
    ConnectionInfo connectionInfo_;
    State state_ = State::initial;
};

} // namespace wamp

#endif // CPPWAMP_TRANSPORT_HPP
