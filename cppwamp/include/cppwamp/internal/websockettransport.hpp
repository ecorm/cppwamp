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
#include <boost/asio/read.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream.hpp>
#include "../asiodefs.hpp"
#include "../codec.hpp"
#include "../errorcodes.hpp"
#include "../messagebuffer.hpp"
#include "../transport.hpp"
#include "../version.hpp"
#include "../transports/websocketendpoint.hpp"
#include "../transports/websocketprotocol.hpp"
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
    using Ptr             = std::shared_ptr<WebsocketTransport>;
    using TcpSocket       = boost::asio::ip::tcp::socket;
    using WebsocketSocket = boost::beast::websocket::stream<TcpSocket>;
    using WebsocketPtr    = std::unique_ptr<WebsocketSocket>;
    using RxHandler       = typename Transporting::RxHandler;
    using TxErrorHandler  = typename Transporting::TxErrorHandler;

protected:
    static std::error_code netErrorCodeToStandard(
        boost::system::error_code netEc)
    {
        bool disconnected =
            netEc == boost::asio::error::broken_pipe ||
            netEc == boost::asio::error::connection_reset ||
            netEc == boost::asio::error::eof;
        auto ec = disconnected
                      ? make_error_code(TransportErrc::disconnected)
                      : static_cast<std::error_code>(netEc);
        if (netEc == boost::asio::error::operation_aborted)
            ec = make_error_code(TransportErrc::aborted);
        return ec;
    }

    // Constructor for client transports
    WebsocketTransport(WebsocketPtr&& ws, TransportInfo info)
        : Base(makeConnectionInfo(ws->next_layer()), info),
          strand_(boost::asio::make_strand(ws->get_executor())),
          websocket_(std::move(ws))
    {
        if (timeoutIsDefinite(Base::info().heartbeatInterval()))
            pinger_ = std::make_shared<Pinger>(strand_, Base::info());
    }

    // Constructor for server transports
    WebsocketTransport(TcpSocket& tcp)
        : Base(makeConnectionInfo(tcp)),
          strand_(boost::asio::make_strand(tcp.get_executor()))
    {}

    void assignWebsocket(WebsocketPtr&& ws, TransportInfo i)
    {
        websocket_ = std::move(ws);
        Base::completeAccept(i);
    }

    template <typename F, typename... Ts>
    void post(F&& handler, Ts&&... args) //
    {
        // NOLINTNEXTLINE(modernize-avoid-bind)
        boost::asio::post(strand_, std::bind(std::forward<F>(handler),
                                             std::forward<Ts>(args)...));
    }

