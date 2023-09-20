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
#include "../codec.hpp"
#include "../traits.hpp"
#include "../version.hpp"
#include "../transports/websocketprotocol.hpp"
#include "basictransport.hpp"

namespace wamp
{

namespace internal
{

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
class WebsocketTransport : public BasicTransport<WebsocketTransport>
{
public:
    using Ptr             = std::shared_ptr<WebsocketTransport>;
    using TcpSocket       = boost::asio::ip::tcp::socket;
    using WebsocketSocket = boost::beast::websocket::stream<TcpSocket>;
    using WebsocketPtr    = std::unique_ptr<WebsocketSocket>;

protected:
    static std::error_code netErrorCodeToStandard(
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

    // Constructor for client transports
    WebsocketTransport(WebsocketPtr&& ws, TransportInfo info)
        : Base(boost::asio::make_strand(ws->get_executor()),
               makeConnectionInfo(ws->next_layer()),
               info),
          websocket_(std::move(ws))
    {}

    // Constructor for server transports
    WebsocketTransport(TcpSocket& tcp)
        : Base(boost::asio::make_strand(tcp.get_executor()),
               makeConnectionInfo(tcp))
    {}

    void assignWebsocket(WebsocketPtr&& ws, TransportInfo i)
    {
        websocket_ = std::move(ws);
        Base::completeAccept(i);
    }

private:
    using Base = BasicTransport<WebsocketTransport>;

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

    bool socketIsOpen() const {return websocket_ != nullptr;}

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
        if (websocket_)
            websocket_->control_callback();
    }

    void stopTransport()
    {
        if (websocket_)
            websocket_->next_layer().close();
    }

    void closeTransport(CloseHandler handler)
    {
        doClose(boost::beast::websocket::normal, std::move(handler));
    }

    void doClose(boost::beast::websocket::close_code closeCode,
                 CloseHandler&& handler = nullptr)
    {
        if (websocket_)
            closeWebsocket(boost::beast::websocket::normal, std::move(handler));
        if (!websocket_ && (handler != nullptr))
            Base::post(std::move(handler), true);
    }

    void closeWebsocket(boost::beast::websocket::close_code reason,
                        CloseHandler&& handler = nullptr)
    {
        if (!websocket_)
            return;
        websocket_->control_callback();

        struct Closed
        {
            Ptr self;
            CloseHandler handler;

            void operator()(boost::beast::error_code netEc)
            {
                auto ec = static_cast<std::error_code>(netEc);
                self->onWebsocketClosed(ec, std::move(handler));
            }
        };

        auto self =
            std::dynamic_pointer_cast<WebsocketTransport>(shared_from_this());
        websocket_->async_close(reason,
                                Closed{std::move(self), std::move(handler)});
    }

    void onWebsocketClosed(std::error_code ec, CloseHandler&& handler)
    {
        websocket_->next_layer().close();
        if ((handler == nullptr))
            return;
        if (ec)
            return Base::post(std::move(handler), makeUnexpected(ec));
        Base::post(std::move(handler), true);
    }

