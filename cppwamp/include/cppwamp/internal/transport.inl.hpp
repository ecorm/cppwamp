/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transport.hpp"
#include "../api.hpp"

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
    admit(unspecifiedTimeout, std::move(handler));
}

CPPWAMP_INLINE void Transporting::admit(Timeout timeout, AdmitHandler handler)
{
    assert(state_ == State::initial);
    onAdmit(timeout, std::move(handler));
    state_ = State::accepting;
}

CPPWAMP_INLINE void Transporting::shed(AdmitHandler handler)
{
    shed(unspecifiedTimeout, std::move(handler));
}

CPPWAMP_INLINE void Transporting::shed(Timeout timeout, AdmitHandler handler)
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

    // Needs to be dispatched via strand because this function is
    // invoked from RouterServer's execution context.
    boost::asio::dispatch(strand_, Dispatched{shared_from_this(), timeout,
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

CPPWAMP_INLINE void Transporting::setAbortTimeout(Timeout abortTimeout)
{
    onSetAbortTimeout(abortTimeout);
}

CPPWAMP_INLINE void Transporting::sendAbort(MessageBuffer abortMessage)
{
    assert(state_ != TransportState::initial);
    if (state_ == State::running)
        onSendAbort(std::move(abortMessage));
    state_ = State::stopped;
}

CPPWAMP_INLINE void Transporting::close(CloseHandler handler)
{
    switch (state_)
    {
        case State::ready:
        case State::running:
            onClose(std::move(handler));
            break;

        case State::accepting:
        case State::shedding:
            onCancelAdmission();
            // Fall through
        default:
            post(std::move(handler), false);
            state_ = State::stopped;
            break;
    }
}

CPPWAMP_INLINE void Transporting::stop()
{
    switch (state_)
    {
    case State::accepting:
    case State::shedding:
        onCancelAdmission();
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

CPPWAMP_INLINE Transporting::Transporting(IoStrand strand, ConnectionInfo ci,
                                          TransportInfo ti)
    : strand_(std::move(strand)),
      info_(ti),
      connectionInfo_(std::move(ci))
{
    if (ti.codecId() != 0)
        state_ = State::ready;
}

CPPWAMP_INLINE void Transporting::onAdmit(Timeout, AdmitHandler)
{
    assert(false && "Not a server transport");
}

CPPWAMP_INLINE void Transporting::onShed(Timeout timeout, AdmitHandler handler)
{
    // state_ will be State::shedding when the following is called.
    onAdmit(timeout, std::move(handler));
}

CPPWAMP_INLINE void Transporting::onCancelAdmission()
{
    assert(false && "Not a server transport");
}

CPPWAMP_INLINE void Transporting::onSetAbortTimeout(Timeout) {}

CPPWAMP_INLINE void Transporting::onClose(CloseHandler handler)
{
    onStop();
    post(std::move(handler), true);
}

CPPWAMP_INLINE void Transporting::completeAdmission(TransportInfo ti)
{
    info_ = ti;
    state_ = State::ready;
}

CPPWAMP_INLINE void Transporting::shutdown()
{
    connectionInfo_ = {};
    state_ = State::stopped;
}

} // namespace wamp
