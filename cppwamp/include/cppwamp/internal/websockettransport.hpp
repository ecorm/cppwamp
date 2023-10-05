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
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream.hpp>
#include "../basictransport.hpp"
#include "../codec.hpp"
#include "../routerlogger.hpp"
#include "../traits.hpp"
#include "../version.hpp"
#include "../transports/websocketprotocol.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// Byte container wrapper meeting the requirements of Beast's DynamicBuffer.
// Needed because websocket::stream::async_read only takes lvalues, and
// boost::asio::dynamic_vector_buffer cannot be reassigned.
// https://github.com/boostorg/beast/issues/1112
//------------------------------------------------------------------------------
template <typename TStorage>
class DynamicWebsocketBuffer
{
private:
    static constexpr auto sizeLimit_ = std::numeric_limits<std::size_t>::max();

public:
    using storage_type = TStorage;
    using const_buffers_type = boost::asio::const_buffer;
    using mutable_buffers_type = boost::asio::mutable_buffer;

    DynamicWebsocketBuffer() = default;

    DynamicWebsocketBuffer(TStorage& s, std::size_t maxSize = sizeLimit_)
        : storage_(&s),
        maxSize_(maxSize)
    {}

    std::size_t size() const noexcept
    {
        if (!storage_)
            return 0;
        return (size_ == noSize_) ? std::min(storage_->size(), max_size())
                                  : size_;
    }

    std::size_t max_size() const noexcept {return maxSize_;}

    std::size_t capacity() const noexcept
    {
        return storage_ ? std::min(storage_->capacity(), max_size()) : 0;
    }

    const_buffers_type data() const noexcept
    {
        if (storage_)
            return const_buffers_type{storage_->data(), storage_->size()};
        else
            return const_buffers_type{nullptr, 0};
    }

    mutable_buffers_type prepare(std::size_t n)
    {
        assert(storage_ != nullptr);
        auto s = size();
        auto m = max_size();
        if ((s > m) || (m - s < n))
        {
            throw std::length_error("wamp::DynamicWebsocketBuffer::prepare: "
                                    "buffer too long");
        }

        if (size_ == noSize_)
            size_ = storage_->size();
        storage_->resize(s + n);
        return boost::asio::buffer(boost::asio::buffer(*storage_) + size_, n);
    }

    void commit(std::size_t n)
    {
        assert(storage_ != nullptr);
        size_ += std::min(n, storage_->size() - size_);
        storage_->resize(size_);
    }

    void consume(std::size_t n)
    {
        assert(storage_ != nullptr);
        if (size_ != noSize_)
        {
            std::size_t length = std::min(n, size_);
            storage_->erase(storage_->begin(), storage_->begin() + length);
            size_ -= length;
            return;
        }
        storage_->erase(storage_->begin(),
                        storage_->begin() + std::min(size(), n));
    }

    storage_type& storage()
    {
        assert(storage_ != nullptr);
        return *storage_;
    }

    void reset() {resetStorage(nullptr);}

    void reset(storage_type& storage, std::size_t maxSize = sizeLimit_)
    {
        resetStorage(&storage, maxSize);
    }

private:
    static constexpr auto noSize_ = std::numeric_limits<std::size_t>::max();

    void resetStorage(storage_type* storage, std::size_t maxSize)
    {
        storage_ = storage;
        size_ = noSize_;
        maxSize_ = maxSize;
    }

    TStorage* storage_ = nullptr;
    std::size_t size_ = noSize_;
    std::size_t maxSize_ = sizeLimit_;
};

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
    using WebsocketPtr    = std::unique_ptr<WebsocketSocket>;

protected:
    // Constructor for client transports
    WebsocketTransport(WebsocketPtr&& ws, TransportInfo info)
        : Base(boost::asio::make_strand(ws->get_executor()),
               makeConnectionInfo(ws->next_layer()),
               info),
        websocket_(std::move(ws))
    {}

    // Constructor for server transports
    WebsocketTransport(TcpSocket& tcp, const std::string& server)
        : Base(boost::asio::make_strand(tcp.get_executor()),
               makeConnectionInfo(tcp, server))
    {}

    void assignWebsocket(WebsocketPtr&& ws, TransportInfo i)
    {
        websocket_ = std::move(ws);
        Base::completeAdmission(i);
    }

