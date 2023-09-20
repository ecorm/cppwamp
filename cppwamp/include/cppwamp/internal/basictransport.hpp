/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_BASICTRANSPORT_HPP
#define CPPWAMP_INTERNAL_BASICTRANSPORT_HPP

#include <deque>
#include <memory>
#include <boost/asio/steady_timer.hpp>
#include "../errorcodes.hpp"
#include "../messagebuffer.hpp"
#include "../transport.hpp"
#include "pinger.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class TransportFrame
{
public:
    using Ptr = std::shared_ptr<TransportFrame>;

    TransportFrame() = default;

    TransportFrame(MessageBuffer&& payload, bool isPing = false)
        : payload_(std::move(payload)),
          isPing_(isPing)
    {}

    void clear()
    {
        payload_.clear();
        isPing_ = false;
        isPoisoned_ = false;
    }

    bool isPing() const {return isPing_;}

    const MessageBuffer& payload() const & {return payload_;}

    MessageBuffer&& payload() && {return std::move(payload_);}

    void poison(bool poisoned = true) {isPoisoned_ = poisoned;}

    bool isPoisoned() const {return isPoisoned_;}

private:
    MessageBuffer payload_;
    bool isPing_ = false;
    bool isPoisoned_ = false;
};

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

    BasicTransport(IoStrand strand, ConnectionInfo ci, TransportInfo ti = {})
        : Base(std::move(strand), ci, ti),
          timer_(Base::strand())
    {
        if (timeoutIsDefinite(Base::info().heartbeatInterval()))
            pinger_ = std::make_shared<Pinger>(Base::strand(), Base::info());
    }

    void onPong(const Byte* data, std::size_t size)
    {
        if (pinger_)
            pinger_->pong(data, size);
    }

    template <typename F>
    void timeoutAfter(Timeout timeout, F&& action)
    {
        timer_.expires_from_now(timeout);
        timer_.async_wait(std::forward<F>(action));
    }

private:
    using Base          = Transporting;
    using TransmitQueue = std::deque<TransportFrame::Ptr>;

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
        sendFrame(std::move(buf));
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
        assert((frame->payload().size() <= info().maxTxLength()) &&
               "Outgoing message is longer than allowed by peer");
        frame->poison();
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
        auto buf = enframe(std::move(message), true);
        sendFrame(std::move(buf));
    }

    TransportFrame::Ptr enframe(MessageBuffer&& payload, bool isPing = false)
    {
        // TODO: Pool/reuse frames somehow
        return std::make_shared<TransportFrame>(std::move(payload), isPing);
    }

    void sendFrame(TransportFrame::Ptr frame)
    {
        assert((frame->payload().size() <= info().maxTxLength()) &&
               "Outgoing message is longer than allowed by peer");
        txQueue_.push_back(std::move(frame));
        transmit();
    }

    void transmit()
    {
        if (!isReadyToTransmit())
            return;

        txFrame_ = txQueue_.front();
        txQueue_.pop_front();
        if (txFrame_->isPing())
            sendPing();
        else
            sendWampMessage();
    }

    void sendPing()
    {
        auto self = this->shared_from_this();
        derived().transmitPing(
            txFrame_->payload(),
            [this, self](std::error_code ec)
            {
                txFrame_.reset();
                if (checkTxError(ec))
                    transmit();
            });
    }

    void sendWampMessage()
    {
        auto self = this->shared_from_this();
        derived().transmitMessage(
            txFrame_->payload(),
            [this, self](std::error_code ec)
            {
                auto frame = std::move(txFrame_);
                txFrame_.reset();
                if (!checkTxError(ec))
                    return;
                if (frame && frame->isPoisoned())
                    abortiveClose();
                else
                    transmit();
            });
    }

    void abortiveClose()
    {
        if (!timeoutIsDefinite(abortTimeout_))
            return onClose(nullptr);

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
        return derived().socketIsOpen() &&
               !txFrame_ &&       // No async_write is in progress
               !txQueue_.empty(); // One or more messages are enqueued
    }

    void receive()
    {
        if (!derived().socketIsOpen())
            return;

        rxBuffer_.clear();
        auto self = this->shared_from_this();
        derived().receiveMessage(
            rxBuffer_,
            [this, self](std::error_code ec)
            {
                if (checkRxError(ec))
                    onReceiveCompleted();
            });
    }

    void onReceiveCompleted()
    {
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
        txFrame_ = nullptr;
        pingFrame_ = nullptr;
        pinger_.reset();
    }

    TDerived& derived() {return static_cast<TDerived&>(*this);}

    const TDerived& derived() const
    {
        return static_cast<const TDerived&>(*this);
    }

    boost::asio::steady_timer timer_;
    RxHandler rxHandler_;
    TxErrorHandler txErrorHandler_;
    MessageBuffer rxBuffer_;
    TransmitQueue txQueue_;
    MessageBuffer pingBuffer_;
    TransportFrame::Ptr txFrame_;
    TransportFrame::Ptr pingFrame_;
    std::shared_ptr<Pinger> pinger_;
    Timeout abortTimeout_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_BASICTRANSPORT_HPP
