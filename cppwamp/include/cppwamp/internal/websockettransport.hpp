/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_WEBSOCKETTRANSPORT_HPP
#define CPPWAMP_INTERNAL_WEBSOCKETTRANSPORT_HPP

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/optional/optional.hpp>
#include "../basictransport.hpp"
#include "../codec.hpp"
#include "../routerlogger.hpp"
#include "../traits.hpp"
#include "../transports/websocketprotocol.hpp"
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
    auto ec = disconnected
                  ? make_error_code(TransportErrc::disconnected)
                  : static_cast<std::error_code>(netEc);

    if (netEc == AE::operation_aborted)
        ec = make_error_code(TransportErrc::aborted);
    if (netEc == WE::buffer_overflow || netEc == WE::message_too_big)
        ec = make_error_code(TransportErrc::inboundTooLong);

    return ec;
}

//------------------------------------------------------------------------------
class WebsocketTransport : public BasicTransport<WebsocketTransport>
{
public:
    using Ptr             = std::shared_ptr<WebsocketTransport>;
    using TcpSocket       = boost::asio::ip::tcp::socket;
    using WebsocketSocket = boost::beast::websocket::stream<TcpSocket>;

protected:
    // Constructor for client transports
    WebsocketTransport(WebsocketSocket&& ws, TransportInfo info)
        : Base(boost::asio::make_strand(ws.get_executor()),
               makeConnectionInfo(ws.next_layer()),
               info),
        websocket_(std::move(ws))
    {}

    // Constructor for server transports
    WebsocketTransport(TcpSocket& tcp)
        : Base(boost::asio::make_strand(tcp.get_executor()),
               makeConnectionInfo(tcp))
    {}

    void assignWebsocket(WebsocketSocket&& ws, TransportInfo i)
    {
        websocket_.emplace(std::move(ws));
        Base::completeAdmission(i);
    }

private:
    using Base = BasicTransport<WebsocketTransport>;

    using DynamicBufferAdapter =
        boost::asio::dynamic_vector_buffer<MessageBuffer::value_type,
                                           MessageBuffer::allocator_type>;

    static ConnectionInfo makeConnectionInfo(const TcpSocket& socket)
    {
        return TcpTraits::connectionInfo(socket.remote_endpoint(), "WS");
    }

