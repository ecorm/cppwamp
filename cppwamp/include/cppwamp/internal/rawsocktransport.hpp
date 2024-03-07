/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_RAWSOCKTRANSPORT_HPP
#define CPPWAMP_INTERNAL_RAWSOCKTRANSPORT_HPP

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <limits>
#include <utility>
#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include "../anyhandler.hpp"
#include "../codec.hpp"
#include "../errorcodes.hpp"
#include "../queueingclienttransport.hpp"
#include "../queueingservertransport.hpp"
#include "../traits.hpp"
#include "rawsockheader.hpp"
#include "rawsockhandshake.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
inline std::error_code rawsockErrorCodeToStandard(
    boost::system::error_code netEc)
{
    bool disconnected =
        netEc == boost::asio::error::broken_pipe ||
        netEc == boost::asio::error::connection_reset;
    if (disconnected)
        return make_error_code(TransportErrc::disconnected);
    if (netEc == boost::asio::error::eof)
        return make_error_code(TransportErrc::ended);
    if (netEc == boost::asio::error::operation_aborted)
        return make_error_code(TransportErrc::aborted);
    return static_cast<std::error_code>(netEc);
}

//------------------------------------------------------------------------------
template <typename TTraits>
struct RawsockUnderlyingSocket
{
    using Socket           = typename TTraits::Socket;
    using UnderlyingSocket = typename TTraits::UnderlyingSocket;

    static UnderlyingSocket& get(Socket& socket)
    {
        return underlyingSocket(IsTls{}, socket);
    }

    static const UnderlyingSocket& get(const Socket& socket)
    {
        return underlyingSocket(IsTls{}, socket);
    }

private:
    using IsTls = typename TTraits::IsTls;

    template <typename S>
    static UnderlyingSocket& underlyingSocket(FalseType, S& socket)
    {
        return socket;
    }

    template <typename S>
    static const UnderlyingSocket& underlyingSocket(FalseType, const S& socket)
    {
        return socket;
    }

    template <typename S>
    static UnderlyingSocket& underlyingSocket(TrueType, S& socket)
    {
        return socket.next_layer();
    }

    template <typename S>
    static const UnderlyingSocket& underlyingSocket(TrueType, const S& socket)
    {
        return socket.next_layer();
    }
};

//------------------------------------------------------------------------------
template <typename TTraits>
class RawsockStream
{
public:
    using Traits           = TTraits;
    using Socket           = typename Traits::Socket;
    using ShutdownHandler  = AnyCompletionHandler<void (std::error_code)>;
    using ReadHandler      =
        AnyCompletionHandler<void (std::error_code, std::size_t, bool)>;
    using HeartbeatHandler =
        std::function<void (TransportFrameKind, const uint8_t* data,
                            std::size_t size)>;

    static ConnectionInfo makeConnectionInfo(const Socket& s)
    {
        return Traits::connectionInfo(s);
    }

    explicit RawsockStream(AnyIoExecutor e) : socket_(std::move(e)) {}

    template <typename S>
    explicit RawsockStream(Socket&& socket, const std::shared_ptr<S>& settings)
        : socket_(std::move(socket)),
          wampPayloadLimit_(settings->limits().wampReadMsgSize()),
          heartbeatPayloadLimit_(settings->limits().heartbeatSize())
    {}

    AnyIoExecutor executor() {return socket_.get_executor();}

    bool isOpen() const {return underlyingSocket().is_open();}

    void observeHeartbeats(HeartbeatHandler handler)
    {
        heartbeatHandler_ = std::move(handler);
    }

    void unobserveHeartbeats() {heartbeatHandler_ = nullptr;}

    template <typename F>
    void ping(const uint8_t* data, std::size_t size, F&& callback)
    {
        sendHeartbeatFrame(TransportFrameKind::ping, data, size,
                           std::forward<F>(callback));
    }

    template <typename F>
    void pong(const uint8_t* data, std::size_t size, F&& callback)
    {
        sendHeartbeatFrame(TransportFrameKind::pong, data, size,
                           std::forward<F>(callback));
    }

    template <typename F>
    void writeSome(const uint8_t* data, std::size_t size, F&& callback)
    {
        if (!headerSent_)
            return writeWampHeader(data, size, std::forward<F>(callback));
        writeMoreWampPayload(data, size, std::forward<F>(callback));
    }

