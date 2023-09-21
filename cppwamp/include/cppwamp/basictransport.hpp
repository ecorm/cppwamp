/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_BASICTRANSPORT_HPP
#define CPPWAMP_BASICTRANSPORT_HPP

#include <deque>
#include <memory>
#include <utility>
#include <boost/asio/steady_timer.hpp>
#include "errorcodes.hpp"
#include "messagebuffer.hpp"
#include "transport.hpp"
#include "internal/transportframe.hpp"
#include "internal/pinger.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** CRTP base class providing outbound message queueing and ping/pong handling
    for transports.

    Derived class requirements:
    - `bool socketIsOpen() const;`
    - `void enablePinging();`
    - `void disablePinging();`
    - `void stopTransport();`
    - `void closeTransport(CloseHandler);`
    - `void cancelClose();`
    - `void stopTransport();`
    - `void failTransport(std::error_code);`
    - `template <typename F> void transmitMessage(TransportFrameKind, MessageBuffer&, F&& callback);`
    - `template <typename F> void receiveMessage(MessageBuffer&, F&& callback);` */
//------------------------------------------------------------------------------
template <typename TDerived>
class BasicTransport : public Transporting
{
public:
    using Ptr            = std::shared_ptr<BasicTransport>;
    using RxHandler      = typename Transporting::RxHandler;
    using TxErrorHandler = typename Transporting::TxErrorHandler;

protected:
    using Byte = MessageBuffer::value_type;
    using Pinger = internal::Pinger;

    BasicTransport(IoStrand strand, ConnectionInfo ci, TransportInfo ti = {})
        : Base(std::move(strand), ci, ti),
          timer_(Base::strand())
    {
        if (internal::timeoutIsDefinite(Base::info().heartbeatInterval()))
            pinger_ = std::make_shared<Pinger>(Base::strand(), Base::info());
    }

    void onPong(const Byte* data, std::size_t size)
    {
        if (pinger_)
            pinger_->pong(data, size);
    }

    void enqueuePong(MessageBuffer payload)
    {
        if (!derived().socketIsOpen())
            return;
        auto buf = enframe(std::move(payload), TransportFrameKind::pong);
        enqueueFrame(std::move(buf));
    }

    template <typename F>
    void timeoutAfter(Timeout timeout, F&& action)
    {
        timer_.expires_from_now(timeout);
        timer_.async_wait(std::forward<F>(action));
    }

private:
    using Base = Transporting;
    using Frame = internal::TransportFrame;
    using PingBytes = internal::PingBytes;

    void onStart(RxHandler rxHandler, TxErrorHandler txErrorHandler) override
    {
        rxHandler_ = rxHandler;
        txErrorHandler_ = txErrorHandler;

        if (pinger_)
        {
            std::weak_ptr<BasicTransport> self =
                std::dynamic_pointer_cast<BasicTransport>(shared_from_this());
            derived().enablePinging();
            pinger_->start(
                [self, this](ErrorOr<PingBytes> pingBytes)
                {
                    auto me = self.lock();
                    if (me)
                        onPingGeneratedOrTimedOut(pingBytes);
                });
        }

        receive();
    }

    void onSend(MessageBuffer message) override
    {
        if (!derived().socketIsOpen())
            return;
        auto buf = enframe(std::move(message));
        enqueueFrame(std::move(buf));
    }

    void onSetAbortTimeout(Timeout timeout) override
    {
        abortTimeout_ = timeout;
    }

    void onSendAbort(MessageBuffer message) override
    {
        if (!derived().socketIsOpen())
            return;
        auto frame = enframe(std::move(message));
        assert((frame.payload().size() <= info().maxTxLength()) &&
               "Outgoing message is longer than allowed by peer");
        frame.poison();
        txQueue_.push_front(std::move(frame));
        transmit();
    }

    void onStop() override
    {
        rxHandler_ = nullptr;
        txErrorHandler_ = nullptr;
        txQueue_.clear();
        if (pinger_)
            pinger_->stop();
        derived().stopTransport();
    }

    void onClose(CloseHandler handler) override
    {
        doClose(std::move(handler));
    }

    void doClose(CloseHandler handler)
    {
        rxHandler_ = nullptr;
        txErrorHandler_ = nullptr;
        txQueue_.clear();
        if (pinger_)
            pinger_->stop();
        derived().closeTransport(std::move(handler));
    }

    void onPingGeneratedOrTimedOut(ErrorOr<PingBytes> pingBytes)
    {
        if (state() != TransportState::running)
            return;

        if (!pingBytes.has_value())
            return fail(pingBytes.error());

        MessageBuffer message{pingBytes->begin(), pingBytes->end()};
        auto buf = enframe(std::move(message), TransportFrameKind::ping);
        enqueueFrame(std::move(buf));
    }