    static std::error_code interpretCloseReason(
        const boost::beast::websocket::close_reason& reason)
    {
        std::error_code ec = make_error_code(TransportErrc::disconnected);
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

    bool socketIsOpen() const
    {
        return websocket_.has_value() &&
               websocket_->next_layer().is_open() &&
               websocket_->is_open();
    }

    void enablePinging()
    {
        std::weak_ptr<Transporting> self = shared_from_this();

        assert(websocket_.has_value());
        websocket_->control_callback(
            [self, this](boost::beast::websocket::frame_type type,
                         boost::beast::string_view msg)
            {
                auto me = self.lock();
                if (me && type == boost::beast::websocket::frame_type::pong)
                {
                    using Byte = MessageBuffer::value_type;
                    const auto* ptr = reinterpret_cast<const Byte*>(msg.data());
                    Base::onPong(ptr, msg.size());
                }
            });
    }

    void disablePinging()
    {
        assert(websocket_.has_value());
        websocket_->control_callback();
    }

    void stopTransport()
    {
        assert(websocket_.has_value());
        websocket_->next_layer().close();
    }

    void closeTransport(CloseHandler handler)
    {
        if (socketIsOpen())
            closeWebsocket(boost::beast::websocket::normal, std::move(handler));
        else
            Base::post(std::move(handler), true);
    }

    void closeWebsocket(boost::beast::websocket::close_code reason,
                        CloseHandler&& handler = nullptr)
    {
        struct Closed
        {
            Ptr self;
            CloseHandler handler;

            void operator()(boost::beast::error_code netEc)
            {
                auto ec = static_cast<std::error_code>(netEc);
                self->onWebsocketClosed(ec, handler);
            }
        };

        assert(websocket_.has_value());
        websocket_->control_callback();

        auto self =
            std::dynamic_pointer_cast<WebsocketTransport>(shared_from_this());
        websocket_->async_close(reason,
                                Closed{std::move(self), std::move(handler)});
    }

    void onWebsocketClosed(std::error_code ec, CloseHandler& handler)
    {
        assert(websocket_.has_value());
        websocket_->next_layer().close();
        if (handler == nullptr)
            return;
        if (ec)
            return Base::post(std::move(handler), makeUnexpected(ec));
        Base::post(std::move(handler), true);
    }

    void cancelClose()
    {
        assert(websocket_.has_value());
        websocket_->next_layer().close();
    }

    void failTransport(std::error_code ec)
    {
        if (!websocket_.has_value())
            return;

        if (!websocket_->is_open())
        {
            websocket_->next_layer().close();
            return;
        }

        using boost::beast::websocket::close_code;
        using boost::beast::websocket::condition;
        using boost::beast::websocket::error;

        auto closeCode = close_code::internal_error;

        if (ec == TransportErrc::inboundTooLong)
        {
            closeCode = close_code::too_big;
        }
        else if (ec == TransportErrc::expectedBinary ||
                 ec == TransportErrc::expectedText)
        {
            closeCode = close_code::bad_payload;
        }
        else
        {
            auto netEc = static_cast<boost::system::error_code>(ec);
            if (netEc == condition::protocol_violation)
                closeCode = close_code::protocol_error;
        }

        closeWebsocket(closeCode);
    }

    template <typename F>
    void transmitMessage(TransportFrameKind kind, const MessageBuffer& payload,
                         F&& callback)
    {
        using K = TransportFrameKind;
        switch (kind)
        {
        case K::wamp:
            return transmitWampMessage(payload, std::forward<F>(callback));

        case K::ping:
            return transmitPing(payload, std::forward<F>(callback));

        default:
            assert(false && "Unexpected TransportFrameKind enumerator");
        }
    }

    template <typename F>
    void transmitWampMessage(const MessageBuffer& payload, F&& callback)
    {
        struct Written
        {
            Decay<F> callback;

            void operator()(boost::beast::error_code netEc, size_t)
            {
                callback(websocketErrorCodeToStandard(netEc));
            }
        };

        assert(websocket_.has_value());
        websocket_->async_write(
            boost::asio::buffer(payload.data(), payload.size()),
            Written{std::forward<F>(callback)});
    }

    template <typename F>
    void transmitPing(const MessageBuffer& payload, F&& callback)
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

        const auto size = payload.size();
        assert(size <= PingData::static_capacity);
        const auto* ptr = reinterpret_cast<const CharType*>(payload.data());

        // Beast copies the payload
        assert(websocket_.has_value());
        websocket_->async_ping(PingData{ptr, ptr + size},
                               Pinged{std::forward<F>(callback)});
    }

    template <typename F>
    void receiveMessage(MessageBuffer& payload, F&& callback)
    {
        struct Received
        {
            Decay<F> callback;
            WebsocketTransport* self;

            void operator()(boost::beast::error_code netEc, std::size_t)
            {
                self->onMessageReceived(netEc, callback);
            }
        };

        rxBuffer_.emplace(payload);
        assert(websocket_.has_value());
        websocket_->async_read(*rxBuffer_,
                               Received{std::forward<F>(callback), this});
    }

    template <typename F>
    void onMessageReceived(boost::beast::error_code netEc, F& callback)
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

        if (ec)
            callback(makeUnexpected(ec));
        return callback(true);
    }

    boost::optional<WebsocketSocket> websocket_;
    boost::optional<DynamicBufferAdapter> rxBuffer_;

    friend class BasicTransport<WebsocketTransport>;
};

//------------------------------------------------------------------------------
class WebsocketClientTransport : public WebsocketTransport
{
public:
    using Ptr = std::shared_ptr<WebsocketClientTransport>;
    using Settings = WebsocketHost;

    WebsocketClientTransport(WebsocketSocket&& w, const Settings& s,
                             TransportInfo info)
        : Base(std::move(w), info)
    {
        Base::setAbortTimeout(s.abortTimeout());
    }

private:
    using Base = WebsocketTransport;
};

//------------------------------------------------------------------------------
class WebsocketAdmitter : public std::enable_shared_from_this<WebsocketAdmitter>
{
public:
    using Ptr             = std::shared_ptr<WebsocketAdmitter>;
    using TcpSocket       = boost::asio::ip::tcp::socket;
    using Settings        = WebsocketEndpoint;
    using SettingsPtr     = std::shared_ptr<Settings>;
    using WebsocketSocket = boost::beast::websocket::stream<TcpSocket>;
    using Handler         = AnyCompletionHandler<void (AdmitResult)>;
    using UpgradeRequest =
        boost::beast::http::request<boost::beast::http::string_body>;

