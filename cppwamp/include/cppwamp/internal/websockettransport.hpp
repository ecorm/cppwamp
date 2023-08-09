/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_WEBSOCKETTRANSPORT_HPP
#define CPPWAMP_INTERNAL_WEBSOCKETTRANSPORT_HPP

#include <array>
#include <deque>
#include <memory>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream.hpp>
#include "../asiodefs.hpp"
#include "../errorcodes.hpp"
#include "../messagebuffer.hpp"
#include "../transport.hpp"
#include "endian.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
enum class WebsocketMsgKind
{
    wamp,
    ping,
    pong
};

//------------------------------------------------------------------------------
class WebsocketFrame
{
public:
    using Ptr = std::shared_ptr<WebsocketFrame>;

    WebsocketFrame() = default;

    WebsocketFrame(WebsocketMsgKind kind, MessageBuffer&& payload)
        : payload_(std::move(payload)),
          kind_(kind)
    {}

    void clear()
    {
        payload_.clear();
        kind_ = {};
        isPoisoned_ = false;
    }

    WebsocketMsgKind kind() const {return kind_;}

    const MessageBuffer& payload() const & {return payload_;}

    MessageBuffer&& payload() && {return std::move(payload_);}

    void poison(bool poisoned = true) {isPoisoned_ = poisoned;}

    bool isPoisoned() const {return isPoisoned_;}

    boost::asio::const_buffer payloadBuffer() const
    {
        return boost::asio::buffer(&payload_.front(), payload_.size());
    }

private:
    MessageBuffer payload_;
    WebsocketMsgKind kind_;
    bool isPoisoned_ = false;
};

//------------------------------------------------------------------------------
using WebsocketPingBytes = std::array<MessageBuffer::value_type,
                                      2*sizeof(uint64_t)>;

//------------------------------------------------------------------------------
class WebsocketPingFrame
{
public:
    explicit WebsocketPingFrame(uint64_t randomId)
        : baseId_(endian::nativeToBig64(randomId))
    {}

    uint64_t count() const {return sequentialId_;}

    void serialize(WebsocketPingBytes& bytes) const
    {
        auto* ptr = bytes.data();
        std::memcpy(ptr, &baseId_, sizeof(baseId_));
        ptr += sizeof(baseId_);
        uint64_t n = endian::nativeToBig64(sequentialId_);
        std::memcpy(ptr, &n, sizeof(sequentialId_));
    }

    void increment() {++sequentialId_;}

private:
    uint64_t baseId_ = 0;
    uint64_t sequentialId_ = 0;
};

//------------------------------------------------------------------------------
// TODO: Consolidate with RawsockPinger
//------------------------------------------------------------------------------
class WebsocketPinger
    : public std::enable_shared_from_this<WebsocketPinger>
{
public:
    using Handler = std::function<void (ErrorOr<WebsocketPingBytes>)>;

    WebsocketPinger(IoStrand strand, const TransportInfo& info)
        : timer_(strand),
          frame_(info.transportId()),
          interval_(info.heartbeatInterval())
    {}

    void start(Handler handler)
    {
        handler_ = std::move(handler);
        startTimer();
    }

    void stop()
    {
        handler_ = nullptr;
        interval_ = {};
        timer_.cancel();
    }

    void pong(const MessageBuffer& payload)
    {
        if (frame_.count() == 0 || payload.size() != frameBytes_.size())
            return;

        auto cmp = std::memcmp(payload.data(), frameBytes_.data(),
                               frameBytes_.size());
        if (cmp == 0)
            matchingPongReceived_ = true;
    }

private:
    void startTimer()
    {
        std::weak_ptr<WebsocketPinger> self = shared_from_this();
        timer_.expires_after(interval_);
        timer_.async_wait(
            [self, this](boost::system::error_code ec)
            {
                static constexpr auto cancelled =
                    boost::asio::error::operation_aborted;
                auto me = self.lock();

                if (!me || ec == cancelled || !handler_)
                    return;

                if (ec)
                {
                    handler_(makeUnexpected(static_cast<std::error_code>(ec)));
                    return;
                }

                if ((frame_.count() > 0) && !matchingPongReceived_)
                {
                    handler_(makeUnexpectedError(
                        TransportErrc::heartbeatTimeout));
                    return;
                }

                matchingPongReceived_ = false;
                frame_.increment();
                frame_.serialize(frameBytes_);
                handler_(frameBytes_);
                startTimer();
            });
    }

    boost::asio::steady_timer timer_;
    Handler handler_;
    WebsocketPingFrame frame_;
    WebsocketPingBytes frameBytes_ = {};
    Timeout interval_ = {};
    bool matchingPongReceived_ = false;
};

