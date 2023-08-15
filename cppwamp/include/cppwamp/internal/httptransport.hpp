/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_HTTPTRANSPORT_HPP
#define CPPWAMP_INTERNAL_HTTPTRANSPORT_HPP

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
#include "../transports/httpprotocol.hpp"
#include "../transports/websocketprotocol.hpp"
#include "pinger.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
enum class HttpMsgKind
{
    wamp,
    ping
};

//------------------------------------------------------------------------------
class HttpFrame
{
public:
    using Ptr = std::shared_ptr<HttpFrame>;

    HttpFrame() = default;

    HttpFrame(HttpMsgKind kind, MessageBuffer&& payload)
        : payload_(std::move(payload)),
          kind_(kind)
    {}

    void clear()
    {
        payload_.clear();
        kind_ = {};
        isPoisoned_ = false;
    }

    HttpMsgKind kind() const {return kind_;}

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
    HttpMsgKind kind_;
    bool isPoisoned_ = false;
};

//------------------------------------------------------------------------------
class HttpTransport : public Transporting
{
public:
    using Ptr            = std::shared_ptr<HttpTransport>;
    using TcpSocket      = boost::asio::ip::tcp::socket;
    using Socket         = boost::beast::websocket::stream<TcpSocket>;
    using SocketPtr      = std::unique_ptr<Socket>;
    using RxHandler      = typename Transporting::RxHandler;
    using TxErrorHandler = typename Transporting::TxErrorHandler;

    static Ptr create(SocketPtr&& s, TransportInfo info)
    {
        return Ptr(new HttpTransport(std::move(s), info));
    }

private:
    using Base          = Transporting;
    using TransmitQueue = std::deque<HttpFrame::Ptr>;
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

    static boost::beast::websocket::close_code
    httpErrorToCloseCode(boost::beast::error_code ec)
    {
        using boost::beast::websocket::close_code;
        using boost::beast::websocket::condition;
        using boost::beast::websocket::error;

        if (ec == condition::protocol_violation)
            return close_code::protocol_error;
        if (ec == error::buffer_overflow || ec == error::message_too_big)
            return close_code::too_big;
        return close_code::internal_error;
    }

