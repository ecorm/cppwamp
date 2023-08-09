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
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream.hpp>
#include "../asiodefs.hpp"
#include "../errorcodes.hpp"
#include "../messagebuffer.hpp"
#include "../transport.hpp"
#include "pinger.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
enum class WebsocketMsgKind
{
    wamp,
    ping
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

        if (pinger_)
        {
            std::weak_ptr<Transporting> self = shared_from_this();

            socket_->control_callback(
                [self, this](boost::beast::websocket::frame_type type,
                             boost::beast::string_view msg)
                {
                    auto me = self.lock();
                    if (me && type == boost::beast::websocket::frame_type::pong)
                        onPong(msg);
                });

            pinger_->start(
                [self, this](ErrorOr<PingBytes> pingBytes)
                {
                    auto me = self.lock();
                    if (me)
                        onPingFrame(pingBytes);
                });
        }

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
        {
            socket_->control_callback();
            socket_->close(boost::beast::websocket::normal);
        }
        if (pinger_)
            pinger_->stop();
    }

private:
    using Base          = Transporting;
    using TransmitQueue = std::deque<WebsocketFrame::Ptr>;
    using Byte          = MessageBuffer::value_type;

    static ConnectionInfo makeConnectionInfo(const Socket& socket)
    {
        static constexpr unsigned ipv4VersionNo = 4;
        static constexpr unsigned ipv6VersionNo = 6;

        const auto& ep = socket.next_layer().remote_endpoint();
        std::ostringstream oss;
        oss << ep;
        const auto addr = ep.address();
        const bool isIpv6 = addr.is_v6();

        Object details
        {
            {"address", addr.to_string()},
            {"ip_version", isIpv6 ? ipv6VersionNo : ipv4VersionNo},
            {"endpoint", oss.str()},
            {"port", ep.port()},
            {"protocol", "WS"},
        };

        if (!isIpv6)
        {
            details.emplace("numeric_address", addr.to_v4().to_uint());
        }

        return {std::move(details), oss.str()};
    }

    WebsocketTransport(SocketPtr&& socket, TransportInfo info)
        : Base(info, makeConnectionInfo(*socket)),
          strand_(boost::asio::make_strand(socket->get_executor())),
          socket_(std::move(socket))
    {
        if (timeoutIsDefinite(Base::info().heartbeatInterval()))
            pinger_ = std::make_shared<Pinger>(strand_, Base::info());
    }

    void onPong(boost::beast::string_view msg)
    {
        if (pinger_)
        {
            const auto* bytes = reinterpret_cast<const Byte*>(msg.data());
            pinger_->pong(bytes, msg.size());
        }
    }

    void onPingFrame(ErrorOr<PingBytes> pingBytes)
    {
        if (!isRunning())
            return;

        if (!pingBytes.has_value())
        {
            return fail(pingBytes.error());
        }

        MessageBuffer message{pingBytes->begin(), pingBytes->end()};
        auto buf = enframe(WebsocketMsgKind::ping, std::move(message));
        sendFrame(std::move(buf));
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

    void transmit()
    {
        if (isReadyToTransmit())
        {
            txFrame_ = txQueue_.front();
            txQueue_.pop_front();
            auto kind = txFrame_->kind();
            if (kind == WebsocketMsgKind::ping)
            {
                sendPing();
            }
            else
            {
                assert(kind == WebsocketMsgKind::wamp);
                sendWampMessage();
            }
        }
    }

    void sendPing()
    {
        using PingData = boost::beast::websocket::ping_data;

        const auto size = txFrame_->payload().size();
        assert(size <= PingData::static_capacity);
        const auto* data = txFrame_->payload().data();
        const auto* ptr = reinterpret_cast<const PingData::value_type*>(data);
        PingData buffer{ptr, ptr + size};
        auto self = this->shared_from_this();

        socket_->async_ping(
            buffer,
            [this, self](boost::system::error_code asioEc)
            {
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
                else
                {
                    transmit();
                }
            });
    }

    void sendWampMessage()
    {
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

    bool isReadyToTransmit() const
    {
        return socket_ &&          // Socket is still open
               !txFrame_ &&        // No async_write is in progress
               !txQueue_.empty();  // One or more messages are enqueued
    }

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

    void onRemoteDisconnect()
    {
        if (rxHandler_)
            post(rxHandler_, makeUnexpectedError(TransportErrc::disconnected));
        cleanup();
    }

    void processPayload()
    {
        using Byte = MessageBuffer::value_type;
        const auto* ptr = reinterpret_cast<const Byte*>(rxFrame_.cdata().data());
        MessageBuffer buffer{ptr, ptr + rxFrame_.cdata().size()};
        post(rxHandler_, std::move(buffer));
        receive();
    }

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
    std::shared_ptr<Pinger> pinger_;
    bool running_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WEBSOCKETTRANSPORT_HPP
