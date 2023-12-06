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
class RawsockStream
{
public:
    using Traits      = TTraits;
    using NetProtocol = typename Traits::NetProtocol;
    using Socket      = typename NetProtocol::socket;
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
          wampPayloadLimit_(settings->limits().readMsgSize()),
          heartbeatPayloadLimit_(settings->limits().heartbeatSize())
    {}

    AnyIoExecutor executor() {return socket_.get_executor();}

    bool isOpen() const {return socket_.is_open();}

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

    template <typename F>
    void awaitRead(MessageBuffer&, F&& callback)
    {
        payloadReadStarted_ = false;
        doAwaitRead(std::forward<F>(callback));
    }

    template <typename F>
    void readSome(MessageBuffer& buffer, F&& callback)
    {
        if (payloadReadStarted_)
            return readMoreWampPayload(buffer, std::forward<F>(callback));

        payloadReadStarted_ = true;
        readWampPayload(buffer, std::forward<F>(callback));
    }

    // TODO: Take care of flushing
    template <typename F>
    void shutdown(std::error_code reason, F&& callback)
    {
        boost::system::error_code netEc;
        socket_.shutdown(Socket::shutdown_send, netEc);
        auto ec = static_cast<std::error_code>(netEc);
        postAny(socket_.get_executor(), std::forward<F>(callback), ec, true);
    }

    void close() {socket_.close();}

private:
    using Header     = uint32_t;
    using GatherBufs = std::array<boost::asio::const_buffer, 2>;

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

    template <typename F>
    void doAwaitRead(F&& callback)
    {
        // Wait until the header bytes of WAMP frame read, so that the read
        // timeout logic in QueueingTransport only applies to WAMP frames.

        struct Received
        {
            Decay<F> callback;
            RawsockStream* self;

            void operator()(boost::system::error_code netEc, std::size_t n)
            {
                self->onHeaderRead(netEc, callback);
            }
        };

        rxHeader_ = 0;
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(&rxHeader_, sizeof(rxHeader_)),
            Received{std::move(callback), this});
    }

    template <typename F>
    void onHeaderRead(boost::system::error_code netEc, F& callback)
    {
        if (!checkRead(netEc, callback))
            return;

        const auto header = RawsockHeader::fromBigEndian(rxHeader_);
        if (!header.frameKindIsValid())
            return failRead(TransportErrc::badCommand, callback);

        auto kind = header.frameKind();
        auto length = header.length();
        auto limit = kind == TransportFrameKind::wamp ? wampPayloadLimit_
                                                      : heartbeatPayloadLimit_;
        if (limit != 0 && length > limit)
            return failRead(TransportErrc::inboundTooLong, callback);

        if (kind != TransportFrameKind::wamp)
            return readHeartbeatPayload(kind, length, callback);

        if (length == 0)
        {
            callback(std::error_code{}, 0, true);
            return;
        }

        wampRxBytesRemaining_ = length;
        callback(std::error_code{}, 0, false);
    }

    template <typename F>
    void readWampPayload(MessageBuffer& payload, F&& callback)
    {
        assert(wampRxBytesRemaining_ != 0);

        bool tooLong = wampPayloadLimit_ != 0 &&
                       wampRxBytesRemaining_ > wampPayloadLimit_;
        if (tooLong)
            return failRead(TransportErrc::inboundTooLong, callback);

        try
        {
            payload.resize(wampRxBytesRemaining_);
        }
        catch (const std::bad_alloc&)
        {
            return failRead(std::errc::not_enough_memory,
                            std::forward<F>(callback));
        }
        catch (const std::length_error&)
        {
            return failRead(std::errc::not_enough_memory,
                            std::forward<F>(callback));
        }

        readMoreWampPayload(payload, std::forward<F>(callback));
    }

    template <typename F>
    void readMoreWampPayload(MessageBuffer& payload, F&& callback)
    {
        struct Read
        {
            Decay<F> callback;
            RawsockStream* self;

            void operator()(boost::system::error_code netEc, std::size_t n)
            {
                self->onWampPayloadRead(netEc, n, callback);
            }
        };

        assert(payload.size() >= wampRxBytesRemaining_);
        auto bytesReadSoFar = payload.size() - wampRxBytesRemaining_;
        auto ptr = payload.data() + bytesReadSoFar;
        socket_.async_read_some(
            boost::asio::buffer(ptr, wampRxBytesRemaining_),
            Read{std::forward<F>(callback), this});
    }

    template <typename F>
    void onWampPayloadRead(boost::system::error_code netEc,
                           std::size_t bytesRead, F& callback)
    {
        assert(bytesRead <= wampRxBytesRemaining_);
        wampRxBytesRemaining_ -= bytesRead;
        bool done = wampRxBytesRemaining_ == 0;
        callback(rawsockErrorCodeToStandard(netEc), bytesRead, done);
    }

    template <typename F>
    void readHeartbeatPayload(TransportFrameKind kind, size_t length,
                              F& callback)
    {
        struct Read
        {
            Decay<F> callback;
            RawsockStream* self;
            TransportFrameKind kind;

            void operator()(boost::system::error_code netEc, std::size_t)
            {
                self->onHeartbeatPayloadRead(netEc, kind, callback);
            }
        };

        if (heartbeatPayloadLimit_ != 0 && length > heartbeatPayloadLimit_)
            return failRead(TransportErrc::inboundTooLong, callback);

        try
        {
            heartbeatPayload_.resize(length);
        }
        catch (const std::bad_alloc&)
        {
            return failRead(std::errc::not_enough_memory, callback);
        }
        catch (const std::length_error&)
        {
            return failRead(std::errc::not_enough_memory, callback);
        }

        if (length == 0)
            onHeartbeatPayloadRead({}, kind, callback);

        boost::asio::async_read(
            socket_,
            boost::asio::buffer(heartbeatPayload_.data(), length),
            Read{std::move(callback), this, kind});
    }

    template <typename F>
    void onHeartbeatPayloadRead(boost::system::error_code netEc,
                                TransportFrameKind kind, F& callback)
    {
        if (!checkRead(netEc, callback))
            return;

        if (heartbeatHandler_ != nullptr)
        {
            postAny(socket_.get_executor(), heartbeatHandler_, kind,
                    heartbeatPayload_.data(), heartbeatPayload_.size());
        }

        doAwaitRead(callback);
    }

    template <typename F>
    bool checkRead(boost::system::error_code netEc, F&& callback)
    {
        if (netEc == boost::asio::error::eof)
        {
            callback(make_error_code(TransportErrc::ended), 0, true);
            return false;
        }

        if (netEc)
            callback(rawsockErrorCodeToStandard(netEc), 0, false);
        return !netEc;
    }

    template <typename TErrc, typename F>
    void failRead(TErrc errc, F&& callback)
    {
        callback(make_error_code(errc), 0, false);
    }

    static constexpr auto unlimited_ = std::numeric_limits<std::size_t>::max();

    Socket socket_;
    MessageBuffer heartbeatPayload_;
    HeartbeatHandler heartbeatHandler_;
    std::size_t wampPayloadLimit_ = unlimited_;
    std::size_t heartbeatPayloadLimit_ = unlimited_;
    std::size_t wampRxBytesRemaining_ = 0;
    Header rxHeader_ = 0;
    Header txHeader_ = 0;
    bool headerSent_ = false;
    bool payloadReadStarted_ = false;
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
    using ListenerSocket  = typename Traits::NetProtocol::socket;
    using Socket          = ListenerSocket;
    using Settings        = typename Traits::ServerSettings;
    using SettingsPtr     = std::shared_ptr<Settings>;
    using Handler         = AnyCompletionHandler<void (AdmitResult)>;
    using ShutdownHandler = AnyCompletionHandler<void (std::error_code, bool)>;

    explicit RawsockAdmitter(ListenerSocket&& s, SettingsPtr p,
                             const CodecIdSet& c)
        : socket_(std::move(s)),
          codecIds_(c),
          settings_(p)
    {}

    void admit(bool isShedding, Handler handler)
    {
        isShedding_ = isShedding;
        handler_ = std::move(handler);

        auto self = this->shared_from_this();
        isReading_ = true;
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(&handshake_, sizeof(handshake_)),
            [this, self](boost::system::error_code ec, size_t)
            {
                isReading_ = false;

                if (shutdownHandler_ != nullptr)
                {
                    if (ec == boost::asio::error::eof)
                        shutdownHandler_(std::error_code{}, false);
                    else
                        shutdownHandler_(rawsockErrorCodeToStandard(ec), false);

                    shutdownHandler_ = nullptr;
                    return;
                }

                if (check(ec, "socket read"))
                    onHandshakeReceived(Handshake::fromBigEndian(handshake_));
            });
    }

    void shutdown(std::error_code /*reason*/, ShutdownHandler handler)
    {
        boost::system::error_code netEc;
        socket_.shutdown(Socket::shutdown_send, netEc);
        auto ec = static_cast<std::error_code>(netEc);
        if (ec)
        {
            postAny(socket_.get_executor(), std::move(handler), ec, true);
            return;
        }

        shutdownHandler_ = std::move(handler);
        if (!isReading_)
            flush();
    }

    void close() {socket_.close();}

    const TransportInfo& transportInfo() const {return transportInfo_;}

    Socket&& releaseSocket() {return std::move(socket_);}