    void awaitRead(MessageBuffer&, ReadHandler handler)
    {
        readHandler_ = std::move(handler);
        payloadReadStarted_ = false;
        doAwaitRead();
    }

    void readSome(MessageBuffer& buffer, ReadHandler handler)
    {
        readHandler_ = std::move(handler);

        if (payloadReadStarted_)
            return readMoreWampPayload(buffer);

        payloadReadStarted_ = true;
        readWampPayload(buffer);
    }

    // Caller must keep this object alive during flush by binding
    // shared_from_this() to the given handler.
    void shutdown(std::error_code /*reason*/, ShutdownHandler handler)
    {
        doShutdown(IsTls{}, std::move(handler));
    }

    void close() {underlyingSocket().close();}

private:
    using Header           = uint32_t;
    using GatherBufs       = std::array<boost::asio::const_buffer, 2>;
    using IsTls            = typename Traits::IsTls;
    using UnderlyingSocket = typename Traits::UnderlyingSocket;

    static Header computeHeader(TransportFrameKind kind, std::size_t size)
    {
        return RawsockHeader().setFrameKind(kind).setLength(size).toBigEndian();
    }

    template <typename F>
    void sendHeartbeatFrame(TransportFrameKind kind, const uint8_t* data,
                            std::size_t size, F&& callback)
    {
        struct Written
        {
            Decay<F> callback;

            void operator()(boost::system::error_code netEc, std::size_t)
            {
                callback(rawsockErrorCodeToStandard(netEc));
            }
        };

        txHeader_ = computeHeader(kind, size);
        auto buffers = GatherBufs{{ {&txHeader_, sizeof(txHeader_)},
                                   {data, size} }};
        boost::asio::async_write(socket_, buffers,
                                 Written{std::move(callback)});
    }

    template <typename F>
    void writeWampHeader(const uint8_t* payloadData, std::size_t payloadSize,
                         F&& callback)
    {
        struct Written
        {
            Decay<F> callback;
            const uint8_t* data;
            std::size_t size;
            RawsockStream* self;

            void operator()(boost::system::error_code netEc, std::size_t)
            {
                if (netEc)
                {
                    callback(rawsockErrorCodeToStandard(netEc), 0);
                    return;
                }

                if (size == 0)
                {
                    callback(std::error_code{}, 0);
                    return;
                }

                self->headerSent_ = true;
                self->writeMoreWampPayload(data, size, callback);
            }
        };

        txHeader_ = computeHeader(TransportFrameKind::wamp, payloadSize);
        boost::asio::async_write(
            socket_,
            boost::asio::const_buffer{&txHeader_, sizeof(txHeader_)},
            Written{std::move(callback), payloadData, payloadSize, this});
    }

    template <typename F>
    void writeMoreWampPayload(const uint8_t* data, std::size_t size,
                              F&& callback)
    {
        struct Written
        {
            Decay<F> callback;
            std::size_t size;
            RawsockStream* self;

            void operator()(boost::system::error_code netEc, std::size_t n)
            {
                if (n >= size)
                    self->headerSent_ = false;
                callback(rawsockErrorCodeToStandard(netEc), n);
            }
        };

        assert(headerSent_);
        socket_.async_write_some(
            boost::asio::const_buffer{data, size},
            Written{std::move(callback), size, this});
    }

