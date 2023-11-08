/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_WEBSOCKETTRANSPORT_HPP
#define CPPWAMP_INTERNAL_WEBSOCKETTRANSPORT_HPP

#include <cstddef>
#include <functional>
#include <memory>
#include <limits>
#include <utility>
#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/websocket/option.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/optional/optional.hpp>
#include "../anyhandler.hpp"
#include "../basictransport.hpp"
#include "../codec.hpp"
#include "../errorcodes.hpp"
#include "../traits.hpp"
#include "../transports/websocketprotocol.hpp"
#include "../wampdefs.hpp"
#include "tcptraits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
inline std::error_code websocketErrorCodeToStandard(
    boost::system::error_code netEc)
{
    if (!netEc)
        return {};

    namespace AE = boost::asio::error;
    using WE = boost::beast::websocket::error;
    bool disconnected = netEc == AE::broken_pipe ||
                        netEc == AE::connection_reset ||
                        netEc == AE::eof;
    if (disconnected)
        return make_error_code(TransportErrc::disconnected);
    if (netEc == boost::beast::websocket::error::closed)
        return make_error_code(TransportErrc::ended);
    if (netEc == AE::operation_aborted)
        return make_error_code(TransportErrc::aborted);
    if (netEc == WE::buffer_overflow || netEc == WE::message_too_big)
        return make_error_code(TransportErrc::inboundTooLong);

    return static_cast<std::error_code>(netEc);
}

//------------------------------------------------------------------------------
class WebsocketStream
{
public:
    using TcpSocket = boost::asio::ip::tcp::socket;
    using Socket = boost::beast::websocket::stream<TcpSocket>;

    static ConnectionInfo makeConnectionInfo(const Socket& s)
    {
        return makeConnectionInfo(s.next_layer());
    }

    static ConnectionInfo makeConnectionInfo(const TcpSocket& s)
    {
        return TcpTraits::connectionInfo(s, "WS");
    }

    WebsocketStream(AnyIoExecutor e) : websocket_(std::move(e)) {}

    template <typename S>
    explicit WebsocketStream(Socket&& ws, const std::shared_ptr<S>& settings)
        : websocket_(std::move(ws))
    {
        auto n = settings->limits().rxMsgSize();
        if (n != 0)
            websocket_->read_message_max(n);
    }

    WebsocketStream& operator=(WebsocketStream&& rhs)
    {
        websocket_.emplace(std::move(rhs.websocket_).value());
        rhs.websocket_.reset();
        return *this;
    }

    AnyIoExecutor executor() {return websocket_->get_executor();}

    bool isOpen() const
    {
        return websocket_->next_layer().is_open() &&
               websocket_->is_open();
    }

    template <typename F>
    void observeControlFrames(F&& callback)
    {
        struct Handler
        {
            Decay<F> callback;

            void operator()(boost::beast::websocket::frame_type type,
                            boost::beast::string_view msg)
            {
                if (type == boost::beast::websocket::frame_type::pong)
                {
                    using Byte = MessageBuffer::value_type;
                    const auto* ptr = reinterpret_cast<const Byte*>(msg.data());
                    callback(TransportFrameKind::pong, ptr, msg.size());
                }
            }
        };

        websocket_->control_callback(Handler{std::forward<F>(callback)});
    }

    void unobserveControlFrames()
    {
        websocket_->control_callback();
    }

    template <typename F>
    void ping(const uint8_t* data, std::size_t size, F&& callback)
    {
        using PingData = boost::beast::websocket::ping_data;
        using CharType = PingData::value_type;

        struct Pinged
        {
            Decay<F> callback;

            void operator()(boost::beast::error_code netEc)
            {
                callback(websocketErrorCodeToStandard(netEc));
            }
        };

        assert(size <= PingData::static_capacity);
        const auto* ptr = reinterpret_cast<const CharType*>(data);

        // Beast copies the payload
        websocket_->async_ping(PingData{ptr, ptr + size},
                               Pinged{std::forward<F>(callback)});
    }

    template <typename F>
    void pong(const uint8_t*, std::size_t, F&&)
    {
        // Do nothing; Beast automatically responds to pings.
    }

    template <typename F>
    void writeSome(const uint8_t* data, std::size_t size, F&& callback)
    {
        struct Written
        {
            Decay<F> callback;

            void operator()(boost::beast::error_code netEc, size_t n)
            {
                callback(websocketErrorCodeToStandard(netEc), n);
            }
        };

        websocket_->async_write_some(true, boost::asio::buffer(data, size),
                                     Written{std::forward<F>(callback)});
    }