private:
    using Handshake = internal::RawsockHandshake;

    void onHandshakeReceived(Handshake hs)
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
            const auto rxLimit = settings_->limits().readMsgSize();
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
        auto txLimit = settings_->limits().writeMsgSize();
        txLimit = txLimit < peerSizeLimit_ ? txLimit : peerSizeLimit_;
        const auto rxLimit = settings_->limits().readMsgSize();
        transportInfo_ = TransportInfo{codecId, txLimit, rxLimit};

        finish(AdmitResult::wamp(codecId));
    }

    bool check(boost::system::error_code netEc, const char* operation)
    {
        if (netEc)
        {
            socket_.close();
            finish(AdmitResult::failed(rawsockErrorCodeToStandard(netEc),
                                       operation));
        }
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
        socket_.async_read_some(
            boost::asio::buffer(buffer_.data(), +flushReadSize_),
            [this, self](boost::system::error_code netEc, size_t n)
            {
                if (!netEc)
                    return flush();

                socket_.close();

                if (netEc == boost::asio::error::eof)
                    shutdownHandler_(std::error_code{}, false);
                else
                    shutdownHandler_(rawsockErrorCodeToStandard(netEc), false);

                shutdownHandler_ = nullptr;
            });
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
                            RawsockStream<TTraits>>;

//------------------------------------------------------------------------------
template <typename TTraits>
using RawsockServerTransport =
    QueueingServerTransport<typename TTraits::ServerSettings,
                            RawsockAdmitter<TTraits>>;

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_RAWSOCKTRANSPORT_HPP