    void doAwaitRead()
    {
        // Wait until the header bytes of WAMP frame read, so that the read
        // timeout logic in QueueingTransport only applies to WAMP frames.

        rxHeader_ = 0;
        isReading_ = true;
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(&rxHeader_, sizeof(rxHeader_)),
            [this](boost::system::error_code netEc, std::size_t n)
            {
                onHeaderRead(netEc);
            });
    }

    void onHeaderRead(boost::system::error_code netEc)
    {
        if (!checkRead(netEc))
            return;

        const auto header = RawsockHeader::fromBigEndian(rxHeader_);
        if (!header.frameKindIsValid())
            return failRead(TransportErrc::badCommand);

        auto kind = header.frameKind();
        auto length = header.length();
        auto limit = kind == TransportFrameKind::wamp ? wampPayloadLimit_
                                                      : heartbeatPayloadLimit_;
        if (limit != 0 && length > limit)
            return failRead(TransportErrc::inboundTooLong);

        if (kind != TransportFrameKind::wamp)
            return readHeartbeatPayload(kind, length);

        if (length == 0)
            return completeRead(std::error_code{}, 0, true);

        wampRxBytesRemaining_ = length;
        completeRead(std::error_code{}, 0, false);
    }

    void readWampPayload(MessageBuffer& payload)
    {
        assert(wampRxBytesRemaining_ != 0);

        bool tooLong = wampPayloadLimit_ != 0 &&
                       wampRxBytesRemaining_ > wampPayloadLimit_;
        if (tooLong)
            return failRead(TransportErrc::inboundTooLong);

        try
        {
            payload.resize(wampRxBytesRemaining_);
        }
        catch (const std::bad_alloc&)
        {
            return failRead(std::errc::not_enough_memory);
        }
        catch (const std::length_error&)
        {
            return failRead(std::errc::not_enough_memory);
        }

        readMoreWampPayload(payload);
    }

    void readMoreWampPayload(MessageBuffer& payload)
    {
        assert(payload.size() >= wampRxBytesRemaining_);
        auto bytesReadSoFar = payload.size() - wampRxBytesRemaining_;
        auto ptr = payload.data() + bytesReadSoFar;
        isReading_ = true;
        socket_.async_read_some(
            boost::asio::buffer(ptr, wampRxBytesRemaining_),
            [this](boost::system::error_code netEc, std::size_t n)
            {
                onWampPayloadRead(netEc, n);
            });
    }

    void onWampPayloadRead(boost::system::error_code netEc,
                           std::size_t bytesRead)
    {
        if (!checkShutdown(netEc))
            return;

        assert(bytesRead <= wampRxBytesRemaining_);
        wampRxBytesRemaining_ -= bytesRead;
        bool done = wampRxBytesRemaining_ == 0;
        completeRead(rawsockErrorCodeToStandard(netEc), bytesRead, done);
    }

    void readHeartbeatPayload(TransportFrameKind kind, size_t length)
    {
        if (heartbeatPayloadLimit_ != 0 && length > heartbeatPayloadLimit_)
            return failRead(TransportErrc::inboundTooLong);

        try
        {
            buffer_.resize(length);
        }
        catch (const std::bad_alloc&)
        {
            return failRead(std::errc::not_enough_memory);
        }
        catch (const std::length_error&)
        {
            return failRead(std::errc::not_enough_memory);
        }

        if (length == 0)
            onHeartbeatPayloadRead({}, kind);

        isReading_ = true;
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(buffer_.data(), length),
            [this, kind](boost::system::error_code netEc, std::size_t)
            {
                onHeartbeatPayloadRead(netEc, kind);
            });
    }

    void onHeartbeatPayloadRead(boost::system::error_code netEc,
                                TransportFrameKind kind)
    {
        if (!checkRead(netEc))
            return;

        if (heartbeatHandler_ != nullptr)
            post(heartbeatHandler_, kind, buffer_.data(), buffer_.size());

        doAwaitRead();
    }

    bool checkRead(boost::system::error_code netEc)
    {
        bool canContinueReading = checkShutdown(netEc);

        if (netEc == boost::asio::error::eof)
        {
            completeRead(make_error_code(TransportErrc::ended), 0, true);
            return false;
        }

        if (!canContinueReading)
            return false;

        if (netEc)
            completeRead(rawsockErrorCodeToStandard(netEc), 0, false);

        return !netEc;
    }

    bool checkShutdown(boost::system::error_code netEc)
    {
        isReading_ = false;

        if (shutdownHandler_ == nullptr)
            return true;

        if (!netEc)
        {
            flush();
            return false;
        }

        close();
        completeShutdown(netEc);
        return false;
    }

    void completeRead(std::error_code ec, std::size_t length, bool done)
    {
        if (!readHandler_)
            return;
        auto handler = std::move(readHandler_);
        readHandler_ = nullptr;
        handler(ec, length, done);
    }

    void completeShutdown(boost::system::error_code netEc)
    {
        if (!shutdownHandler_)
            return;
        auto handler = std::move(shutdownHandler_);
        shutdownHandler_ = nullptr;
        if (netEc == boost::asio::error::eof)
            handler(std::error_code{});
        else
            handler(rawsockErrorCodeToStandard(netEc));
    }

    template <typename TErrc>
    void failRead(TErrc errc)
    {
        if (readHandler_ != nullptr)
            readHandler_(make_error_code(errc), 0, false);
    }

    void flush()
    {
        buffer_.resize(+flushReadSize_);
        underlyingSocket().async_read_some(
            boost::asio::buffer(buffer_.data(), +flushReadSize_),
            [this](boost::system::error_code netEc, size_t n)
            {
                if (!netEc)
                    return flush();
                onFlushComplete(netEc);
            });
    }

    void onFlushComplete(boost::system::error_code netEc)
    {
        close();
        completeShutdown(netEc);

        if (netEc == boost::asio::error::eof)
            completeRead(make_error_code(TransportErrc::ended), 0, true);
        else
            completeRead(make_error_code(TransportErrc::aborted), 0, true);
    }

    // Non-TLS overload
    template <typename F>
    void doShutdown(FalseType, F&& handler)
    {
        boost::system::error_code netEc;
        socket_.shutdown(Socket::shutdown_send, netEc);
        auto ec = static_cast<std::error_code>(netEc);
        if (ec)
            return post(std::forward<F>(handler), ec);

        shutdownHandler_ = std::forward<F>(handler);
        if (!isReading_)
            flush();
    }

    // TLS overload
    template <typename F>
    void doShutdown(TrueType, F&& handler)
    {
        shutdownHandler_ = std::forward<F>(handler);
        socket_.async_shutdown(
            [this](boost::system::error_code netEc)
            {
                if (!shutdownHandler_)
                    return;
                auto handler = std::move(shutdownHandler_);
                shutdownHandler_ = nullptr;
                if (netEc != boost::asio::error::eof)
                {
                    close();
                    handler(rawsockErrorCodeToStandard(netEc));
                }
                else
                {
                    shutdownUnderlying();
                }
            });
    }

    void shutdownUnderlying()
    {
        boost::system::error_code netEc;
        underlyingSocket().shutdown(UnderlyingSocket::shutdown_send, netEc);
        auto ec = static_cast<std::error_code>(netEc);
        if (ec)
        {
            auto handler = std::move(shutdownHandler_);
            shutdownHandler_ = nullptr;
            handler(ec);
            return;
        }

        if (!isReading_)
            flush();
    }

    UnderlyingSocket& underlyingSocket()
    {
        return RawsockUnderlyingSocket<Traits>::get(socket_);
    }

    const UnderlyingSocket& underlyingSocket() const
    {
        return RawsockUnderlyingSocket<Traits>::get(socket_);
    }

    template <typename F, typename... Ts>
    void post(F&& handler, Ts&&... args)
    {
        postAny(socket_.get_executor(), std::forward<F>(handler),
                std::forward<Ts>(args)...);
    }

    static constexpr auto unlimited_ = std::numeric_limits<std::size_t>::max();
    static constexpr std::size_t flushReadSize_ = 1536;

    Socket socket_;
    MessageBuffer buffer_;
    HeartbeatHandler heartbeatHandler_;
    ReadHandler readHandler_;
    ShutdownHandler shutdownHandler_;
    std::size_t wampPayloadLimit_ = unlimited_;
    std::size_t heartbeatPayloadLimit_ = unlimited_;
    std::size_t wampRxBytesRemaining_ = 0;
    Header rxHeader_ = 0;
    Header txHeader_ = 0;
    bool headerSent_ = false;
    bool payloadReadStarted_ = false;
    bool isReading_ = false;
};

