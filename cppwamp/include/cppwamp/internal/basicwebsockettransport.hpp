/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023-2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_BASICWEBSOCKETTRANSPORT_HPP
#define CPPWAMP_INTERNAL_BASICWEBSOCKETTRANSPORT_HPP

#include <cstddef>
#include <memory>
#include <utility>
#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/websocket/option.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/optional/optional.hpp>
#include "../anyhandler.hpp"
#include "../codec.hpp"
#include "../errorcodes.hpp"
#include "../queueingclienttransport.hpp"
#include "../queueingservertransport.hpp"
#include "../traits.hpp"
#include "../transports/websocketprotocol.hpp"
#include "../wampdefs.hpp"
#include "httpurlvalidator.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
inline bool isHttpParseErrorDueToClient(boost::system::error_code netEc)
{
    using E = boost::beast::http::error;
    const auto& cat = make_error_code(E::end_of_stream).category();
    if (netEc.category() != cat)
        return false;

    auto code = static_cast<E>(netEc.value());
    switch (code)
    {
    case E::end_of_stream:           return false;
    case E::partial_message:         return true;
    case E::need_more:               return false;
    case E::unexpected_body:         return true;
    case E::need_buffer:             return false;
    case E::end_of_chunk:            return false;
    case E::buffer_overflow:         return false;
    case E::header_limit:            return true;
    case E::body_limit:              return true;
    case E::bad_alloc:               return false;
    case E::bad_line_ending:         return true;
    case E::bad_method:              return true;
    case E::bad_target:              return true;
    case E::bad_version:             return true;
    case E::bad_status:              return true;
    case E::bad_reason:              return true;
    case E::bad_field:               return true;
    case E::bad_value:               return true;
    case E::bad_content_length:      return true;
    case E::bad_transfer_encoding:   return true;
    case E::bad_chunk:               return true;
    case E::bad_chunk_extension:     return true;
    case E::bad_obs_fold:            return false;
    case E::multiple_content_length: return true;
    case E::stale_parser:            return false;
    case E::short_read:              return false;
    default:                         break;
    }

    return false;
}

//------------------------------------------------------------------------------
inline std::error_code websocketErrorCodeToStandard(
    boost::system::error_code netEc)
{
    if (!netEc)
        return {};

    namespace AE = boost::asio::error;
    bool disconnected = netEc == AE::broken_pipe ||
                        netEc == AE::connection_reset ||
                        netEc == AE::eof;
    if (disconnected)
        return make_error_code(TransportErrc::disconnected);
    if (netEc == AE::operation_aborted)
        return make_error_code(TransportErrc::aborted);

    using WE = boost::beast::websocket::error;
    if (netEc == WE::closed)
        return make_error_code(TransportErrc::ended);
    if (netEc == WE::buffer_overflow || netEc == WE::message_too_big)
        return make_error_code(TransportErrc::inboundTooLong);

    return static_cast<std::error_code>(netEc);
}

//------------------------------------------------------------------------------
inline boost::beast::websocket::close_code
errorCodeToWebsocketCloseCode(std::error_code ec)
{
    using boost::beast::websocket::close_code;
    using boost::beast::websocket::condition;
    using boost::beast::websocket::error;

    if (!ec)
        return close_code::normal;

    if (ec.category() == transportCategory())
    {
        switch (static_cast<TransportErrc>(ec.value()))
        {
        case TransportErrc::ended:          return close_code::going_away;
        case TransportErrc::inboundTooLong: return close_code::too_big;
        case TransportErrc::expectedBinary: return close_code::bad_payload;
        case TransportErrc::expectedText:   return close_code::bad_payload;
        case TransportErrc::shedded:        return close_code::try_again_later;
        default: break;
        }
    }
    if (ec.category() == serverCategory())
    {
        switch (static_cast<ServerErrc>(ec.value()))
        {
        case ServerErrc::overloaded:        return close_code::try_again_later;
        case ServerErrc::shedded:           return close_code::try_again_later;
        case ServerErrc::evicted:           return close_code::try_again_later;
        default: break;
        }
    }
    else
    {
        auto netEc = static_cast<boost::system::error_code>(ec);
        if (netEc == condition::protocol_violation)
            return close_code::protocol_error;
    }

    return close_code::internal_error;
}