private:
    using Base = BasicTransport<WebsocketTransport>;

    // TODO: Consolidate with RawsockTransport
    static ConnectionInfo makeConnectionInfo(const TcpSocket& socket,
                                             const std::string& server = {})
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
            details.emplace("numeric_address", addr.to_v4().to_uint());

        return {std::move(details), oss.str(), server};
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
        return websocket_->next_layer().is_open() && websocket_->is_open();
    }

    void enablePinging()
    {
        std::weak_ptr<Transporting> self = shared_from_this();

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
        websocket_->control_callback();
    }

    void stopTransport()
    {
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

        websocket_->control_callback();

        auto self =
            std::dynamic_pointer_cast<WebsocketTransport>(shared_from_this());
        websocket_->async_close(reason,
                                Closed{std::move(self), std::move(handler)});
    }

    void onWebsocketClosed(std::error_code ec, CloseHandler& handler)
    {
        websocket_->next_layer().close();
        if (handler == nullptr)
            return;
        if (ec)
            return Base::post(std::move(handler), makeUnexpected(ec));
        Base::post(std::move(handler), true);
    }

    void cancelClose()
    {
        websocket_->next_layer().close();
    }

    void failTransport(std::error_code ec)
    {
        if (!socketIsOpen())
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

        rxBuffer_.reset(payload);
        websocket_->async_read(rxBuffer_,
                               Received{std::forward<F>(callback), this});
    }

    template <typename F>
    void onMessageReceived(boost::beast::error_code netEc, F& callback)
    {
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

    WebsocketPtr websocket_;
    DynamicWebsocketBuffer<MessageBuffer> rxBuffer_;

    friend class BasicTransport<WebsocketTransport>;
};

//------------------------------------------------------------------------------
class WebsocketClientTransport : public WebsocketTransport
{
public:
    using Ptr = std::shared_ptr<WebsocketClientTransport>;
    using Settings = WebsocketHost;