private:
    using Base          = Transporting;
    using TransmitQueue = std::deque<WebsocketFrame::Ptr>;
    using Byte          = MessageBuffer::value_type;

    static ConnectionInfo makeConnectionInfo(const TcpSocket& socket)
    {
        static constexpr unsigned ipv4VersionNo = 4;
        static constexpr unsigned ipv6VersionNo = 6;

        const auto& ep = socket.remote_endpoint();
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
    websocketErrorToCloseCode(boost::beast::error_code ec)
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

            websocket_->control_callback(
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
        if (!websocket_)
            return;
        auto buf = enframe(WebsocketMsgKind::wamp, std::move(message));
        sendFrame(std::move(buf));
    }

    void onSendNowAndStop(MessageBuffer message) override
    {
        if (!websocket_)
            return;
        auto frame = enframe(WebsocketMsgKind::wamp, std::move(message));
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
        if (websocket_)
        {
            websocket_->control_callback();
            closeWebsocket(boost::beast::websocket::normal);
        }
        if (pinger_)
            pinger_->stop();
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
        if (state() != TransportState::running)
            return;

        if (!pingBytes.has_value())
            return fail(pingBytes.error(), boost::beast::websocket::going_away);

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

    void sendPing()
    {
        using PingData = boost::beast::websocket::ping_data;

        const auto size = txFrame_->payload().size();
        assert(size <= PingData::static_capacity);
        const auto* data = txFrame_->payload().data();
        const auto* ptr = reinterpret_cast<const PingData::value_type*>(data);
        PingData buffer{ptr, ptr + size};
        auto self = this->shared_from_this();

        websocket_->async_ping(
            buffer,
            [this, self](boost::beast::error_code netEc)
            {
                txFrame_.reset();
                if (!checkTxError(netEc))
                    transmit();
            });
    }

    void sendWampMessage()
    {
        auto self = this->shared_from_this();
        websocket_->async_write(
            txFrame_->payloadBuffer(),
            [this, self](boost::beast::error_code netEc, size_t)
            {
                auto frame = std::move(txFrame_);
                txFrame_.reset();
                if (!checkTxError(netEc))
                    return;
                if (frame && frame->isPoisoned())
                    onStop();
                else
                    transmit();
            });
    }

    bool isReadyToTransmit() const
    {
        return websocket_ &&      // Socket is still open
               !txFrame_ &&       // No async_write is in progress
               !txQueue_.empty(); // One or more messages are enqueued
    }

    void receive()
    {
        if (!websocket_)
            return;

        rxFrame_.clear();
        auto self = this->shared_from_this();
        websocket_->async_read(
            rxFrame_,
            [this, self](boost::beast::error_code netEc, size_t)
            {
                if (netEc == boost::beast::websocket::error::closed)
                    onRemoteClose();
                else if (check(netEc))
                    processPayload();
            });
    }

    void onRemoteClose()
    {
        if (rxHandler_)
        {
            std::error_code ec = make_error_code(TransportErrc::disconnected);
            auto reasonCode = websocket_->reason().code;
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
        if (websocket_->text() && websocket_->got_binary())
        {
            return fail(TransportErrc::expectedText,
                        boost::beast::websocket::unknown_data);
        }

        if (websocket_->binary() && websocket_->got_text())
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

    void closeWebsocket(boost::beast::websocket::close_code reason)
    {
        if (!websocket_)
            return;
        auto self = shared_from_this();
        // TODO: Timeout
        websocket_->async_close(
            reason,
            [this, self](boost::beast::error_code)
            {
                // TODO: Report close failure once asynchronous
                // Session::disconnect is implemented.
                websocket_->next_layer().close();
            });
    }

    bool checkTxError(boost::system::error_code netEc)
    {
        if (!netEc)
            return true;
        if (txErrorHandler_)
            post(txErrorHandler_, netErrorCodeToStandard(netEc));
        cleanup();
        return false;
    }

    bool check(boost::system::error_code netEc)
    {
        if (!netEc)
            return true;
        auto ec = netErrorCodeToStandard(netEc);
        using BWE = boost::beast::websocket::error;
        if (netEc == BWE::message_too_big || netEc == BWE::buffer_overflow)
            ec = make_error_code(TransportErrc::inboundTooLong);
        fail(ec, websocketErrorToCloseCode(netEc));
        return false;
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
        closeWebsocket(closeCode);
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
    WebsocketPtr websocket_;
    RxHandler rxHandler_;
    TxErrorHandler txErrorHandler_;
    boost::beast::flat_buffer rxFrame_;
    TransmitQueue txQueue_;
    MessageBuffer pingBuffer_;
    WebsocketFrame::Ptr txFrame_;
    WebsocketFrame::Ptr pingFrame_;
    std::shared_ptr<Pinger> pinger_;
};

//------------------------------------------------------------------------------
class WebsocketClientTransport : public WebsocketTransport
{
public:
    using Ptr = std::shared_ptr<WebsocketClientTransport>;

    static Ptr create(WebsocketPtr&& s, TransportInfo i)
    {
        return Ptr(new WebsocketClientTransport(std::move(s), i));
    }

private:
    using Base = WebsocketTransport;

    WebsocketClientTransport(WebsocketPtr&& s, TransportInfo info)
        : Base(std::move(s), info)
    {}
};

//------------------------------------------------------------------------------
class WebsocketServerTransport : public WebsocketTransport
{
public:
    using Ptr = std::shared_ptr<WebsocketServerTransport>;
    using Settings = WebsocketEndpoint;

    static Ptr create(TcpSocket&& t, const Settings& s, const CodecIdSet& c)
    {
        return Ptr(new WebsocketServerTransport(std::move(t), s, c));
    }

private:
    using Base = WebsocketTransport;

    // This data is only used once for accepting connections.
    struct Data
    {
        Data(TcpSocket&& t, const Settings& s, const CodecIdSet& c)
            : tcpSocket(std::move(t)),
              codecIds(c),
              settings(s)
        {}

        TcpSocket tcpSocket;
        CodecIdSet codecIds;
        WebsocketEndpoint settings;
        AcceptHandler handler;
        boost::beast::flat_buffer buffer;
        boost::beast::http::request<boost::beast::http::string_body> request;
        boost::beast::http::response<boost::beast::http::string_body> response;
        std::unique_ptr<WebsocketSocket> websocket; // TODO: Use optional<T>
        int codecId;
    };

    static bool subprotocolIsText(int codecId)
    {
        return codecId == KnownCodecIds::json();
    }

    static int parseSubprotocol(boost::beast::string_view field)
    {
        if (field == "wamp.2.json")
            return KnownCodecIds::json();
        else if (field == "wamp.2.msgpack")
            return KnownCodecIds::msgpack();
        else if (field == "wamp.2.cbor")
            return KnownCodecIds::cbor();
        return 0;
    }

    template <typename T>
    static void setHandshakeField(WebsocketSocket& websocket,
                                  boost::beast::http::field field, T&& value)
    {
        namespace http = boost::beast::http;

        struct Decorator
        {
            ValueTypeOf<T> value;
            http::field field;

            void operator()(http::response_header<>& header)
            {
                header.set(field, std::move(value));
            }
        };

        websocket.set_option(boost::beast::websocket::stream_base::decorator(
            Decorator{std::forward<T>(value), field}));
    }

    WebsocketServerTransport(TcpSocket&& t, const Settings& s,
                             const CodecIdSet& c)
        : Base(t),
          data_(new Data(std::move(t), s, c))
    {}

    void onAccept(AcceptHandler handler) override
    {
        // TODO: Timeout waiting for handshake
        assert((data_ != nullptr) && "Accept already performed");
        data_->handler = std::move(handler);
        auto self = this->shared_from_this();
        boost::beast::http::async_read(
            data_->tcpSocket, data_->buffer, data_->request,
            [this, self] (const boost::beast::error_code& netEc, std::size_t)
            {
                if (check(netEc))
                    acceptHandshake();
            });
    }

    void onCancelAccept() override
    {
        if (!data_)
            return;
        if (data_->websocket)
            data_->websocket->next_layer().close();
        else
            data_->tcpSocket.close();
    }

    void acceptHandshake()
    {
        // TODO: Multiplex websocket transports with same port but different
        //       request-target URIs.

        // Check that we actually received a websocket upgrade request
        if (!boost::beast::websocket::is_upgrade(data_->request))
            return fail(boost::beast::websocket::error::no_connection_upgrade);

        // Parse the subprotocol to determine the peer's desired codec
        using boost::beast::http::field;
        const auto& upgrade = data_->request;
        auto found = upgrade.base().find(field::sec_websocket_protocol);
        if (found == upgrade.base().end())
        {
            return respondThenFail("No subprotocol was requested",
                                   TransportErrc::noSerializer);
        }
        auto subprotocol = found->value();
        data_->codecId = parseSubprotocol(subprotocol);
        if (data_->codecIds.count(data_->codecId) == 0)
        {
            return respondThenFail("The requested subprotocol is not supported",
                                   TransportErrc::badSerializer);
        }

        // Transfer the TCP socket to a new websocket stream
        data_->websocket.reset(
            new WebsocketSocket{std::move(data_->tcpSocket)});
        auto& ws = *(data_->websocket);

        // Set the Server field of the handshake
        using boost::beast::http::field;
        setHandshakeField(ws, field::server, Version::agentString());

        // Set the Sec-WebsocketSocket-Protocol field of the handshake
        setHandshakeField(ws, field::sec_websocket_protocol, subprotocol);

        // Complete the handshake
        auto self = shared_from_this();
        ws.async_accept(
            data_->request,
            [this, self](boost::beast::error_code netEc)
            {
                if (check(netEc))
                    complete();
            });
    }

    void respondThenFail(std::string msg, TransportErrc errc)
    {
        namespace http = boost::beast::http;
        data_->response.result(http::status::bad_request);
        data_->response.body() = std::move(msg);
        auto self = shared_from_this();
        http::async_write(
            data_->tcpSocket, data_->response,
            [this, self, errc](boost::beast::error_code netEc, std::size_t)
            {
                if (check(netEc))
                    fail(errc);
            });
    }

    void complete()
    {
        if (subprotocolIsText(data_->codecId))
            data_->websocket->text(true);
        else
            data_->websocket->binary(true);

        data_->websocket->read_message_max(data_->settings.maxRxLength());

        const TransportInfo i{data_->codecId,
                              std::numeric_limits<std::size_t>::max(),
                              data_->settings.maxRxLength()};

        Base::assignWebsocket(std::move(data_->websocket), i);
        Base::post(data_->handler, data_->codecId);
        data_.reset();
    }

    bool check(boost::system::error_code netEc)
    {
        if (netEc)
            fail(Base::netErrorCodeToStandard(netEc));
        return !netEc;
    }

    void fail(std::error_code ec)
    {
        Base::post(data_->handler, makeUnexpected(ec));
        if (data_->websocket)
            data_->websocket->next_layer().close();
        else
            data_->tcpSocket.close();
        data_.reset();
        Base::shutdown();
    }

    template <typename TErrc>
    void fail(TErrc errc)
    {
        fail(static_cast<std::error_code>(make_error_code(errc)));
    }

    std::unique_ptr<Data> data_; // Only used once for accepting connection.
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WEBSOCKETTRANSPORT_HPP