//------------------------------------------------------------------------------
template <typename TSocket, typename TSettings>
void setWebsocketOptions(TSocket& socket, const TSettings& settings,
                         bool isServer)
{
    const auto& pmd = settings.options().permessageDeflate();
    if (pmd.enabled())
    {
        boost::beast::websocket::permessage_deflate pd;
        pd.server_enable = true;
        pd.server_max_window_bits = pmd.maxWindowBits();
        pd.server_no_context_takeover = pmd.noContextTakeover();
        pd.compLevel = pmd.compressionLevel();
        pd.memLevel = pmd.memoryLevel();
        pd.msg_size_threshold = pmd.threshold();

        socket.set_option(pd);
    }

    const auto& limits = settings.limits();
    socket.write_buffer_bytes(limits.websocketWriteIncrement());
    socket.auto_fragment(true);
}

//------------------------------------------------------------------------------
template <typename TTraits>
class BasicWebsocketStream
{
public:
    using TcpSocket = boost::asio::ip::tcp::socket;
    using Socket    = typename TTraits::Socket;

    template <typename TSocket>
    static ConnectionInfo makeConnectionInfo(const TSocket& s)
    {
        return TTraits::makeConnectionInfo(s);
    }

    template <typename S>
    explicit BasicWebsocketStream(Socket&& ws,
                                  const std::shared_ptr<S>& settings)
        : websocket_(std::move(ws)),
          readIncrementSize_(settings->limits().websocketReadIncrement())
    {
        auto n = settings->limits().wampReadMsgSize();
        if (n != 0)
            websocket_->read_message_max(n);
    }

    BasicWebsocketStream(BasicWebsocketStream&& rhs)
        : websocket_(std::move(rhs.websocket_).value())
    {}

    BasicWebsocketStream& operator=(BasicWebsocketStream&& rhs)
    {
        websocket_.emplace(std::move(rhs.websocket_).value());
        rhs.websocket_.reset();
        return *this;
    }

    AnyIoExecutor executor() {return websocket_->get_executor();}

    bool isOpen() const
    {
        return tcpLayer().is_open() && websocket_->is_open();
    }

    template <typename F>
    void observeHeartbeats(F&& callback)
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

    void unobserveHeartbeats()
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
    void awaitRead(MessageBuffer& buffer, F&& callback)
    {
        doReadSome(buffer, 1, std::forward<F>(callback));
    }

    template <typename F>
    void readSome(MessageBuffer& buffer, F&& callback)
    {
        doReadSome(buffer, readIncrementSize_, std::forward<F>(callback));
    }

    template <typename F>
    void shutdown(std::error_code reason, F&& callback)
    {
        if (!websocket_->is_open())
            return shutdownNextLayer(IsTls{}, std::forward<F>(callback));

        struct Closed
        {
            Decay<F> callback;

            void operator()(boost::beast::error_code netEc)
            {
                // https://security.stackexchange.com/a/91442/169835
                ignoreSslTruncation(netEc);

                callback(static_cast<std::error_code>(netEc));
            }
        };

        websocket_->control_callback();
        websocket_->async_close(errorCodeToWebsocketCloseCode(reason),
                                Closed{std::forward<F>(callback)});
    }

    void close() {tcpLayer().close();}

private:
    using IsTls = typename TTraits::IsTls;

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

    static void
    treatSslTruncationAsDisconnection(boost::system::error_code& netEc)
    {
        // https://security.stackexchange.com/a/91442/169835
        if (TTraits::isSslTruncationError(netEc))
            netEc = make_error_code(boost::asio::error::connection_reset);
    }

    static void ignoreSslTruncation(boost::system::error_code& netEc)
    {
        // https://security.stackexchange.com/a/91442/169835
        if (TTraits::isSslTruncationError(netEc))
            netEc = {};
    }

    template <typename F>
    void doReadSome(MessageBuffer& buffer, std::size_t limit, F&& callback)
    {
        struct Received
        {
            Decay<F> callback;
            BasicWebsocketStream* self;

            void operator()(boost::beast::error_code netEc, std::size_t n)
            {
                self->onRead(netEc, n, callback);
            }
        };

        rxBuffer_.emplace(buffer);
        websocket_->async_read_some(
            *rxBuffer_,
            limit, // Beast will choose 1536
            Received{std::forward<F>(callback), this});
    }