    WebsocketClientTransport(WebsocketPtr&& w, const Settings& s,
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
    using SettingsPtr     = std::shared_ptr<WebsocketEndpoint>;
    using WebsocketSocket = boost::beast::websocket::stream<TcpSocket>;
    using WebsocketPtr    = std::unique_ptr<WebsocketSocket>;
    using Handler = AnyCompletionHandler<void (std::error_code, WebsocketPtr,
                                               TransportInfo)>;

    WebsocketAdmitter(TcpSocket&& t, SettingsPtr s, const CodecIdSet& c)
        : tcpSocket_(std::move(t)),
          codecIds_(c),
          settings_(std::move(s))
    {
        std::string agent = settings_->agent();
        if (agent.empty())
            agent = Version::agentString();
        response_.base().set(boost::beast::http::field::server,
                             std::move(agent));
    }

    void admit(bool isShedding, Handler handler)
    {
        isShedding_ = isShedding;
        handler_ = std::move(handler);
        auto self = this->shared_from_this();

        boost::beast::http::async_read(
            tcpSocket_, buffer_, request_,
            [this, self] (const boost::beast::error_code& netEc, std::size_t)
            {
                if (check(netEc))
                    acceptHandshake();
            });
    }

    void cancel()
    {
        if (websocket_)
            websocket_->next_layer().close();
        else
            tcpSocket_.close();
    }

    void timeout(boost::system::error_code ec)
    {
        if (!ec)
            return fail(TransportErrc::timeout);
        if (ec != boost::asio::error::operation_aborted)
            fail(static_cast<std::error_code>(ec));
    }

private:
    using HttpStatus = boost::beast::http::status;

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

    void onTimeout(boost::system::error_code ec)
    {
        if (!ec)
            return fail(TransportErrc::timeout);
        if (ec != boost::asio::error::operation_aborted)
            fail(static_cast<std::error_code>(ec));
    }

    void acceptHandshake()
    {
        // TODO: Multiplex websocket transports with same port but different
        //       request-target URIs.

        // Check that we actually received a websocket upgrade request
        if (!boost::beast::websocket::is_upgrade(request_))
            return fail(boost::beast::websocket::error::no_connection_upgrade);

        // Parse the subprotocol to determine the peer's desired codec
        using boost::beast::http::field;
        auto found = request_.base().find(field::sec_websocket_protocol);
        if (found == request_.base().end())
        {
            return respondThenFail("No subprotocol was requested",
                                   HttpStatus::bad_request,
                                   TransportErrc::noSerializer);
        }
        auto subprotocol = found->value();
        codecId_ = parseSubprotocol(subprotocol);
        if (codecIds_.count(codecId_) == 0)
        {
            return respondThenFail("Requested subprotocol is not supported",
                                   HttpStatus::bad_request,
                                   TransportErrc::badSerializer);
        }

        // Send an error response if the server connection limit
        // has been reached
        if (isShedding_)
        {
            return respondThenFail("Connection limit reached",
                                   HttpStatus::service_unavailable,
                                   TransportErrc::shedded);
        }

        // Transfer the TCP socket to a new websocket stream
        websocket_.reset(
            new WebsocketSocket{std::move(tcpSocket_)});

        // Set the Server and Sec-WebsocketSocket-Protocol fields of
        // the handshake
        std::string agent = settings_->agent();
        if (agent.empty())
            agent = Version::agentString();
        websocket_->set_option(boost::beast::websocket::stream_base::decorator(
            Decorator{std::move(agent), subprotocol}));

        // Complete the handshake
        auto self = shared_from_this();
        websocket_->async_accept(
            request_,
            [this, self](boost::beast::error_code netEc)
            {
                if (check(netEc))
                    complete();
            });
    }

    void respondThenFail(std::string msg, HttpStatus status, TransportErrc errc)
    {
        namespace http = boost::beast::http;
        response_.result(status);
        response_.body() = std::move(msg);
        auto self = shared_from_this();
        http::async_write(
            tcpSocket_, response_,
            [this, self, errc](boost::beast::error_code netEc, std::size_t)
            {
                if (check(netEc))
                    fail(errc);
            });
    }

    void complete()
    {
        if (subprotocolIsText(codecId_))
            websocket_->text(true);
        else
            websocket_->binary(true);

        websocket_->read_message_max(settings_->maxRxLength());

        const TransportInfo i{codecId_, std::numeric_limits<std::size_t>::max(),
                              settings_->maxRxLength()};

        handler_(std::error_code{}, std::move(websocket_), i);
        handler_ = nullptr;
    }

    bool check(boost::system::error_code netEc)
    {
        if (netEc)
            fail(websocketErrorCodeToStandard(netEc));
        return !netEc;
    }

    void fail(std::error_code ec)
    {
        if (!handler_)
            return;
        handler_(ec, nullptr, TransportInfo{});
        handler_ = nullptr;
        if (websocket_)
            websocket_->next_layer().close();
        else
            tcpSocket_.close();
    }

    template <typename TErrc>
    void fail(TErrc errc)
    {
        fail(static_cast<std::error_code>(make_error_code(errc)));
    }

    TcpSocket tcpSocket_;
    CodecIdSet codecIds_;
    SettingsPtr settings_;
    Handler handler_;
    boost::beast::flat_buffer buffer_;
    boost::beast::http::request<boost::beast::http::string_body> request_;
    boost::beast::http::response<boost::beast::http::string_body> response_;
    std::unique_ptr<WebsocketSocket> websocket_; // TODO: Use optional<T>
    int codecId_ = 0;
    bool isShedding_ = false;
};

//------------------------------------------------------------------------------
class WebsocketServerTransport : public WebsocketTransport
{
public:
    using Ptr = std::shared_ptr<WebsocketServerTransport>;
    using Settings = WebsocketEndpoint;
    using SettingsPtr = std::shared_ptr<WebsocketEndpoint>;

    WebsocketServerTransport(TcpSocket&& t, SettingsPtr s, const CodecIdSet& c,
                             const std::string& server, RouterLogger::Ptr l)
        : Base(t, server),
          admitter_(std::make_shared<WebsocketAdmitter>(
              std::move(t), std::move(s), c))
    {}

private:
    using Base = WebsocketTransport;

    void onAdmit(Timeout timeout, AdmitHandler handler) override
    {
        assert((admitter_ != nullptr) && "Admit already performed");

        struct Admitted
        {
            AdmitHandler handler;
            Ptr self;

            void operator()(std::error_code ec, WebsocketPtr ws,
                            TransportInfo ti)
            {
                self->onAdmissionCompletion(ec, ws, ti, handler);
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

    void onAdmissionCompletion(std::error_code ec, WebsocketPtr& socket,
                               const TransportInfo& info, AdmitHandler& handler)
    {
        if (ec)
        {
            Base::shutdown();
            Base::post(std::move(handler), makeUnexpected(ec));
        }
        else
        {
            Base::assignWebsocket(std::move(socket), info);
            Base::post(std::move(handler), info.codecId());
        }

        admitter_.reset();
    }

    WebsocketAdmitter::Ptr admitter_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WEBSOCKETTRANSPORT_HPP
