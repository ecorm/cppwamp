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
#include "timeout.hpp"
#include "traits.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains information pertaining to a transport. */
//------------------------------------------------------------------------------
class CPPWAMP_API TransportInfo
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
class CPPWAMP_API BodyTimeout
{
public:
    BodyTimeout() = default;

    BodyTimeout(Timeout max) : max_(internal::checkTimeout(max)) {}

    BodyTimeout(Timeout min, size_t minRate, Timeout max = unspecifiedTimeout)
        : min_(internal::checkTimeout(min)),
          max_(internal::checkTimeout(max)),
          minRate_(minRate)
    {}

    Timeout min() const {return min_;}

    Timeout max() const {return max_;}

    size_t minRate() const {return minRate_;}

private:
    Timeout min_ = unspecifiedTimeout;
    Timeout max_ = unspecifiedTimeout;
    size_t minRate_ = 0;
};

//------------------------------------------------------------------------------
/** Contains timeouts and size limits for client transports. */
//------------------------------------------------------------------------------
class CPPWAMP_API ClientLimits
{
public:
    ClientLimits& withBodySize(std::size_t n);

    ClientLimits& withControlSize(std::size_t n);

    ClientLimits& withLingerTimeout(Timeout t);

    std::size_t bodySize() const;

    std::size_t controlSize() const;

    Timeout lingerTimeout() const;

private:
    Timeout lingerTimeout_   = neverTimeout;
    std::size_t bodySize_    = 0;
    std::size_t controlSize_ = 0;
};

//------------------------------------------------------------------------------
/** Contains timeouts and size limits for server transports. */
//------------------------------------------------------------------------------
class CPPWAMP_API ServerLimits
{
public:
    ServerLimits& withHeaderSize(std::size_t n);

    ServerLimits& withBodySize(std::size_t n);

    ServerLimits& withControlSize(std::size_t n);

    ServerLimits& withHandshakeTimeout(Timeout t);

    ServerLimits& withHeaderTimeout(Timeout t);

    ServerLimits& withBodyTimeout(BodyTimeout t);

    ServerLimits& withSendTimeout(BodyTimeout t);

    ServerLimits& withIdleTimeout(Timeout t);

    ServerLimits& withLingerTimeout(Timeout t);

    ServerLimits& withBacklogCapacity(int n);

    ServerLimits& withPingKeepsAliveDisabled(bool disabled = true);

    // TODO: Header/Body limits for HTTP and Websocket only
    std::size_t headerSize() const;

    std::size_t bodySize() const;

    std::size_t controlSize() const;

    Timeout handshakeTimeout() const;

    Timeout headerTimeout() const;

    const BodyTimeout& bodyTimeout() const;

    const BodyTimeout& sendTimeout() const;

    Timeout idleTimeout() const;

    Timeout lingerTimeout() const;

    int backlogCapacity() const;

    bool pingKeepsAlive() const;

private:
    BodyTimeout bodyTimeout_;
    BodyTimeout sendTimeout_;
    Timeout handshakeTimeout_ = neverTimeout;
    Timeout headerTimeout_    = neverTimeout;
    Timeout idleTimeout_      = neverTimeout;
    Timeout lingerTimeout_    = neverTimeout;
    std::size_t headerSize_   = 0;
    std::size_t bodySize_     = 0;
    std::size_t controlSize_  = 0;
    int backlogCapacity_      = 0;
    bool pingKeepsAlive_      = true;
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
    using AdmitHandler = std::function<void (AdmitResult)>;

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
        @post `this->state() == TransportState::stopped` */
    void close();

protected:
    Transporting(IoStrand strand, ConnectionInfo ci, TransportInfo ti = {});

    /** Must be overridden by server transports to initiate the handshake. */
    virtual void onAdmit(AdmitHandler);

    /** May be overridden by server transports to shed the connection
        due to overload. */
    virtual void onShed(AdmitHandler handler);

    /** Must be overridden to start the transport's I/O operations. */
    virtual void onStart(RxHandler rxHandler, TxErrorHandler txHandler) = 0;

    /** Must be overridden to send the given serialized message. */
    virtual void onSend(MessageBuffer message) = 0;

    /** Must be overriden to send the given serialized ABORT message ASAP and
        then close gracefully. */
    virtual void onAbort(MessageBuffer abortMessage) = 0;

    /** Must be overriden to stop I/O operations and gracefully close. */
    virtual void onShutdown(std::error_code reason, ShutdownHandler f) = 0;

    /** Must be overriden to stop I/O operations and abtruptly disconnect. */
    virtual void onClose() = 0;

    /** Must be called by derived server classes after transport details
        have been negotiated successfully. */
    void setReady(TransportInfo ti);

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
