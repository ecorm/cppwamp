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
// ClientTransportLimits
//******************************************************************************

CPPWAMP_INLINE ClientTransportLimits&
ClientTransportLimits::withBodySizeLimit(std::size_t n)
{
    bodySizeLimit_ = n;
    return *this;
}

CPPWAMP_INLINE ClientTransportLimits&
ClientTransportLimits::withControlSizeLimit(std::size_t n)
{
    controlSizeLimit_ = n;
    return *this;
}

CPPWAMP_INLINE ClientTransportLimits&
ClientTransportLimits::withLingerTimeout(Timeout t)
{
    lingerTimeout_ = t;
    return *this;
}

CPPWAMP_INLINE std::size_t ClientTransportLimits::bodySizeLimit() const
{
    return bodySizeLimit_;
}

CPPWAMP_INLINE std::size_t ClientTransportLimits::controlSizeLimit() const
{
    return controlSizeLimit_;
}

CPPWAMP_INLINE Timeout ClientTransportLimits::lingerTimeout() const
{
    return lingerTimeout_;
}


//******************************************************************************
// ServerTransportLimits
//******************************************************************************

CPPWAMP_INLINE ServerTransportLimits&
ServerTransportLimits::withHeaderSizeLimit(std::size_t n)
{
    headerSizeLimit_ = n;
    return *this;
}

CPPWAMP_INLINE ServerTransportLimits&
ServerTransportLimits::withBodySizeLimit(std::size_t n)
{
    bodySizeLimit_ = n;
    return *this;
}

CPPWAMP_INLINE ServerTransportLimits&
ServerTransportLimits::withControlSizeLimit(std::size_t n)
{
    controlSizeLimit_ = n;
    return *this;
}

CPPWAMP_INLINE ServerTransportLimits&
ServerTransportLimits::withHandshakeTimeout(Timeout t)
{
    handshakeTimeout_ = internal::checkTimeout(t);
    return *this;
}

CPPWAMP_INLINE ServerTransportLimits&
ServerTransportLimits::withHeaderTimeout(Timeout t)
{
    headerTimeout_ = internal::checkTimeout(t);
    return *this;
}

CPPWAMP_INLINE ServerTransportLimits&
ServerTransportLimits::withBodyTimeout(BodyTimeout t)
{
    bodyTimeout_ = t;
    return *this;
}

CPPWAMP_INLINE ServerTransportLimits&
ServerTransportLimits::withSendTimeout(BodyTimeout t)
{
    sendTimeout_ = t;
    return *this;
}

CPPWAMP_INLINE ServerTransportLimits&
ServerTransportLimits::withIdleTimeout(Timeout t)
{
    idleTimeout_ = internal::checkTimeout(t);
    return *this;
}

CPPWAMP_INLINE ServerTransportLimits&
ServerTransportLimits::withLingerTimeout(Timeout t)
{
    lingerTimeout_ = internal::checkTimeout(t);
    return *this;
}

CPPWAMP_INLINE ServerTransportLimits&
ServerTransportLimits::withBacklogCapacity(int n)
{
    CPPWAMP_LOGIC_CHECK(n > 0, "Backlog capacity must be positive");
    backlogCapacity_ = n;
    return *this;
}

CPPWAMP_INLINE ServerTransportLimits&
ServerTransportLimits::withPingKeepsAliveDisabled(bool disabled)
{
    pingKeepsAlive_ = disabled;
    return *this;
}

CPPWAMP_INLINE std::size_t ServerTransportLimits::headerSizeLimit() const
{
    return headerSizeLimit_;
}

CPPWAMP_INLINE std::size_t ServerTransportLimits::bodySizeLimit() const
{
    return bodySizeLimit_;
}

CPPWAMP_INLINE std::size_t ServerTransportLimits::controlSizeLimit() const
{
    return controlSizeLimit_;
}

CPPWAMP_INLINE Timeout ServerTransportLimits::handshakeTimeout() const
{
    return handshakeTimeout_;
}

CPPWAMP_INLINE Timeout ServerTransportLimits::headerTimeout() const
{
    return headerTimeout_;
}

CPPWAMP_INLINE const BodyTimeout& ServerTransportLimits::bodyTimeout() const
{
    return bodyTimeout_;
}

CPPWAMP_INLINE const BodyTimeout& ServerTransportLimits::sendTimeout() const
{
    return sendTimeout_;
}

CPPWAMP_INLINE Timeout ServerTransportLimits::idleTimeout() const
{
    return idleTimeout_;
}

CPPWAMP_INLINE Timeout ServerTransportLimits::lingerTimeout() const
{
    return lingerTimeout_;
}

CPPWAMP_INLINE int ServerTransportLimits::withBacklogCapacity() const
{
    return backlogCapacity_;
}

CPPWAMP_INLINE bool ServerTransportLimits::pingKeepsAlive() const
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

    onSendAbort(std::move(abortMessage));
    state_ = State::aborting;
}

CPPWAMP_INLINE void Transporting::shutdown(ShutdownHandler handler)
{
    assert(state_ != TransportState::initial);
    if (state_ != State::ready && state_ != State::running)
    {
        return post(std::move(handler),
                    make_error_code(MiscErrc::invalidState));
    }

    return onShutdown(std::move(handler));
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