    template <typename F>
    void onRead(boost::beast::error_code netEc, std::size_t bytesRead,
                F& callback)
    {
        rxBuffer_.reset();
        treatSslTruncationAsDisconnection(netEc);
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

    // Non-TLS overload
    template <typename F>
    void shutdownNextLayer(FalseType, F&& callback)
    {
        boost::system::error_code netEc;
        tcpLayer().shutdown(boost::asio::ip::tcp::socket::shutdown_send, netEc);
        postAny(websocket_->get_executor(),
                std::forward<F>(callback),
                static_cast<std::error_code>(netEc));
    }

    // TLS overload
    template <typename F>
    void shutdownNextLayer(TrueType, F&& callback)
    {
        struct Shutdown
        {
            Decay<F> callback;
            BasicWebsocketStream* self;

            void operator()(boost::system::error_code netEc)
            {
                ignoreSslTruncation(netEc);
                callback(netEc);
            }
        };

        websocket_->next_layer().async_shutdown(
            Shutdown{std::forward<F>(callback), this});
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
                ignoreSslTruncation(netEc);
                callback(static_cast<std::error_code>(netEc), false);
            }
        };

        websocket_->control_callback();
        websocket_->async_close(reason, Closed{std::forward<F>(callback)});
    }

    const TcpSocket& tcpLayer() const {return TTraits::tcpLayer(*websocket_);}

    TcpSocket& tcpLayer() {return TTraits::tcpLayer(*websocket_);}

    boost::optional<Socket> websocket_;
    boost::optional<DynamicBufferAdapter> rxBuffer_;
    std::size_t readIncrementSize_ = 0;
};