//------------------------------------------------------------------------------
class WebsocketTransport : public Transporting
{
public:
    using Ptr            = std::shared_ptr<WebsocketTransport>;
    using TcpSocket      = boost::asio::ip::tcp::socket;
    using Socket         = boost::beast::websocket::stream<TcpSocket>;
    using SocketPtr      = std::unique_ptr<Socket>;
    using RxHandler      = typename Transporting::RxHandler;
    using TxErrorHandler = typename Transporting::TxErrorHandler;

    static Ptr create(SocketPtr&& s, TransportInfo info)
    {
        return Ptr(new WebsocketTransport(std::move(s), info));
    }

    bool isRunning() const override {return running_;}

    void start(RxHandler rxHandler, TxErrorHandler txErrorHandler) override
    {
        assert(!isRunning());
        rxHandler_ = rxHandler;
        txErrorHandler_ = txErrorHandler;
        receive();
        running_ = true;
    }

    void send(MessageBuffer message) override
    {
        // Due to the handler being posted, the caller may not be aware of a
        // network error having already occurred by time this is called.
        // So do nothing if the transport is already closed.
        if (!isRunning())
            return;

        auto buf = enframe(WebsocketMsgKind::wamp, std::move(message));
        sendFrame(std::move(buf));
    }

    void sendNowAndStop(MessageBuffer message) override
    {
        // Due to the handler being posted, the caller may not be aware of a
        // network error having already occurred by time this is called.
        // So do nothing if the transport is already closed.
        if (!isRunning())
            return;

        assert(socket_ && "Attempting to send on bad transport");
        auto frame = enframe(WebsocketMsgKind::wamp, std::move(message));
        assert((frame->payload().size() <= info().maxTxLength()) &&
               "Outgoing message is longer than allowed by peer");
        frame->poison();
        txQueue_.push_front(std::move(frame));
        transmit();
        running_ = false;
    }

    void stop() override
    {
        Base::clearConnectionInfo();
        rxHandler_ = nullptr;
        txErrorHandler_ = nullptr;
        txQueue_.clear();
        running_ = false;
        if (socket_)
            socket_->close(boost::beast::websocket::normal);
        if (pinger_)
            pinger_->stop();
    }

private:
    using Base = Transporting;
    using TransmitQueue = std::deque<WebsocketFrame::Ptr>;
    using TimePoint     = std::chrono::high_resolution_clock::time_point;

    static ConnectionInfo makeConnectionInfo(const Socket& socket)
    {
        // TODO
        return {};
    }

    WebsocketTransport(SocketPtr&& socket, TransportInfo info)
        : Base(info, makeConnectionInfo(*socket)),
          strand_(boost::asio::make_strand(socket->get_executor())),
          socket_(std::move(socket))
    {
        if (timeoutIsDefinite(Base::info().heartbeatInterval()))
            pinger_ = std::make_shared<WebsocketPinger>(strand_, Base::info());
    }

    WebsocketFrame::Ptr enframe(WebsocketMsgKind kind, MessageBuffer&& payload)
    {
        // TODO: Pool/reuse frames somehow
        return std::make_shared<WebsocketFrame>(kind, std::move(payload));
    }

    void sendFrame(WebsocketFrame::Ptr frame)
    {
        assert(socket_ && "Attempting to send on bad transport");
        assert((frame->payload().size() <= info().maxTxLength()) &&
               "Outgoing message is longer than allowed by peer");
        txQueue_.push_back(std::move(frame));
        transmit();
    }