    template <typename F>
    void readSome(MessageBuffer& buffer, F&& callback)
    {
        struct Received
        {
            Decay<F> callback;
            WebsocketStream* self;

            void operator()(boost::beast::error_code netEc, std::size_t n)
            {
                self->onRead(netEc, n, callback);
            }
        };

        rxBuffer_.emplace(buffer);
        websocket_->async_read_some(
            *rxBuffer_,
            0, // Beast will choose 1536
            Received{std::forward<F>(callback), this});
    }

    template <typename F>
    void shutdown(std::error_code reason, F&& callback)
    {
        if (!websocket_->is_open())
        {
            boost::system::error_code netEc;
            websocket_->next_layer().shutdown(
                boost::asio::ip::tcp::socket::shutdown_send, netEc);
            postAny(websocket_->get_executor(),
                    std::forward<F>(callback),
                    static_cast<std::error_code>(netEc));
            return;
        }

        using boost::beast::websocket::close_code;
        using boost::beast::websocket::condition;
        using boost::beast::websocket::error;

        if (!reason)
            return closeWebsocket(close_code::normal, std::forward<F>(callback));

        auto closeCode = close_code::internal_error;

        if (reason == TransportErrc::ended)
        {
            closeCode = close_code::going_away;
        }
        else if (reason == TransportErrc::inboundTooLong)
        {
            closeCode = close_code::too_big;
        }
        else if (reason == TransportErrc::expectedBinary ||
                 reason == TransportErrc::expectedText)
        {
            closeCode = close_code::bad_payload;
        }
        else
        {
            auto netEc = static_cast<boost::system::error_code>(reason);
            if (netEc == condition::protocol_violation)
                closeCode = close_code::protocol_error;
        }

        closeWebsocket(closeCode, std::forward<F>(callback));
    }

    std::error_code shutdown()
    {
        boost::system::error_code netEc;
        websocket_->next_layer().shutdown(
            boost::asio::ip::tcp::socket::shutdown_send, netEc);
        return static_cast<std::error_code>(netEc);
    }

    void close()
    {
        websocket_->next_layer().close();
    }

private:
    using DynamicBufferAdapter =
        boost::asio::dynamic_vector_buffer<MessageBuffer::value_type,
                                           MessageBuffer::allocator_type>;

    static std::error_code interpretCloseReason(
        const boost::beast::websocket::close_reason& reason)
    {
        std::error_code ec = make_error_code(TransportErrc::ended);
        auto code = reason.code;
        if (code != boost::beast::websocket::close_code::normal)
        {
            auto value = static_cast<int>(code);
            auto msg = websocketCloseCategory().message(value);
            if (!msg.empty())
                ec = std::error_code{value, websocketCloseCategory()};
            if (ec == WebsocketCloseErrc::tooBig)
                ec = make_error_code(TransportErrc::outboundTooLong);
        }
        return ec;
    }

    template <typename F>
    void onRead(boost::beast::error_code netEc, std::size_t bytesRead,
                F& callback)
    {
        rxBuffer_.reset();

        std::error_code ec = websocketErrorCodeToStandard(netEc);
        if (netEc == boost::beast::websocket::error::closed)
            ec = interpretCloseReason(websocket_->reason());

        if (!ec)
        {
            if (websocket_->text() && websocket_->got_binary())
                ec = make_error_code(TransportErrc::expectedText);
            if (websocket_->binary() && websocket_->got_text())
                ec = make_error_code(TransportErrc::expectedBinary);
        }

        callback(ec, bytesRead, websocket_->is_message_done());
    }

    template <typename F>
    void closeWebsocket(boost::beast::websocket::close_code reason,
                        F&& callback)
    {
        struct Closed
        {
            Decay<F> callback;

            void operator()(boost::beast::error_code netEc)
            {
                callback(static_cast<std::error_code>(netEc));
            }
        };

        websocket_->control_callback();
        websocket_->async_close(reason, Closed{std::forward<F>(callback)});
    }

    boost::optional<Socket> websocket_;
    boost::optional<DynamicBufferAdapter> rxBuffer_;
};