    Frame enframe(MessageBuffer&& payload,
                  TransportFrameKind kind = TransportFrameKind::wamp)
    {
        return Frame{std::move(payload), kind};
    }

    void enqueueFrame(Frame&& frame)
    {
        assert((frame.payload().size() <= info().maxTxLength()) &&
               "Outgoing message is longer than allowed by peer");
        txQueue_.emplace_back(std::move(frame));
        transmit();
    }

    void transmit()
    {
        if (!isReadyToTransmit())
            return;

        txFrame_ = std::move(txQueue_.front());
        txQueue_.pop_front();
        const auto kind = txFrame_.kind();
        if (kind == TransportFrameKind::wamp)
            sendWampMessage();
        else
            sendControlMessage(kind);
    }

    void sendWampMessage()
    {
        auto self = this->shared_from_this();
        isTransmitting_ = true;
        derived().transmitMessage(
            TransportFrameKind::wamp,
            txFrame_.payload(),
            [this, self](std::error_code ec)
            {
                isTransmitting_ = false;
                if (!checkTxError(ec))
                    return;
                if (txFrame_.isPoisoned())
                    abortiveClose();
                else
                    transmit();
            });
    }

    void sendControlMessage(TransportFrameKind kind)
    {
        auto self = this->shared_from_this();
        isTransmitting_ = true;
        derived().transmitMessage(
            kind,
            txFrame_.payload(),
            [this, self](std::error_code ec)
            {
                isTransmitting_ = false;
                if (!txQueue_.empty())
                    txQueue_.pop_front();
                if (checkTxError(ec))
                    transmit();
            });
    }

    void abortiveClose()
    {
        if (!internal::timeoutIsDefinite(abortTimeout_))
            return doClose([](ErrorOr<bool>) {});

        auto self = this->shared_from_this();
        std::weak_ptr<BasicTransport> weakSelf =
            std::dynamic_pointer_cast<BasicTransport>(self);

        timer_.expires_after(abortTimeout_);
        timer_.async_wait([weakSelf](boost::system::error_code ec)
        {
            if (ec == boost::asio::error::operation_aborted)
                return;

            auto self = weakSelf.lock();
            if (!self)
                return;

            self->derived().cancelClose();
        });

        derived().closeTransport(
            [this, self](ErrorOr<bool>) {timer_.cancel();});
    }

    bool isReadyToTransmit() const
    {
        return derived().socketIsOpen() && !isTransmitting_ &&
               !txQueue_.empty();
    }

    void receive()
    {
        if (!derived().socketIsOpen())
            return;

        rxBuffer_.clear();
        auto self = this->shared_from_this();
        derived().receiveMessage(
            rxBuffer_,
            [this, self](ErrorOr<bool> isWampMessage)
            {
                if (isWampMessage.has_value())
                    return onReceiveCompleted(isWampMessage.value());
                checkRxError(isWampMessage.error());
            });
    }

    void onReceiveCompleted(bool isWampMessage)
    {
        if (isWampMessage && rxHandler_)
            Base::post(rxHandler_, std::move(rxBuffer_));
        receive();
    }

    bool checkTxError(std::error_code ec)
    {
        if (!ec)
            return true;
        if (txErrorHandler_)
            post(txErrorHandler_, ec);
        cleanup();
        return false;
    }

    bool checkRxError(std::error_code ec)
    {
        if (!ec)
            return true;
        fail(ec);
        return false;
    }

    template <typename TErrc>
    void fail(TErrc errc)
    {
        fail(make_error_code(errc));
    }

    void fail(std::error_code ec)
    {
        if (rxHandler_)
            Base::post(rxHandler_, makeUnexpected(ec));
        derived().failTransport(ec);
        cleanup();
    }

    void cleanup()
    {
        Base::shutdown();
        derived().disablePinging();
        rxHandler_ = nullptr;
        txErrorHandler_ = nullptr;
        txQueue_.clear();
        pinger_.reset();
    }

    TDerived& derived() {return static_cast<TDerived&>(*this);}

    const TDerived& derived() const
    {
        return static_cast<const TDerived&>(*this);
    }

    boost::asio::steady_timer timer_;
    std::deque<Frame> txQueue_;
    Frame txFrame_;
    MessageBuffer rxBuffer_;
    RxHandler rxHandler_;
    TxErrorHandler txErrorHandler_;
    std::shared_ptr<Pinger> pinger_;
    Timeout abortTimeout_;
    bool isTransmitting_ = false;
};

} // namespace wamp

#endif // CPPWAMP_BASICTRANSPORT_HPP