    WebsocketAdmitter(TcpSocket&& t, SettingsPtr s, const CodecIdSet& c)
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

        auto self = this->shared_from_this();

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

    void timeout(boost::system::error_code ec)
    {
        if (!ec)
        {
            return finish(AdmitResult::rejected(TransportErrc::timeout));
        }

        if (ec != boost::asio::error::operation_aborted)
            fail(static_cast<std::error_code>(ec), "timer wait");
    }

    void upgrade(const UpgradeRequest& request, Handler handler)
    {
        handler_ = std::move(handler);
        performUpgrade(request);
    }

    const TransportInfo& transportInfo() const {return transportInfo_;}

    WebsocketSocket&& releaseSocket()
    {
        assert(websocket_.has_value());
        return std::move(*websocket_);
    }

private:
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

        websocket_->read_message_max(settings_->maxRxLength());

        transportInfo_ = TransportInfo{codecId_,
                                       std::numeric_limits<std::size_t>::max(),
                                       settings_->maxRxLength()};
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
        close();
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
        // Keep the socket open if we're establishing a WAMP connection
        if (result.status() != AdmitStatus::wamp)
            close();

        if (handler_)
            handler_(result);
        handler_ = nullptr;
    }

    void close()
    {
        if (websocket_.has_value())
        {
            websocket_->next_layer().close();
            websocket_.reset();
        }
        else
        {
            tcpSocket_.close();
        }
    }

    TcpSocket tcpSocket_;
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
class WebsocketServerTransport : public WebsocketTransport
{
public:
    using Ptr         = std::shared_ptr<WebsocketServerTransport>;
    using Settings    = WebsocketEndpoint;
    using SettingsPtr = std::shared_ptr<WebsocketEndpoint>;
    using UpgradeRequest =
        boost::beast::http::request<boost::beast::http::string_body>;

    WebsocketServerTransport(TcpSocket&& t, SettingsPtr s, const CodecIdSet& c,
                             RouterLogger::Ptr l)
        : Base(t),
          admitter_(std::make_shared<WebsocketAdmitter>(std::move(t),
                                                        std::move(s), c))
    {}

    void upgrade(const UpgradeRequest& request, AdmitHandler handler)
    {
        assert((admitter_ != nullptr) && "Admit already performed");

        struct Admitted
        {
            AdmitHandler handler;
            Ptr self;

            void operator()(AdmitResult result)
            {
                self->onAdmissionCompletion(result, handler);
            }
        };

        auto self = std::dynamic_pointer_cast<WebsocketServerTransport>(
            this->shared_from_this());
        admitter_->upgrade(request,
                           Admitted{std::move(handler), std::move(self)});
    }

private:
    using Base = WebsocketTransport;

    void onAdmit(Timeout timeout, AdmitHandler handler) override
    {
        assert((admitter_ != nullptr) && "Admit already performed");

        struct Admitted
        {
            AdmitHandler handler;
            Ptr self;

            void operator()(AdmitResult result)
            {
                self->onAdmissionCompletion(result, handler);
            }
        };

        auto self = std::dynamic_pointer_cast<WebsocketServerTransport>(
            this->shared_from_this());

        if (timeoutIsDefinite(timeout))
        {
            Base::timeoutAfter(
                timeout,
                [this, self](boost::system::error_code ec)
                {
                    if (admitter_)
                        admitter_->timeout(ec);
                });
        }

        bool isShedding = Base::state() == TransportState::shedding;
        admitter_->admit(isShedding,
                         Admitted{std::move(handler), std::move(self)});
    }

    void onCancelAdmission() override
    {
        if (admitter_)
            admitter_->cancel();
    }

    void onAdmissionCompletion(AdmitResult result, AdmitHandler& handler)
    {
        if (result.status() == AdmitStatus::wamp)
        {
            Base::assignWebsocket(admitter_->releaseSocket(),
                                  admitter_->transportInfo());
        }
        else
        {
            Base::shutdown();
        }

        Base::post(std::move(handler), result);
        admitter_.reset();
    }

    WebsocketAdmitter::Ptr admitter_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WEBSOCKETTRANSPORT_HPP