    // NOLINTBEGIN(misc-no-recursion)
    void transmit()
    {
        if (isReadyToTransmit())
        {
            txFrame_ = txQueue_.front();
            txQueue_.pop_front();

            auto self = this->shared_from_this();
            socket_->async_write(
                txFrame_->payloadBuffer(),
                [this, self](boost::system::error_code asioEc, size_t)
                {
                    const bool frameWasPoisoned = txFrame_ &&
                                                  txFrame_->isPoisoned();
                    txFrame_.reset();
                    if (asioEc)
                    {
                        if (txErrorHandler_)
                        {
                            auto ec = static_cast<std::error_code>(asioEc);
                            txErrorHandler_(ec);
                        }
                        cleanup();
                    }
                    else if (frameWasPoisoned)
                    {
                        stop();
                    }
                    else
                    {
                        transmit();
                    }
                });
        }
    }
    // NOLINTEND(misc-no-recursion)

    bool isReadyToTransmit() const
    {
        return socket_ &&          // Socket is still open
               !txFrame_ &&        // No async_write is in progress
               !txQueue_.empty();  // One or more messages are enqueued
    }

    // NOLINTBEGIN(misc-no-recursion)
    void receive()
    {
        if (socket_)
        {
            rxFrame_.clear();
            auto self = this->shared_from_this();
            socket_->async_read(
                rxFrame_,
                [this, self](boost::system::error_code ec, size_t)
                {
                    if (ec == boost::asio::error::connection_reset ||
                        ec == boost::asio::error::eof)
                    {
                        onRemoteDisconnect();
                    }
                    else if (check(ec))
                    {
                        processPayload();
                    }
                });
        }
    }
    // NOLINTEND(misc-no-recursion)

    void onRemoteDisconnect()
    {
        if (rxHandler_)
            post(rxHandler_, makeUnexpectedError(TransportErrc::disconnected));
        cleanup();
    }

    // NOLINTBEGIN(misc-no-recursion)
    void processPayload()
    {
        using Byte = MessageBuffer::value_type;
        const auto* ptr = reinterpret_cast<const Byte*>(rxFrame_.cdata().data());
        MessageBuffer buffer{ptr, ptr + rxFrame_.cdata().size()};
        post(rxHandler_, std::move(buffer));
    }
    // NOLINTEND(misc-no-recursion)

    template <typename F, typename... Ts>
    void post(F&& handler, Ts&&... args)
    {
        // NOLINTNEXTLINE(modernize-avoid-bind)
        boost::asio::post(strand_, std::bind(std::forward<F>(handler),
                                             std::forward<Ts>(args)...));
    }

    bool check(boost::system::error_code asioEc)
    {
        if (asioEc)
        {
            if (rxHandler_)
            {
                auto ec = static_cast<std::error_code>(asioEc);
                post(rxHandler_, UnexpectedError(ec));
            }
            cleanup();
        }
        return !asioEc;
    }

    template <typename TErrc>
    bool check(bool condition, TErrc errc)
    {
        if (!condition)
        {
            if (rxHandler_)
                post(rxHandler_, makeUnexpectedError(errc));
            cleanup();
        }
        return condition;
    }

    void fail(std::error_code ec)
    {
        if (rxHandler_)
            post(rxHandler_, makeUnexpected(ec));
        cleanup();
    }

    void cleanup()
    {
        Base::clearConnectionInfo();
        rxHandler_ = nullptr;
        txErrorHandler_ = nullptr;
        rxFrame_.clear();
        txQueue_.clear();
        txFrame_ = nullptr;
        pingFrame_ = nullptr;
        socket_.reset();
        pinger_.reset();
        running_ = false;
    }

    IoStrand strand_;
    RxHandler rxHandler_;
    TxErrorHandler txErrorHandler_;
    boost::beast::flat_buffer rxFrame_;
    TransmitQueue txQueue_;
    MessageBuffer pingBuffer_;
    WebsocketFrame::Ptr txFrame_;
    WebsocketFrame::Ptr pingFrame_;
    std::unique_ptr<Socket> socket_;
    std::shared_ptr<WebsocketPinger> pinger_;
    bool running_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WEBSOCKETTRANSPORT_HPP