    void onStart(RxHandler rxHandler, TxErrorHandler txErrorHandler) override
    {
        rxHandler_ = rxHandler;
        txErrorHandler_ = txErrorHandler;

        if (pinger_)
        {
            std::weak_ptr<Transporting> self = shared_from_this();

            http_->control_callback(
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
    }

    void onSend(MessageBuffer message) override
    {
        if (!http_)
            return;
        auto buf = enframe(HttpMsgKind::wamp, std::move(message));
        sendFrame(std::move(buf));
    }

    void onSendNowAndStop(MessageBuffer message) override
    {
        if (!http_)
            return;
        auto frame = enframe(HttpMsgKind::wamp, std::move(message));
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
        if (http_)
        {
            http_->control_callback();
            closeHttp(boost::beast::websocket::normal);
        }
        if (pinger_)
            pinger_->stop();
    }

    HttpTransport(SocketPtr&& socket, TransportInfo info)
        : Base(info, makeConnectionInfo(*socket)),
          strand_(boost::asio::make_strand(socket->get_executor())),
          http_(std::move(socket))
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
        if (state() != Transporting::State::running)
            return;

        if (!pingBytes.has_value())
            return fail(pingBytes.error(), boost::beast::websocket::going_away);

        MessageBuffer message{pingBytes->begin(), pingBytes->end()};
        auto buf = enframe(HttpMsgKind::ping, std::move(message));
        sendFrame(std::move(buf));
    }

    HttpFrame::Ptr enframe(HttpMsgKind kind, MessageBuffer&& payload)
    {
        // TODO: Pool/reuse frames somehow
        return std::make_shared<HttpFrame>(kind, std::move(payload));
    }

    void sendFrame(HttpFrame::Ptr frame)
    {
        assert(http_ && "Attempting to send on bad transport");
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
            if (kind == HttpMsgKind::ping)
            {
                sendPing();
            }
            else
            {
                assert(kind == HttpMsgKind::wamp);
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

        http_->async_ping(
            buffer,
            [this, self](boost::beast::error_code bec)
            {
                txFrame_.reset();
                if (bec)
                {
                    if (txErrorHandler_)
                        txErrorHandler_(static_cast<std::error_code>(bec));
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
        http_->async_write(
            txFrame_->payloadBuffer(),
            [this, self](boost::beast::error_code bec, size_t)
            {
                const bool frameWasPoisoned = txFrame_ &&
                                              txFrame_->isPoisoned();
                txFrame_.reset();
                if (bec)
                {
                    if (txErrorHandler_)
                    {
                        auto ec = static_cast<std::error_code>(bec);
                        txErrorHandler_(ec);
                    }
                    cleanup();
                }
                else if (frameWasPoisoned)
                {
                    onStop();
                }
                else
                {
                    transmit();
                }
            });
    }

    bool isReadyToTransmit() const
    {
        return http_ &&       // Socket is still open
               !txFrame_ &&        // No async_write is in progress
               !txQueue_.empty();  // One or more messages are enqueued
    }

    void receive()
    {
        if (http_)
        {
            rxFrame_.clear();
            auto self = this->shared_from_this();
            http_->async_read(
                rxFrame_,
                [this, self](boost::beast::error_code ec, size_t)
                {
                    namespace bae = boost::asio::error;
                    if (ec == bae::connection_reset || ec == bae::eof)
                        onRemoteDisconnect();
                    else if (ec == boost::beast::websocket::error::closed)
                        onRemoteClose();
                    else if (check(ec))
                        processPayload();
                });
        }
    }

    void onRemoteDisconnect()
    {
        if (rxHandler_)
            post(rxHandler_, makeUnexpectedError(TransportErrc::disconnected));
        cleanup();
    }

    void onRemoteClose()
    {
        if (rxHandler_)
        {
            std::error_code ec = make_error_code(TransportErrc::disconnected);
            auto reasonCode = http_->reason().code;
            if (reasonCode != boost::beast::websocket::close_code::normal)
            {
                auto value = static_cast<int>(reasonCode);
                auto msg = websocketCloseCategory().message(value);
                if (!msg.empty())
                    ec = std::error_code{value, websocketCloseCategory()};
                if (ec == WebsocketCloseErrc::tooBig)
                    ec = make_error_code(TransportErrc::outboundTooLong);
            }
            post(rxHandler_, makeUnexpected(ec));
        }
        cleanup();
    }

    void processPayload()
    {
        if (http_->text() && http_->got_binary())
        {
            return fail(TransportErrc::expectedText,
                        boost::beast::websocket::unknown_data);
        }

        if (http_->binary() && http_->got_text())
        {
            return fail(TransportErrc::expectedBinary,
                        boost::beast::websocket::unknown_data);
        }

        using Byte = MessageBuffer::value_type;
        const auto* ptr =
            reinterpret_cast<const Byte*>(rxFrame_.cdata().data());
        MessageBuffer buffer{ptr, ptr + rxFrame_.cdata().size()};
        post(rxHandler_, std::move(buffer));
        receive();
    }

    void closeHttp(boost::beast::websocket::close_code reason)
    {
        if (http_ == nullptr)
            return;
        auto self = shared_from_this();
        http_->async_close(
            reason,
            [this, self](boost::beast::error_code) {http_.reset();});
    }

    template <typename F, typename... Ts>
    void post(F&& handler, Ts&&... args)
    {
        // NOLINTNEXTLINE(modernize-avoid-bind)
        boost::asio::post(strand_, std::bind(std::forward<F>(handler),
                                             std::forward<Ts>(args)...));
    }

    bool check(boost::beast::error_code bec)
    {
        if (bec)
        {
            if (rxHandler_)
            {
                using BWE = boost::beast::websocket::error;
                auto ec = static_cast<std::error_code>(bec);
                if (bec == BWE::message_too_big || bec == BWE::buffer_overflow)
                    ec = make_error_code(TransportErrc::inboundTooLong);
                post(rxHandler_, UnexpectedError(ec));
            }
            closeHttp(httpErrorToCloseCode(bec));
            cleanup();
        }
        return !bec;
    }

    template <typename TErrc>
    void fail(TErrc errc, boost::beast::websocket::close_code closeCode)
    {
        fail(make_error_code(errc), closeCode);
    }

    void fail(std::error_code ec, boost::beast::websocket::close_code closeCode)
    {
        if (rxHandler_)
            post(rxHandler_, makeUnexpected(ec));
        closeHttp(closeCode);
        cleanup();
    }

    void cleanup()
    {
        Base::shutdown();
        rxHandler_ = nullptr;
        txErrorHandler_ = nullptr;
        rxFrame_.clear();
        txQueue_.clear();
        txFrame_ = nullptr;
        pingFrame_ = nullptr;
        pinger_.reset();
    }

    IoStrand strand_;
    RxHandler rxHandler_;
    TxErrorHandler txErrorHandler_;
    boost::beast::flat_buffer rxFrame_;
    TransmitQueue txQueue_;
    MessageBuffer pingBuffer_;
    HttpFrame::Ptr txFrame_;
    HttpFrame::Ptr pingFrame_;
    std::unique_ptr<Socket> http_;
    std::shared_ptr<Pinger> pinger_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_HTTPTRANSPORT_HPP