//------------------------------------------------------------------------------
class WebsocketAdmitter
    : public std::enable_shared_from_this<WebsocketAdmitter>
{
public:
    using Ptr             = std::shared_ptr<WebsocketAdmitter>;
    using Stream          = WebsocketStream;
    using ListenerSocket  = boost::asio::ip::tcp::socket;
    using Socket          = boost::beast::websocket::stream<ListenerSocket>;
    using Settings        = WebsocketEndpoint;
    using SettingsPtr     = std::shared_ptr<Settings>;
    using Handler         = AnyCompletionHandler<void (AdmitResult)>;
    using UpgradeRequest =
        boost::beast::http::request<boost::beast::http::string_body>;

    WebsocketAdmitter(ListenerSocket&& t, SettingsPtr s, const CodecIdSet& c)
        : tcpSocket_(std::move(t)),
          codecIds_(c),
          settings_(std::move(s))
    {
        response_.base().set(boost::beast::http::field::server,
                             settings_->agent());
    }

    void admit(bool isShedding, Handler handler)
    {
        isShedding_ = isShedding;
        handler_ = std::move(handler);

        // https://github.com/boostorg/beast/issues/971#issuecomment-356306911
        requestParser_.emplace();
        const auto headerSizeLimit = settings_->limits().headerSize();
        if (headerSizeLimit != 0)
            requestParser_->header_limit(headerSizeLimit);

        auto self = shared_from_this();
        boost::beast::http::async_read(
            tcpSocket_, buffer_, *requestParser_,
            [this, self] (const boost::beast::error_code& netEc, std::size_t)
            {
                if (check(netEc, "socket read"))
                    acceptHandshake();
            });
    }

    template <typename F>
    void shutdown(std::error_code /*reason*/, F&& callback)
    {
        TcpSocket* socket = websocket_.has_value() ? &websocket_->next_layer()
                                                   : &tcpSocket_;
        boost::system::error_code netEc;
        socket->shutdown(TcpSocket::shutdown_send, netEc);
        auto ec = static_cast<std::error_code>(netEc);
        postAny(socket->get_executor(), std::forward<F>(callback), ec);
    }

    void close()
    {
        if (websocket_.has_value())
            websocket_->next_layer().close();
        else
            tcpSocket_.close();
    }

    void upgrade(const UpgradeRequest& request, Handler handler)
    {
        handler_ = std::move(handler);
        performUpgrade(request);
    }

    const TransportInfo& transportInfo() const {return transportInfo_;}

    Socket&& releaseSocket()
    {
        assert(websocket_.has_value());
        return std::move(*websocket_);
    }

private:
    using TcpSocket = ListenerSocket;
    using HttpStatus = boost::beast::http::status;
    using Parser =
        boost::beast::http::request_parser<boost::beast::http::empty_body>;

    struct Decorator
    {
        std::string agent;
        std::string subprotocol;

        void operator()(boost::beast::http::response_header<>& header)
        {
            using boost::beast::http::field;
            header.set(field::server, agent);
            header.set(field::sec_websocket_protocol, subprotocol);
        }
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

    void acceptHandshake()
    {
        // TODO: Multiplex websocket transports with same port but different
        //       request-target URIs.

        // Check that we actually received a websocket upgrade request
        assert(requestParser_.has_value());
        if (!requestParser_->upgrade())
        {
            return finish(AdmitResult::rejected(
                boost::beast::websocket::error::no_connection_upgrade));
        }

        // Send an error response if the server connection limit
        // has been reached
        if (isShedding_)
        {
            return reject("Connection limit reached",
                          HttpStatus::service_unavailable,
                          AdmitResult::shedded());
        }

        performUpgrade(requestParser_->get());
    }

    template <typename TRequest>
    void performUpgrade(const TRequest& request)
    {
        // Parse the subprotocol to determine the peer's desired codec
        using boost::beast::http::field;
        auto found = request.base().find(field::sec_websocket_protocol);
        if (found == request.base().end())
        {
            return reject("No subprotocol was requested",
                          HttpStatus::bad_request,
                          AdmitResult::rejected(TransportErrc::noSerializer));
        }
        auto subprotocol = found->value();
        codecId_ = parseSubprotocol(subprotocol);
        if (codecIds_.count(codecId_) == 0)
        {
            return reject("Requested subprotocol is not supported",
                          HttpStatus::bad_request,
                          AdmitResult::rejected(TransportErrc::badSerializer));
        }

        // Transfer the TCP socket to a new websocket stream
        websocket_.emplace(std::move(tcpSocket_));

        // Set the Server and Sec-WebsocketSocket-Protocol fields of
        // the handshake
        websocket_->set_option(boost::beast::websocket::stream_base::decorator(
            Decorator{settings_->agent(), subprotocol}));

        setPermessageDeflateOptions();

        // Complete the handshake
        auto self = shared_from_this();
        websocket_->async_accept(
            request,
            [this, self](boost::beast::error_code netEc)
            {
                if (check(netEc, "handshake accepted write"))
                    complete();
            });
    }

    void setPermessageDeflateOptions()
    {
        const auto& opts = settings_->permessageDeflate();
        if (!opts.enabled())
            return;

        boost::beast::websocket::permessage_deflate pd;
        pd.server_enable = true;
        pd.server_max_window_bits = opts.maxWindowBits();
        pd.server_no_context_takeover = opts.noContextTakeover();
        pd.compLevel = opts.compressionLevel();
        pd.memLevel = opts.memoryLevel();
        pd.msg_size_threshold = opts.threshold();

        websocket_->set_option(pd);
    }

    void reject(std::string msg, HttpStatus status, AdmitResult result)
    {
        namespace http = boost::beast::http;
        response_.result(status);
        response_.body() = std::move(msg);
        auto self = shared_from_this();
        http::async_write(
            tcpSocket_, response_,
            [this, self, result](boost::beast::error_code netEc, std::size_t)
            {
                if (check(netEc, "handshake rejected write"))
                    finish(result);
            });
    }

    void complete()
    {
        assert(websocket_.has_value());

        if (subprotocolIsText(codecId_))
            websocket_->text(true);
        else
            websocket_->binary(true);

        const auto txLimit = settings_->limits().txMsgSize();
        const auto rxLimit = settings_->limits().rxMsgSize();
        transportInfo_ = TransportInfo{codecId_, txLimit, rxLimit};

        finish(AdmitResult::wamp(codecId_));
    }

    bool check(boost::system::error_code netEc, const char* operation)
    {
        if (netEc)
            fail(websocketErrorCodeToStandard(netEc), operation);
        return !netEc;
    }

    void fail(std::error_code ec, const char* operation)
    {
        if (!handler_)
            return;
        handler_(AdmitResult::failed(ec, operation));
        handler_ = nullptr;
    }

    template <typename TErrc>
    void fail(TErrc errc, const char* operation)
    {
        fail(static_cast<std::error_code>(make_error_code(errc)), operation);
    }

    void finish(AdmitResult result)
    {
        if (handler_)
            handler_(result);
        handler_ = nullptr;
    }

    TcpSocket tcpSocket_;
    boost::optional<Socket> websocket_;
    CodecIdSet codecIds_;
    TransportInfo transportInfo_;
    SettingsPtr settings_;
    Handler handler_;
    boost::beast::flat_buffer buffer_;
    boost::optional<Parser> requestParser_;
    boost::beast::http::response<boost::beast::http::string_body> response_;
    int codecId_ = 0;
    bool isShedding_ = false;
};

//------------------------------------------------------------------------------
using WebsocketClientTransport =
    BasicClientTransport<WebsocketHost, WebsocketStream>;

//------------------------------------------------------------------------------
class WebsocketServerTransport
    : public BasicServerTransport<WebsocketEndpoint, WebsocketAdmitter>
{
    using Base = BasicServerTransport<WebsocketEndpoint, WebsocketAdmitter>;

public:
    using Ptr = std::shared_ptr<WebsocketServerTransport>;

    class PassKey
    {
        constexpr PassKey() = default;
        friend class HttpTransport;
    };

    WebsocketServerTransport(ListenerSocket&& l, SettingsPtr s,
                             CodecIdSet c, RouterLogger::Ptr = {})
        : Base(std::move(l), std::move(s), std::move(c), {})
    {}

    void httpStart(PassKey, RxHandler r, TxErrorHandler t)
    {
        Base::onStart(std::move(r), std::move(t));
    }

    void httpSend(PassKey, MessageBuffer message)
    {
        Base::onSend(std::move(message));
    }

    void httpAbort(PassKey, MessageBuffer message)
    {
        Base::onAbort(std::move(message));
    }

    void httpShutdown(PassKey, std::error_code reason, ShutdownHandler handler)
    {
        Base::onShutdown(reason, std::move(handler));
    }

    void httpClose(PassKey) {Base::onClose();}
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WEBSOCKETTRANSPORT_HPP
