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
    using Socket =
        boost::beast::websocket::stream<boost::asio::ip::tcp::socket>;

    using Buffer = MessageBuffer;

    explicit WebsocketStream(Socket&& ws) : websocket_(std::move(ws)) {}

    AnyIoExecutor executor() {return websocket_->get_executor();}

    bool isOpen() const
    {
        return websocket_->next_layer().is_open() &&
               websocket_->is_open();
    }

    template <typename L>
    void setLimits(const L& limits)
    {
        auto n = limits.bodySizeLimit();
        if (n != 0)
            websocket_->read_message_max() = n;
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
    void write(const uint8_t* data, std::size_t size, F&& callback)
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
    void read(Buffer& buffer, F&& callback)
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
        websocket_->async_read_some(*rxBuffer_,
                                    Received{std::forward<F>(callback), this});
    }

    template <typename F>
    void shutdown(std::error_code reason, F&& callback)
    {
        assert(websocket_.has_value());

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

    void close()
    {
        if (!websocket_.has_value())
            return;
        websocket_->next_layer().close();
        websocket_.reset();
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
        assert(websocket_.has_value());

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

        assert(websocket_.has_value());
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
    using Socket          = boost::asio::ip::tcp::socket;
    using Settings        = WebsocketEndpoint;
    using SettingsPtr     = std::shared_ptr<Settings>;
    using Handler         = std::function<void (AdmitResult)>;
    using UpgradeRequest =
        boost::beast::http::request<boost::beast::http::string_body>;

    WebsocketAdmitter(Socket&& t, SettingsPtr s, const CodecIdSet& c)
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
        if (settings_->httpHeaderLimit() != 0)
            requestParser_->header_limit(settings_->httpHeaderLimit());

        auto self = shared_from_this();
        boost::beast::http::async_read(
            tcpSocket_, buffer_, *requestParser_,
            [this, self] (const boost::beast::error_code& netEc, std::size_t)
            {
                if (check(netEc, "socket read"))
                    acceptHandshake();
            });
    }

    void cancel()
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

    Stream releaseStream()
    {
        assert(websocket_.has_value());
        return Stream{std::move(*websocket_)};
    }

private:
    using WebsocketSocket = boost::beast::websocket::stream<Socket>;
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

        transportInfo_ = TransportInfo{codecId_,
                                       std::numeric_limits<std::size_t>::max(),
                                       settings_->limits().bodySizeLimit()};
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

    Socket tcpSocket_;
    boost::optional<WebsocketSocket> websocket_;
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
using WebsocketServerTransport =
    BasicServerTransport<WebsocketEndpoint, WebsocketAdmitter>;

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WEBSOCKETTRANSPORT_HPP