//------------------------------------------------------------------------------
template <typename TTraits>
class RawsockAdmitter
    : public std::enable_shared_from_this<RawsockAdmitter<TTraits>>
{
public:
    using Traits          = TTraits;
    using Ptr             = std::shared_ptr<RawsockAdmitter>;
    using Stream          = RawsockStream<Traits>;
    using ListenerSocket  = typename Traits::Socket;
    using Socket          = ListenerSocket;
    using Settings        = typename Traits::ServerSettings;
    using SettingsPtr     = std::shared_ptr<Settings>;
    using Handler         = AnyCompletionHandler<void (AdmitResult)>;
    using ShutdownHandler = AnyCompletionHandler<void (std::error_code)>;

    explicit RawsockAdmitter(ListenerSocket&& s, SettingsPtr p,
                             const CodecIdSet& c)
        : socket_(std::move(s)),
          codecIds_(c),
          settings_(p)
    {}

    void admit(bool isShedding, Handler handler)
    {
        doAdmit(IsTls{}, isShedding, std::move(handler));
    }

    void shutdown(std::error_code reason, ShutdownHandler handler)
    {
        doShutdown(IsTls{}, reason, std::move(handler));
    }

    void close() {underlyingSocket().close();}

    const TransportInfo& transportInfo() const {return transportInfo_;}

    std::string releaseTargetPath() {return std::string{};}

    Socket&& releaseSocket() {return std::move(socket_);}

private:
    using Handshake        = internal::RawsockHandshake;
    using IsTls            = typename Traits::IsTls;
    using UnderlyingSocket = typename Traits::UnderlyingSocket;

    // Non-TLS overload
    template <typename F>
    void doAdmit(FalseType, bool isShedding, F&& handler)
    {
        isShedding_ = isShedding;
        handler_ = std::forward<F>(handler);
        receiveRawsocketHandshake();
    }

    // TLS overload
    template <typename F>
    void doAdmit(TrueType, bool isShedding, F&& handler)
    {
        isShedding_ = isShedding;
        handler_ = std::forward<F>(handler);

        auto self = this->shared_from_this();
        socket_.async_handshake(
            Socket::server,
            [this, self](boost::system::error_code netEc)
            {
                if (check(netEc, "SSL/TLS handshake"))
                    receiveRawsocketHandshake();
            });
    }

    void receiveRawsocketHandshake()
    {
        auto self = this->shared_from_this();

        boost::asio::async_read(
            socket_,
            boost::asio::buffer(&handshake_, sizeof(handshake_)),
            [this, self](boost::system::error_code netEc, size_t)
            {
                isReading_ = false;
                if (checkShutdown(netEc) && check(netEc, "socket read"))
                {
                    onRawsocketHandshakeReceived(
                        Handshake::fromBigEndian(handshake_));
                }
            });
    }

    // Non-TLS overload
    template <typename F>
    void doShutdown(FalseType, std::error_code reason, F&& handler)
    {
        if (handler_)
        {
            post(std::move(handler_), AdmitResult::cancelled(reason));
            handler_ = nullptr;
        }

        boost::system::error_code netEc;
        socket_.shutdown(Socket::shutdown_send, netEc);
        auto ec = static_cast<std::error_code>(netEc);
        if (ec)
        {
            post(std::forward<F>(handler), ec);
            return;
        }

        shutdownHandler_ = std::forward<F>(handler);
        if (!isReading_)
            flush();
    }

    // TLS overload
    template <typename F>
    void doShutdown(TrueType, std::error_code reason, F&& handler)
    {
        if (handler_)
        {
            post(std::move(handler_), AdmitResult::cancelled(reason));
            handler_ = nullptr;
        }

        socket_.async_shutdown(
            [this](boost::system::error_code netEc)
            {
                if (!handler_)
                    return;
                auto handler = std::move(shutdownHandler_);
                handler_ = nullptr;
                if (netEc != boost::asio::error::eof)
                {
                    close();
                    handler(rawsockErrorCodeToStandard(netEc));
                }
                else
                {
                    shutdownUnderlying();
                }
            });
    }

    void shutdownUnderlying()
    {
        boost::system::error_code netEc;
        underlyingSocket().shutdown(UnderlyingSocket::shutdown_send, netEc);
        auto ec = static_cast<std::error_code>(netEc);
        if (ec)
        {
            auto handler = std::move(shutdownHandler_);
            shutdownHandler_ = nullptr;
            handler(ec);
            return;
        }

        if (!isReading_)
            flush();
    }

    bool checkShutdown(boost::system::error_code netEc)
    {
        if (shutdownHandler_ == nullptr)
            return true;

        if (!netEc)
        {
            flush();
            return false;
        }

        close();
        completeShutdown(netEc);

        if (handler_)
        {
            handler_(AdmitResult::failed(TransportErrc::aborted,
                                         "socket read"));
            handler_ = nullptr;
        }

        return false;
    }

    void completeShutdown(boost::system::error_code netEc)
    {
        auto handler = std::move(shutdownHandler_);
        shutdownHandler_ = nullptr;
        if (netEc == boost::asio::error::eof)
            handler(std::error_code{});
        else
            handler(rawsockErrorCodeToStandard(netEc));
    }

    void onRawsocketHandshakeReceived(Handshake hs)
    {
        auto peerCodec = hs.codecId();

        if (isShedding_)
        {
            sendRefusal();
        }
        else if (!hs.hasMagicOctet())
        {
            reject(TransportErrc::badHandshake);
        }
        else if (hs.reserved() != 0)
        {
            sendHandshake(Handshake::eReservedBitsUsed());
        }
        else if (codecIds_.count(peerCodec) != 0)
        {
            peerSizeLimit_ = hs.sizeLimit();
            const auto rxLimit = settings_->limits().wampReadMsgSize();
            sendHandshake(Handshake().setCodecId(peerCodec)
                                     .setSizeLimit(rxLimit));
        }
        else
        {
            sendHandshake(Handshake::eUnsupportedFormat());
        }
    }

    void sendRefusal()
    {
        handshake_ = Handshake::eMaxConnections().toBigEndian();
        auto self = this->shared_from_this();
        boost::asio::async_write(
            socket_,
            boost::asio::buffer(&handshake_, sizeof(handshake_)),
            [this, self](boost::system::error_code ec, size_t)
            {
                if (check(ec, "handshake rejected write"))
                    finish(AdmitResult::shedded());
            });
    }

    void sendHandshake(Handshake hs)
    {
        handshake_ = hs.toBigEndian();
        auto self = this->shared_from_this();
        boost::asio::async_write(
            socket_,
            boost::asio::buffer(&handshake_, sizeof(handshake_)),
            [this, self, hs](boost::system::error_code ec, size_t)
            {
                if (check(ec, "handshake accepted write"))
                    onHandshakeSent(hs, ec);
            });
    }

    void onHandshakeSent(Handshake hs, boost::system::error_code ec)
    {
        if (!hs.hasError())
            complete(hs);
        else
            reject(hs.errorCode());
    }

    void complete(Handshake hs)
    {
        // Clamp send limit to smallest between settings limit and peer limit
        const auto codecId = hs.codecId();
        auto txLimit = settings_->limits().wampWriteMsgSize();
        txLimit = txLimit < peerSizeLimit_ ? txLimit : peerSizeLimit_;
        const auto rxLimit = settings_->limits().wampReadMsgSize();
        transportInfo_ = TransportInfo{codecId, txLimit, rxLimit};

        finish(AdmitResult::wamp(codecId));
    }

    bool check(boost::system::error_code netEc, const char* operation)
    {
        if (!netEc)
            return true;

        close();

        auto ec = rawsockErrorCodeToStandard(netEc);
        if (ec == TransportErrc::disconnected)
            finish(AdmitResult::disconnected());
        else
            finish(AdmitResult::failed(ec, operation));

        return !netEc;
    }

    template <typename TErrc>
    void reject(TErrc errc)
    {
        finish(AdmitResult::rejected(errc));
    }

    void finish(AdmitResult result)
    {
        if (handler_)
            handler_(std::move(result));
        handler_ = nullptr;
    }

    void flush()
    {
        auto self = this->shared_from_this();
        buffer_.resize(+flushReadSize_);
        underlyingSocket().async_read_some(
            boost::asio::buffer(buffer_.data(), +flushReadSize_),
            [this, self](boost::system::error_code netEc, size_t n)
            {
                if (!netEc)
                    return flush();
                close();
                completeShutdown(netEc);
            });
    }

    UnderlyingSocket& underlyingSocket()
    {
        return RawsockUnderlyingSocket<Traits>::get(socket_);
    }

    const UnderlyingSocket& underlyingSocket() const
    {
        return RawsockUnderlyingSocket<Traits>::get(socket_);
    }

    template <typename F, typename... Ts>
    void post(F&& handler, Ts&&... args)
    {
        postAny(socket_.get_executor(), std::forward<F>(handler),
                std::forward<Ts>(args)...);
    }

    static constexpr std::size_t flushReadSize_ = 1536;

    Socket socket_;
    CodecIdSet codecIds_;
    TransportInfo transportInfo_;
    std::vector<uint8_t> buffer_;
    Handler handler_;
    ShutdownHandler shutdownHandler_;
    SettingsPtr settings_;
    std::size_t peerSizeLimit_ = 0;
    uint32_t handshake_ = 0;
    bool isShedding_ = false;
    bool isReading_ = false;
};

//------------------------------------------------------------------------------
template <typename TTraits>
using RawsockClientTransport =
    QueueingClientTransport<typename TTraits::ClientSettings,
                            RawsockStream<TTraits>,
                            typename TTraits::SslContextType>;

//------------------------------------------------------------------------------
template <typename TTraits>
using RawsockServerTransport =
    QueueingServerTransport<typename TTraits::ServerSettings,
                            RawsockAdmitter<TTraits>,
                            typename TTraits::SslContextType>;

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_RAWSOCKTRANSPORT_HPP
