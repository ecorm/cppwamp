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

// TODO: inl file

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains information pertaining to a transport. */
//------------------------------------------------------------------------------
class TransportInfo
{
public:
    /** Default constructor. */
    TransportInfo();

    /** Constructor taking information. */
    TransportInfo(int codecId, std::size_t maxTxLength, std::size_t maxRxLength,
                  Timeout heartbeatInterval = {});

    /** Obtains the random transport instance ID. */
    uint64_t transportId() const;

    /** Obtains the codec numeric ID. */
    int codecId() const;

    /** Obtains the maximum allowable transmit message length. */
    std::size_t maxTxLength() const;

    /** Obtains the maximum allowable receive message length. */
    std::size_t maxRxLength() const;

    /** Obtains the keep-alive heartbeat interval period. */
    Timeout heartbeatInterval() const;

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
/** Enumerates the possible transport admission statuses. */
//------------------------------------------------------------------------------
enum class AdmitStatus
{
    unknown,   /// Result has not been set
    responded, /// Request (e.g. HTTP GET) has been successfully responded to
    wamp,      /// WAMP codec successfully negotiated
    shedded,   /// Connection limit reached
    rejected,  /// Rejected due to client protocol error or timeout
    failed     /// Failed due to an I/O problem
};

//------------------------------------------------------------------------------
/** Contains the outcome of a server handshake attempt. */
//------------------------------------------------------------------------------
class AdmitResult
{
public:
    using Status = AdmitStatus;

    /** Constructs a result for request successfully responded to. */
    static AdmitResult responded();

    /** Constructs a result for WAMP codec successfully negotiated. */
    static AdmitResult wamp(int codecId);

    /** Constructs a result for connection limit exceeded. */
    static AdmitResult shedded();

    /** Constructs a result for a rejected client. */
    static AdmitResult rejected(std::error_code e);

    /** Constructs a result for a rejected client. */
    template <typename TErrc>
    static AdmitResult rejected(TErrc e)
    {
        return rejected(static_cast<std::error_code>(make_error_code(e)));
    }

    /** Constructs a result for a failed handshake I/O operation. */
    static AdmitResult failed(std::error_code e, const char* operation);

    /** Constructs a result for a failed handshake I/O operation. */
    template <typename TErrc>
    static AdmitResult failed(TErrc e, const char* operation)
    {
        return failed(static_cast<std::error_code>(make_error_code(e)),
                      operation);
    }

    /** Default constructor. */
    AdmitResult();

    /** Obtains the status of the handshake operation. */
    Status status() const;

    /** Obtains the codec ID that was negotiated. */
    const int codecId() const;

    /** Obtains the error code associated with a handshake failure
        or rejection. */
    std::error_code error() const;

    /** Obtains the reason for client rejection.
        @pre `this->status() == AdmitStatus::rejected` */
    const char* reason() const;

    /** Obtains the name of the handshake I/O operation that failed.
        @pre `this->status() == AdmitStatus::failed` */
    const char* operation() const;

private:
    AdmitResult(Status status, int codecId);

    AdmitResult(Status status, std::error_code e, const char* what);

    std::error_code error_;
    const char* what_ = nullptr;
    int codecId_ = 0;
    Status status_ = AdmitStatus::unknown;
};


// Forward declaration
namespace internal { class HttpServerTransport; }

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
    using AdmitHandler = AnyCompletionHandler<void (AdmitResult)>;

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
    virtual ~Transporting();

    /** Obtains the execution strand associated with this transport. */
    const IoStrand& strand() const;

    /** Obtains the current transport state. */
    State state() const;

    /** Obtains information pertaining to this transport. */
    const TransportInfo& info() const;

    /** Obtains connection information. */
    const ConnectionInfo& connectionInfo() const;

    /** Starts the server handshake procedure to admit a new client connection.
        @pre this->state() == TransportState::initial
        @post this->state() == TransportState::accepting */
    void admit(AdmitHandler handler);

    /** Starts the server handshake procedure with the given timeout.
        @copydetails Transporting::admit(AdmitHandler) */
    void admit(Timeout timeout, AdmitHandler handler);

    /** Starts the server handshake procedure, but ultimately refuses the
        client connection due to server connection limit.
        Either an TransportErrc::saturated error will be emitted via the
        handler, or some other error due a handshake failure.
        @pre this->state() == TransportState::initial
        @post this->state() == TransportState::shedding */
    void shed(AdmitHandler handler);

    /** Starts refusal handshake with the given timeout.
        @copydetails Transporting::shed(AdmitHandler) */
    void shed(Timeout timeout, AdmitHandler handler);

    /** Starts the transport's I/O operations.
        @pre this->state() == TransportState::initial
        @post this->state() == TransportState::running */
    void start(RxHandler rxHandler, TxErrorHandler txHandler);

    /** Sends the given serialized message via the transport.
        @pre this->state() != TransportState::initial */
    void send(MessageBuffer message);

    /** Set the post-abort closing handshake timeout period. */
    void setAbortTimeout(Timeout abortTimeout);

    /** Sends the given serialized ABORT message, placing it at the top of the
        queue, then gracefully closes the underlying socket.
        @pre this->state() != TransportState::initial
        @post this->state() == TransportState::stopped */
    void sendAbort(MessageBuffer abortMessage);

    /** Stops I/O operations and gracefully closes the underlying socket.
        Emits `true` upon successful completion if the transport was
        handshaking, ready, or running. Emits `false` if the transport was
        not in a valid state to be closed. */
    void close(CloseHandler handler);

    /** Stops I/O operations and abrubtly closes the underlying socket.
        @post `this->state() == TransportState::stopped` */
    void stop();

protected:
    Transporting(IoStrand strand, ConnectionInfo ci, TransportInfo ti = {});

    /** Must be overridden by server transports to initiate the handshake. */
    virtual void onAdmit(Timeout, AdmitHandler);

    /** May be overridden by server transports to shed the connection
        due to overload. */
    virtual void onShed(Timeout timeout, AdmitHandler handler);

    /** Must be overridden by server transports to cancel a handshake. */
    virtual void onCancelAdmission();

    /** Must be overridden to start the transport's I/O operations. */
    virtual void onStart(RxHandler rxHandler, TxErrorHandler txHandler) = 0;

    /** Must be overridden to send the given serialized message. */
    virtual void onSend(MessageBuffer message) = 0;

    /** May be overriden to set the post-abort closing handshake
        timeout period. */
    virtual void onSetAbortTimeout(Timeout);

    /** Must be overriden to send the given serialized ABORT message ASAP and
        then close gracefully. */
    virtual void onSendAbort(MessageBuffer abortMessage) = 0;

    /** May be overriden to stop I/O operations and gracefully close. */
    virtual void onClose(CloseHandler handler);

    /** Must be overriden to stop I/O operations and abtruptly disconnect. */
    virtual void onStop() = 0;

    /** Should be called by derived server classes after transport details
        have been negotiated successfully. */
    void completeAdmission(TransportInfo ti);

    /** Should be called by derived classes when the transport
        fails/disconnects. */
    void shutdown();

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

    friend class internal::HttpServerTransport;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/transport.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORT_HPP
