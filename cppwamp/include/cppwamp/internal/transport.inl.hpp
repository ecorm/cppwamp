/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transport.hpp"
#include "../api.hpp"
#include "../errorcodes.hpp"
#include "../exceptions.hpp"
#include "random.hpp"

namespace wamp
{

//******************************************************************************
// TransportInfo
//******************************************************************************

CPPWAMP_INLINE TransportInfo::TransportInfo() = default;

CPPWAMP_INLINE TransportInfo::TransportInfo(
    int codecId, std::size_t maxTxLength, std::size_t maxRxLength,
    Timeout heartbeatInterval)
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

CPPWAMP_INLINE uint64_t TransportInfo::transportId() const
{
    return transportId_;
}

CPPWAMP_INLINE int TransportInfo::codecId() const {return codecId_;}

CPPWAMP_INLINE std::size_t TransportInfo::maxTxLength() const
{
    return maxTxLength_;
}

CPPWAMP_INLINE std::size_t TransportInfo::maxRxLength() const
{
    return maxRxLength_;
}

CPPWAMP_INLINE Timeout TransportInfo::heartbeatInterval() const
{
    return heartbeatInterval_;
}


//******************************************************************************
// AdmitResult
//******************************************************************************

CPPWAMP_INLINE AdmitResult AdmitResult::responded()
{
    return AdmitResult{Status::responded, 0};
}

CPPWAMP_INLINE AdmitResult AdmitResult::wamp(int codecId)
{
    return AdmitResult{Status::wamp, codecId};
}

CPPWAMP_INLINE AdmitResult AdmitResult::shedded()
{
    return AdmitResult{Status::shedded, 0};
}

CPPWAMP_INLINE AdmitResult AdmitResult::rejected(std::error_code e)
{
    return AdmitResult{Status::rejected, e, nullptr};
}

CPPWAMP_INLINE AdmitResult AdmitResult::failed(std::error_code e,
                                               const char* operation)
{
    return AdmitResult{Status::failed, e, operation};
}

CPPWAMP_INLINE AdmitResult::AdmitResult() = default;

CPPWAMP_INLINE AdmitResult::Status AdmitResult::status() const {return status_;}

CPPWAMP_INLINE const int AdmitResult::codecId() const {return codecId_;}

CPPWAMP_INLINE std::error_code AdmitResult::error() const {return error_;}

CPPWAMP_INLINE const char* AdmitResult::reason() const
{
    assert(status_ == AdmitStatus::rejected);
    return what_;
}

CPPWAMP_INLINE const char* AdmitResult::operation() const
{
    assert(status_ == AdmitStatus::failed);
    return what_;
}

CPPWAMP_INLINE AdmitResult::AdmitResult(Status status, int codecId)
    : codecId_(codecId),
      status_(status)
{}

CPPWAMP_INLINE AdmitResult::AdmitResult(Status status, std::error_code e,
                                        const char* what)
    : error_(e),
      what_(what),
      status_(status)
{}


//******************************************************************************
// ClientLimits
//******************************************************************************

CPPWAMP_INLINE ClientLimits& ClientLimits::withBodySize(std::size_t n)
{
    bodySize_ = n;
    return *this;
}

CPPWAMP_INLINE ClientLimits& ClientLimits::withControlSize(std::size_t n)
{
    controlSize_ = n;
    return *this;
}

CPPWAMP_INLINE ClientLimits& ClientLimits::withLingerTimeout(Timeout t)
{
    lingerTimeout_ = t;
    return *this;
}

CPPWAMP_INLINE std::size_t ClientLimits::bodySize() const
{
    return bodySize_;
}

CPPWAMP_INLINE std::size_t ClientLimits::controlSize() const
{
    return controlSize_;
}

CPPWAMP_INLINE Timeout ClientLimits::lingerTimeout() const
{
    return lingerTimeout_;
}


//******************************************************************************
// ServerLimits
//******************************************************************************

CPPWAMP_INLINE ServerLimits& ServerLimits::withHeaderSize(std::size_t n)
{
    headerSize_ = n;
    return *this;
}

CPPWAMP_INLINE ServerLimits& ServerLimits::withBodySize(std::size_t n)
{
    bodySize_ = n;
    return *this;
}

CPPWAMP_INLINE ServerLimits& ServerLimits::withControlSize(std::size_t n)
{
    controlSize_ = n;
    return *this;
}

CPPWAMP_INLINE ServerLimits& ServerLimits::withHandshakeTimeout(Timeout t)
{
    handshakeTimeout_ = internal::checkTimeout(t);
    return *this;
}

CPPWAMP_INLINE ServerLimits& ServerLimits::withHeaderTimeout(Timeout t)
{
    headerTimeout_ = internal::checkTimeout(t);
    return *this;
}

CPPWAMP_INLINE ServerLimits& ServerLimits::withBodyTimeout(BodyTimeout t)
{
    bodyTimeout_ = t;
    return *this;
}

CPPWAMP_INLINE ServerLimits& ServerLimits::withSendTimeout(BodyTimeout t)
{
    sendTimeout_ = t;
    return *this;
}

CPPWAMP_INLINE ServerLimits& ServerLimits::withIdleTimeout(Timeout t)
{
    idleTimeout_ = internal::checkTimeout(t);
    return *this;
}

CPPWAMP_INLINE ServerLimits& ServerLimits::withLingerTimeout(Timeout t)
{
    lingerTimeout_ = internal::checkTimeout(t);
    return *this;
}

CPPWAMP_INLINE ServerLimits& ServerLimits::withBacklogCapacity(int n)
{
    CPPWAMP_LOGIC_CHECK(n > 0, "Backlog capacity must be positive");
    backlogCapacity_ = n;
    return *this;
}

CPPWAMP_INLINE ServerLimits&
ServerLimits::withPingKeepsAliveDisabled(bool disabled)
{
    pingKeepsAlive_ = disabled;
    return *this;
}

CPPWAMP_INLINE std::size_t ServerLimits::headerSize() const
{
    return headerSize_;
}

CPPWAMP_INLINE std::size_t ServerLimits::bodySize() const {return bodySize_;}

CPPWAMP_INLINE std::size_t ServerLimits::controlSize() const
{
    return controlSize_;
}

CPPWAMP_INLINE Timeout ServerLimits::handshakeTimeout() const
{
    return handshakeTimeout_;
}

CPPWAMP_INLINE Timeout ServerLimits::headerTimeout() const
{
    return headerTimeout_;
}

CPPWAMP_INLINE const BodyTimeout& ServerLimits::bodyTimeout() const
{
    return bodyTimeout_;
}

CPPWAMP_INLINE const BodyTimeout& ServerLimits::sendTimeout() const
{
    return sendTimeout_;
}

CPPWAMP_INLINE Timeout ServerLimits::idleTimeout() const
{
    return idleTimeout_;
}

CPPWAMP_INLINE Timeout ServerLimits::lingerTimeout() const
{
    return lingerTimeout_;
}

CPPWAMP_INLINE int ServerLimits::backlogCapacity() const
{
    return backlogCapacity_;
}

CPPWAMP_INLINE bool ServerLimits::pingKeepsAlive() const
{
    return pingKeepsAlive_;
}


// Forward declaration
namespace internal { class HttpServerTransport; }


//******************************************************************************
// Transporting
//******************************************************************************

CPPWAMP_INLINE Transporting::~Transporting() = default;

CPPWAMP_INLINE const IoStrand& Transporting::strand() const {return strand_;}

CPPWAMP_INLINE Transporting::State Transporting::state() const {return state_;}

CPPWAMP_INLINE const TransportInfo& Transporting::info() const {return info_;}

CPPWAMP_INLINE const ConnectionInfo& Transporting::connectionInfo() const
{
    return connectionInfo_;
}

CPPWAMP_INLINE void Transporting::admit(AdmitHandler handler)
{
    assert(state_ == State::initial);
    onAdmit(std::move(handler));
    state_ = State::accepting;
}

CPPWAMP_INLINE void Transporting::shed(AdmitHandler handler)
{
    struct Dispatched
    {
        Ptr self;
        AdmitHandler handler;
        void operator()() {self->onShed(std::move(handler));}
    };

    assert(state_ == State::initial);
    state_ = State::shedding;

    // Needs to be dispatched via strand because this function is
    // invoked from RouterServer's execution context.
    boost::asio::dispatch(strand_, Dispatched{shared_from_this(),
                                              std::move(handler)});
}

CPPWAMP_INLINE void Transporting::start(RxHandler rxHandler,
                                        TxErrorHandler txHandler)
{
    assert(state_ == State::ready);
    onStart(std::move(rxHandler), std::move(txHandler));
    state_ = State::running;
}

CPPWAMP_INLINE void Transporting::send(MessageBuffer message)
{
    assert(state_ != State::initial);
    if (state_ == State::running)
        onSend(std::move(message));
}

CPPWAMP_INLINE void Transporting::abort(MessageBuffer abortMessage,
                                        ShutdownHandler handler)
{
    assert(state_ != TransportState::initial);
    if (state_ != State::running)
    {
        return post(std::move(handler),
                    make_error_code(MiscErrc::invalidState));
    }

    onAbort(std::move(abortMessage));
    state_ = State::aborting;
}

CPPWAMP_INLINE void Transporting::shutdown(std::error_code reason,
                                           ShutdownHandler handler)
{
    assert(state_ != TransportState::initial);
    if (state_ != State::ready && state_ != State::running)
    {
        return post(std::move(handler),
                    make_error_code(MiscErrc::invalidState));
    }

    return onShutdown(reason, std::move(handler));
}

CPPWAMP_INLINE void Transporting::close()
{
    if (state_ != State::closed)
        onClose();
    state_ = State::closed;
    connectionInfo_ = {};
}

CPPWAMP_INLINE Transporting::Transporting(IoStrand strand, ConnectionInfo ci,
                                          TransportInfo ti)
    : strand_(std::move(strand)),
      info_(ti),
      connectionInfo_(std::move(ci))
{
    if (ti.codecId() != 0)
        state_ = State::ready;
}

CPPWAMP_INLINE void Transporting::onAdmit(AdmitHandler)
{
    assert(false && "Not a server transport");
}

CPPWAMP_INLINE void Transporting::onShed(AdmitHandler handler)
{
    // state_ will be State::shedding when the following is called.
    onAdmit(std::move(handler));
}

CPPWAMP_INLINE void Transporting::setReady(TransportInfo ti)
{
    info_ = ti;
    state_ = State::ready;
}

} // namespace wamp