//------------------------------------------------------------------------------
template <typename TTraits>
class BasicWebsocketAdmitter
    : public std::enable_shared_from_this<BasicWebsocketAdmitter<TTraits>>
{
public:
    using Ptr            = std::shared_ptr<BasicWebsocketAdmitter>;
    using Stream         = BasicWebsocketStream<TTraits>;
    using ListenerSocket = typename TTraits::HttpSocket; // TCP or TLS
    using Socket         = typename TTraits::Socket;
    using Settings       = typename TTraits::ServerSettings;
    using SettingsPtr    = std::shared_ptr<Settings>;
    using Handler        = AnyCompletionHandler<void (AdmitResult)>;

    BasicWebsocketAdmitter(ListenerSocket&& h, SettingsPtr s,
                           const CodecIdSet& c)
        : httpSocket_(std::move(h)),
          executor_(httpSocket_.get_executor()),
          codecIds_(c),
          settings_(std::move(s))
    {
        response_.base().set(boost::beast::http::field::server,
                             settings_->options().agent());
    }

    void admit(bool isShedding, Handler handler)
    {
        isShedding_ = isShedding;

        // The parser is not resettable; the Beast author recommends wrapping
        // it in boost::optional.
        // https://github.com/boostorg/beast/issues/971#issuecomment-356306911
        requestParser_.emplace();
        const auto requestHeaderSizeLimit =
            settings_->limits().requestHeaderSize();
        if (requestHeaderSizeLimit != 0)
            requestParser_->header_limit(requestHeaderSizeLimit);

        doAdmit(IsTls{}, std::move(handler));
    }

    template <typename F>
    void shutdown(std::error_code reason, F&& callback)
    {
        if (handler_)
        {
            post(std::move(handler_), AdmitResult::cancelled(reason));
            handler_ = nullptr;
        }

        if (!websocket_.has_value() || !websocket_->is_open())
            return shutdownNextLayer(IsTls{}, std::forward<F>(callback));

        struct Closed
        {
            Decay<F> callback;

            void operator()(boost::beast::error_code netEc)
            {
                ignoreSslTruncation(netEc);
                callback(static_cast<std::error_code>(netEc));
            }
        };

        websocket_->control_callback();
        websocket_->async_close(errorCodeToWebsocketCloseCode(reason),
                                Closed{std::forward<F>(callback)});
    }

    void close()
    {
        if (websocket_.has_value())
            TTraits::tcpLayer(*websocket_).close();
        else
            TTraits::tcpLayer(httpSocket_).close();
    }

    template <typename TRequest>
    void upgrade(const TRequest& request, Handler handler)
    {
        handler_ = std::move(handler);
        performUpgrade(request);
    }

    const TransportInfo& transportInfo() const {return transportInfo_;}

    std::string releaseTargetPath() {return std::string{std::move(target_)};}

    Socket&& releaseSocket()
    {
        assert(websocket_.has_value());
        return std::move(*websocket_);
    }

private:
    using IsTls = typename TTraits::IsTls;
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

    static void
    treatSslTruncationAsDisconnection(boost::system::error_code& netEc)
    {
        // https://security.stackexchange.com/a/91442/169835
        if (TTraits::isSslTruncationError(netEc))
            netEc = make_error_code(boost::asio::error::connection_reset);
    }

    static void ignoreSslTruncation(boost::system::error_code& netEc)
    {
        // https://security.stackexchange.com/a/91442/169835
        if (TTraits::isSslTruncationError(netEc))
            netEc = {};
    }

    // Non-TLS overload
    template <typename F>
    void doAdmit(FalseType, F&& handler)
    {
        handler_ = std::forward<F>(handler);
        readUpgradeRequest();
    }

    // TLS overload
    template <typename F>
    void doAdmit(TrueType, F&& handler)
    {
        handler_ = std::forward<F>(handler);

        auto self = this->shared_from_this();
        httpSocket_.async_handshake(
            Socket::next_layer_type::server,
            [this, self](boost::system::error_code netEc)
            {
                if (netEc)
                {
                    treatSslTruncationAsDisconnection(netEc);
                    auto ec = websocketErrorCodeToStandard(netEc);
                    return fail(ec, "SSL/TLS handshake");
                }

                readUpgradeRequest();
            });
    }

    void readUpgradeRequest()
    {
        auto self = this->shared_from_this();
        boost::beast::http::async_read(
            httpSocket_, buffer_, *requestParser_,
            [this, self] (const boost::beast::error_code& netEc, std::size_t)
            {
                if (checkReadRequest(netEc))
                    acceptWebsocketHandshake();
            });
    }

    bool checkReadRequest(boost::system::error_code netEc)
    {
        if (!netEc)
            return true;

        treatSslTruncationAsDisconnection(netEc);
        auto ec = websocketErrorCodeToStandard(netEc);

        if (isHttpParseErrorDueToClient(netEc))
        {
            reject("Bad request", HttpStatus::bad_request,
                   AdmitResult::rejected(ec));
        }
        else
        {
            fail(ec, "request read");
        }
        return false;
    }

    void acceptWebsocketHandshake()
    {
        // Check that we actually received a websocket upgrade request
        assert(requestParser_.has_value());
        if (!requestParser_->upgrade())
        {
            auto errc = boost::beast::websocket::error::no_connection_upgrade;
            return reject(
                "This service requires use of the Websocket protocol.",
                HttpStatus::upgrade_required,
                AdmitResult::rejected(errc));
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

        // Validate and store the request-target string
        auto normalized =
            HttpUrlValidator::interpretAndNormalize(
                request.target(),
                boost::beast::http::verb::get);
        if (!normalized)
        {
            return reject("Invalid request-target",
                          HttpStatus::bad_request,
                          AdmitResult::rejected(TransportErrc::badHandshake));
        }

        target_ = normalized->buffer();

        // Transfer the TCP socket to a new websocket stream
        websocket_.emplace(std::move(httpSocket_));

        // Set the Server and Sec-WebsocketSocket-Protocol fields of
        // the handshake
        websocket_->set_option(boost::beast::websocket::stream_base::decorator(
            Decorator{settings_->options().agent(), subprotocol}));

        setWebsocketOptions(*websocket_, *settings_, true);

        // Complete the handshake
        auto self = this->shared_from_this();
        websocket_->async_accept(
            request,
            [this, self](boost::beast::error_code netEc)
            {
                if (checkAccept(netEc))
                    complete();
            });
    }

    bool checkAccept(boost::system::error_code netEc)
    {
        if (!netEc)
            return true;

        const auto& websocketErrorCat =
            make_error_code(boost::beast::websocket::error::closed).category();
        bool isWebsocketError = netEc.category() == websocketErrorCat;
        auto ec = websocketErrorCodeToStandard(netEc);

        if (isWebsocketError || isHttpParseErrorDueToClient(netEc))
        {
            reject("Bad request", HttpStatus::bad_request,
                   AdmitResult::rejected(ec));
        }
        else
        {
            fail(ec, "handshake accept");
        }
        return false;
    }

    void reject(std::string msg, HttpStatus status, AdmitResult result)
    {
        namespace http = boost::beast::http;

        response_.result(status);

        if (status == HttpStatus::upgrade_required)
        {
            response_.set(boost::beast::http::field::connection, "Upgrade");
            response_.set(boost::beast::http::field::upgrade, "websocket");
        }

        response_.body() = std::move(msg);

        ListenerSocket* socket =
            websocket_.has_value() ? &websocket_->next_layer() : &httpSocket_;

        auto self = this->shared_from_this();
        http::async_write(
            *socket, response_,
            [this, self, result](boost::beast::error_code netEc, std::size_t)
            {
                if (checkRejectWrite(netEc))
                    finish(result);
            });
    }

    bool checkRejectWrite(boost::system::error_code netEc)
    {
        if (netEc)
            fail(websocketErrorCodeToStandard(netEc), "handshake reject");
        return !netEc;
    }

    void complete()
    {
        assert(websocket_.has_value());

        if (subprotocolIsText(codecId_))
            websocket_->text(true);
        else
            websocket_->binary(true);

        const auto txLimit = settings_->limits().wampWriteMsgSize();
        const auto rxLimit = settings_->limits().wampReadMsgSize();
        transportInfo_ = TransportInfo{codecId_, txLimit, rxLimit};

        finish(AdmitResult::wamp(codecId_));
    }

    // Non-TLS overload
    template <typename F>
    void shutdownNextLayer(FalseType, F&& callback)
    {
        ListenerSocket* tcpSocket =
            websocket_.has_value() ? &websocket_->next_layer()
                                   : &httpSocket_;
        boost::system::error_code netEc;
        tcpSocket->shutdown(
            boost::asio::ip::tcp::socket::shutdown_send, netEc);
        post(std::forward<F>(callback),
             static_cast<std::error_code>(netEc));
        return;
    }

    // TLS overload
    template <typename F>
    void shutdownNextLayer(TrueType, F&& callback)
    {
        struct Shutdown
        {
            Decay<F> callback;
            Ptr self;

            void operator()(boost::system::error_code netEc)
            {
                ignoreSslTruncation(netEc);
                auto ec = static_cast<std::error_code>(netEc);
                self->post(std::move(callback), ec);
            }
        };

        ListenerSocket* sslSocket =
            websocket_.has_value() ? &websocket_->next_layer()
                                   : &httpSocket_;

        auto self = this->shared_from_this();
        sslSocket->async_shutdown(
            Shutdown{std::forward<F>(callback), std::move(self)});
    }

    void fail(std::error_code ec, const char* operation)
    {
        close();
        finish(AdmitResult::failed(ec, operation));
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

    template <typename F, typename... Ts>
    void post(F&& handler, Ts&&... args)
    {
        postAny(executor_, std::forward<F>(handler), std::forward<Ts>(args)...);
    }

    ListenerSocket httpSocket_;
    boost::optional<Socket> websocket_;
    AnyIoExecutor executor_;
    CodecIdSet codecIds_;
    TransportInfo transportInfo_;
    SettingsPtr settings_;
    Handler handler_;
    boost::beast::flat_buffer buffer_;
    boost::optional<Parser> requestParser_;
    boost::beast::http::response<boost::beast::http::string_body> response_;
    std::string target_;
    int codecId_ = 0;
    bool isShedding_ = false;
};

//------------------------------------------------------------------------------
template <typename TTraits>
using BasicWebsocketClientTransport =
    QueueingClientTransport<typename TTraits::ClientSettings,
                            BasicWebsocketStream<TTraits>,
                            typename TTraits::SslContextType>;

//------------------------------------------------------------------------------
template <typename TTraits>
class BasicWebsocketServerTransport
    : public QueueingServerTransport<typename TTraits::ServerSettings,
                                     BasicWebsocketAdmitter<TTraits>,
                                     typename TTraits::SslContextType>
{
    using Base = QueueingServerTransport<typename TTraits::ServerSettings,
                                         BasicWebsocketAdmitter<TTraits>,
                                         typename TTraits::SslContextType>;

public:
    using Ptr = std::shared_ptr<BasicWebsocketServerTransport>;
    using ListenerSocket  = typename Base::ListenerSocket;
    using SettingsPtr     = typename Base::SettingsPtr;
    using SslContextType  = typename Base::SslContextType;
    using RxHandler       = typename Base::RxHandler;
    using TxErrorHandler  = typename Base::TxErrorHandler;
    using ShutdownHandler = typename Base::ShutdownHandler;

    class PassKey
    {
        constexpr PassKey() = default;
        friend class HttpTransport;
    };

    BasicWebsocketServerTransport(ListenerSocket&& l, SettingsPtr s,
                                  CodecIdSet c, RouterLogger::Ptr = {},
                                  SslContextType ssl = {})
        : Base(std::move(l), std::move(s), std::move(c), {}, std::move(ssl))
    {}

    void httpStart(PassKey, RxHandler r, TxErrorHandler t)
    {
        Base::onStart(std::move(r), std::move(t));
    }

    void httpSend(PassKey, MessageBuffer message)
    {
        Base::onSend(std::move(message));
    }

    void httpAbort(PassKey, MessageBuffer message, ShutdownHandler handler)
    {
        Base::onAbort(std::move(message), std::move(handler));
    }

    void httpShutdown(PassKey, std::error_code reason, ShutdownHandler handler)
    {
        Base::onShutdown(reason, std::move(handler));
    }

    void httpClose(PassKey) {Base::onClose();}
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_BASICWEBSOCKETTRANSPORT_HPP
