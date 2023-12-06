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
#include <limits>
#include <memory>
#include <mutex>
#include <system_error>
#include <vector>
#include "api.hpp"
#include "anyhandler.hpp"
#include "asiodefs.hpp"
#include "connectioninfo.hpp"
#include "erroror.hpp"
#include "messagebuffer.hpp"
#include "traits.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains negotiated information pertaining to a transport. */
//------------------------------------------------------------------------------
class CPPWAMP_API TransportInfo
{
public:
    /** Default constructor. */
    TransportInfo();

    /** Constructor taking information. */
    TransportInfo(int codecId, std::size_t sendLimit, std::size_t receiveLimit);

    /** Obtains the random transport instance ID generated upon construction. */
    uint64_t transportId() const;

    /** Obtains the codec numeric ID. */
    int codecId() const;

    /** Obtains the maximum allowable transmit message length. */
    std::size_t sendLimit() const;

    /** Obtains the maximum allowable receive message length. */
    std::size_t receiveLimit() const;

private:
    uint64_t transportId_ = 0;
    int codecId_ = 0;
    std::size_t sendLimit_ = 0;
    std::size_t receiveLimit_ = 0;
};


//------------------------------------------------------------------------------
/** Enumerates the possible transport states. */
//------------------------------------------------------------------------------
enum class TransportState
{
    initial,   /// Initial state of a server transport
    accepting, /// The server transport is performing its handshake
    shedding,  /// Server is handshaking but will ultimately shed connection
    rejected,  /// Transport handshake was rejected
    ready,     /// Transport handshake is complete (initial for client)
    running,   /// Sending and receiving of messages is enabled
    aborting,  /// Transport is sending an ABORT and shutting down
    shutdown,  /// Transport is performing its closing handshake
    closed     /// Transport has been closed
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
class CPPWAMP_API AdmitResult
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
    template <typename E,
              CPPWAMP_NEEDS(!std::is_error_condition_enum<E>::value) = 0>
    static AdmitResult failed(E netErrorCode, const char* operation)
    {
        return failed(static_cast<std::error_code>(netErrorCode), operation);
    }

    /** Constructs a result for a failed handshake I/O operation. */
    template <typename TErrc,
              CPPWAMP_NEEDS(std::is_error_condition_enum<TErrc>::value) = 0>
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

    /// Handler type used for transport shutdown completion.
    using ShutdownHandler = AnyCompletionHandler<void (std::error_code)>;

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

    /** Starts the server handshake procedure, but ultimately refuses the
        client connection due to server connection limit.
        Either an TransportErrc::saturated error will be emitted via the
        handler, or some other error due a handshake failure.
        @pre this->state() == TransportState::initial
        @post this->state() == TransportState::shedding */
    void shed(AdmitHandler handler);

    /** Called periodically on server transports to monitor their health. */
    std::error_code monitor();

    /** Starts the transport's I/O operations.
        @pre this->state() == TransportState::initial
        @post this->state() == TransportState::running */
    void start(RxHandler rxHandler, TxErrorHandler txHandler);

    /** Sends the given serialized message via the transport.
        @pre this->state() != TransportState::initial */
    void send(MessageBuffer message);

    /** Sends the given serialized ABORT message, placing it at the top of the
        queue, then gracefully shuts down the underlying socket.
        @pre this->state() != TransportState::initial */
    void abort(MessageBuffer abortMessage, ShutdownHandler handler);

    /** Stops I/O operations and gracefully shuts down the underlying
        socket.
        @pre this->state() != TransportState::initial */
    void shutdown(std::error_code reason, ShutdownHandler handler);

    /** Stops I/O operations and abrubtly closes the underlying socket.
        @post `this->state() == TransportState::closed` */
    void close();

protected:
    Transporting(IoStrand strand, ConnectionInfo ci, TransportInfo ti = {});

    /** Must be overridden by server transports to initiate the handshake. */
    virtual void onAdmit(AdmitHandler);

    /** May be overridden by server transports to shed the connection
        due to overload. */
    virtual void onShed(AdmitHandler handler);

    /** May be overridden by server transports to report on their health. */
    virtual std::error_code onMonitor();

    /** Must be overridden to start the transport's I/O operations. */
    virtual void onStart(RxHandler rxHandler, TxErrorHandler txHandler) = 0;

    /** Must be overridden to send the given serialized message. */
    virtual void onSend(MessageBuffer message) = 0;

    /** Must be overriden to send the given serialized ABORT message ASAP and
        then close gracefully. */
    virtual void onAbort(MessageBuffer abortMessage, ShutdownHandler f) = 0;

    /** Must be overriden to stop I/O operations and gracefully close. */
    virtual void onShutdown(std::error_code reason, ShutdownHandler f) = 0;

    /** Must be overriden to stop I/O operations and abtruptly disconnect. */
    virtual void onClose() = 0;

    /** Must be called by derived server classes after transport details
        have been negotiated successfully. */
    void setReady(TransportInfo ti);

    /** Must be called by derived server classes when negotiation results
        in rejection. */
    void setRejected();

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

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/transport.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORT_HPP