    void cancelClose()
    {
        if (websocket_)
            websocket_->next_layer().close();
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
                callback(netErrorCodeToStandard(netEc));
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
    void transmitMessage(const MessageBuffer& payload, F&& callback)
    {
        struct Written
        {
            Decay<F> callback;

            void operator()(boost::beast::error_code netEc, size_t)
            {
                callback(netErrorCodeToStandard(netEc));
            }
        };

        auto buffer = boost::asio::buffer(&payload.front(), payload.size());
        websocket_->async_write(buffer, Written{std::forward<F>(callback)});
    }

    template <typename F>
    void receiveMessage(MessageBuffer& payload, F&& callback)
    {
        struct Received
        {
            Decay<F> callback;
            WebsocketTransport* self;

            void operator()(boost::beast::error_code netEc, std::size_t size)
            {
                self->onMessageReceived(netEc, size, callback);
            }
        };

        // websocket::stream::async_read only takes lvalues
        // https://github.com/boostorg/beast/issues/1112
        rxBuffer_.reset(payload);
        Received received{std::forward<F>(callback), this};
        websocket_->async_read(rxBuffer_, std::move(received));
    }

    template <typename F>
    void onMessageReceived(boost::beast::error_code netEc, std::size_t size,
                           F& callback)
    {
        std::error_code ec = netErrorCodeToStandard(netEc);

        if (netEc == boost::beast::websocket::error::closed)
            ec = interpretCloseReason(websocket_->reason());

        if (!ec)
        {
            if (websocket_->text() && websocket_->got_binary())
                ec = make_error_code(TransportErrc::expectedText);
            if (websocket_->binary() && websocket_->got_text())
                ec = make_error_code(TransportErrc::expectedBinary);
        }

        callback(ec);
    }

    void failTransport(std::error_code ec)
    {
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

    static Ptr create(WebsocketPtr&& w, const Settings& s, TransportInfo i)
    {
        return Ptr(new WebsocketClientTransport(std::move(w), s, i));
    }

private:
    using Base = WebsocketTransport;

    WebsocketClientTransport(WebsocketPtr&& w, const Settings& s,
                             TransportInfo info)
        : Base(std::move(w), info)
    {
        Base::setAbortTimeout(s.abortTimeout());
    }
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
    using HttpStatus = boost::beast::http::status;

    // This data is only used once for accepting connections.
    struct Data
    {
        Data(TcpSocket&& t, const Settings& s, const CodecIdSet& c)
            : tcpSocket(std::move(t)),
              codecIds(c),
              settings(s)
        {
            std::string agent = s.agent();
            if (agent.empty())
                agent = Version::agentString();
            response.base().set(boost::beast::http::field::server,
                                std::move(agent));
        }

        TcpSocket tcpSocket;
        CodecIdSet codecIds;
        WebsocketEndpoint settings;
        AcceptHandler handler;
        boost::beast::flat_buffer buffer;
        boost::beast::http::request<boost::beast::http::string_body> request;
        boost::beast::http::response<boost::beast::http::string_body> response;
        std::unique_ptr<WebsocketSocket> websocket; // TODO: Use optional<T>
        int codecId = 0;
        bool isRefusing = false;
    };

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

    WebsocketServerTransport(TcpSocket&& t, const Settings& s,
                             const CodecIdSet& c)
        : Base(t),
          data_(new Data(std::move(t), s, c))
    {}

    void onAccept(Timeout timeout, AcceptHandler handler) override
    {
        assert((data_ != nullptr) && "Accept already performed");

        data_->handler = std::move(handler);
        auto self = this->shared_from_this();

        if (timeoutIsDefinite(timeout))
        {
            Base::timeoutAfter(
                timeout,
                [this, self](boost::system::error_code ec) {onTimeout(ec);});
        }

        boost::beast::http::async_read(
            data_->tcpSocket, data_->buffer, data_->request,
            [this, self] (const boost::beast::error_code& netEc, std::size_t)
            {
                if (check(netEc))
                    acceptHandshake();
            });
    }

    void onCancelHandshake() override
    {
        if (data_)
        {
            if (data_->websocket)
                data_->websocket->next_layer().close();
            else
                data_->tcpSocket.close();
        }
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

        if (!data_)
            return;

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
                                   HttpStatus::bad_request,
                                   TransportErrc::noSerializer);
        }
        auto subprotocol = found->value();
        data_->codecId = parseSubprotocol(subprotocol);
        if (data_->codecIds.count(data_->codecId) == 0)
        {
            return respondThenFail("Requested subprotocol is not supported",
                                   HttpStatus::bad_request,
                                   TransportErrc::badSerializer);
        }

        // Send an error response if the server connection limit
        // has been reached
        if (Base::state() == TransportState::shedding)
        {
            return respondThenFail("Connection limit reached",
                                   HttpStatus::service_unavailable,
                                   TransportErrc::shedded);
        }

        // Transfer the TCP socket to a new websocket stream
        data_->websocket.reset(
            new WebsocketSocket{std::move(data_->tcpSocket)});
        auto& ws = *(data_->websocket);

        // Set the Server and Sec-WebsocketSocket-Protocol fields of
        // the handshake
        std::string agent = data_->settings.agent();
        if (agent.empty())
            agent = Version::agentString();
        ws.set_option(boost::beast::websocket::stream_base::decorator(
            Decorator{std::move(agent), subprotocol}));

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

    void respondThenFail(std::string msg, HttpStatus status, TransportErrc errc)
    {
        namespace http = boost::beast::http;
        data_->response.result(status);
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
        if (!data_)
            return;

        if (subprotocolIsText(data_->codecId))
            data_->websocket->text(true);
        else
            data_->websocket->binary(true);

        data_->websocket->read_message_max(data_->settings.maxRxLength());

        const TransportInfo i{data_->codecId,
                              std::numeric_limits<std::size_t>::max(),
                              data_->settings.maxRxLength()};

        Base::assignWebsocket(std::move(data_->websocket), i);
        Base::post(std::move(data_->handler), data_->codecId);
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
        if (!data_)
            return;
        Base::post(std::move(data_->handler), makeUnexpected(ec));
        shutdown();
    }

    template <typename TErrc>
    void fail(TErrc errc)
    {
        fail(static_cast<std::error_code>(make_error_code(errc)));
    }

    void shutdown()
    {
        if (data_->websocket)
            data_->websocket->next_layer().close();
        else
            data_->tcpSocket.close();
        data_.reset();
        Base::shutdown();
    }

    std::unique_ptr<Data> data_; // Only used once for accepting connection.
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WEBSOCKETTRANSPORT_HPP
