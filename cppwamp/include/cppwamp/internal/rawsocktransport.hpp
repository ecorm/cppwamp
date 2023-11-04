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
#include "../basictransport.hpp"
#include "../codec.hpp"
#include "../errorcodes.hpp"
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
// TODO: Rename Control -> Heartbeat
template <typename TTraits>
class RawsockStream
{
public:
    using Traits      = TTraits;
    using NetProtocol = typename Traits::NetProtocol;
    using Socket      = typename NetProtocol::socket;
    using ControlFrameHandler =
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
          wampFrameLimit_(settings->limits().rxMsgSize()),
        heartbeatFrameLimit_(settings->limits().heartbeatSize())
    {}

    AnyIoExecutor executor() {return socket_.get_executor();}

    bool isOpen() const {return socket_.is_open();}

    void observeControlFrames(ControlFrameHandler handler)
    {
        controlFrameHandler_ = std::move(handler);
    }

    void unobserveControlFrames()
    {
        controlFrameHandler_ = nullptr;
    }

    template <typename F>
    void ping(const uint8_t* data, std::size_t size, F&& callback)
    {
        sendControlFrame(TransportFrameKind::ping, data, size,
                         std::forward<F>(callback));
    }

    template <typename F>
    void pong(const uint8_t* data, std::size_t size, F&& callback)
    {
        sendControlFrame(TransportFrameKind::pong, data, size,
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
    void readSome(MessageBuffer& buffer, F&& callback)
    {
        if (wampRxBytesRemaining_ != 0)
            return readMoreWampPayload(buffer, callback);
        readHeader(buffer, std::forward<F>(callback));
    }

    template <typename F>
    void shutdown(std::error_code reason, F&& callback)
    {
        boost::system::error_code netEc;
        socket_.shutdown(Socket::shutdown_send, netEc);
        auto ec = static_cast<std::error_code>(netEc);
        postAny(socket_.get_executor(), std::forward<F>(callback), ec);
    }

    void close() {socket_.close();}


private:
    using Header     = uint32_t;
    using GatherBufs = std::array<boost::asio::const_buffer, 2>;

    static Header computeHeader(TransportFrameKind kind, std::size_t size)
    {
        return RawsockHeader().setMsgKind(kind).setLength(size).toBigEndian();
    }

    template <typename F>
    void sendControlFrame(TransportFrameKind kind, const uint8_t* data,
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

            void operator()(boost::system::error_code netEc, std::size_t n)
            {
                if (netEc)
                {
                    callback(rawsockErrorCodeToStandard(netEc), 0);
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
    void readHeader(MessageBuffer& wampPayload, F&& callback)
    {
        struct Received
        {
            Decay<F> callback;
            MessageBuffer* payload;
            RawsockStream* self;

            void operator()(boost::system::error_code netEc, std::size_t n)
            {
                self->onHeaderRead(netEc, *payload, callback);
            }
        };

        rxHeader_ = 0;
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(&rxHeader_, sizeof(rxHeader_)),
            Received{std::move(callback), &wampPayload, this});
    }

    template <typename F>
    void onHeaderRead(boost::system::error_code netEc,
                      MessageBuffer& wampPayload, F& callback)
    {
        if (netEc == boost::asio::error::eof)
        {
            callback(make_error_code(TransportErrc::ended), 0, true);
            return;
        }
        if (!checkRead(netEc, callback))
            return;

        const auto header = RawsockHeader::fromBigEndian(rxHeader_);
        if (!header.msgTypeIsValid())
            return failRead(TransportErrc::badCommand, callback);

        auto kind = header.msgKind();
        auto length = header.length();
        auto limit = kind == TransportFrameKind::wamp ? wampFrameLimit_
                                                      : heartbeatFrameLimit_;
        if (limit != 0 && length > limit)
            return failRead(TransportErrc::inboundTooLong, callback);

        if (kind == TransportFrameKind::wamp)
            return readWampPayload(length, wampPayload, callback);

        readControlPayload(kind, length, wampPayload, callback);
    }

    template <typename F>
    void readWampPayload(size_t length, MessageBuffer& payload, F& callback)
    {
        if (wampFrameLimit_ != 0 && length > wampFrameLimit_)
            return failRead(TransportErrc::inboundTooLong, callback);

        if (length == 0)
        {
            callback(std::error_code{}, 0, true);
            return;
        }

        try
        {
            payload.resize(length);
        }
        catch (const std::bad_alloc&)
        {
            return failRead(std::errc::not_enough_memory, callback);
        }
        catch (const std::length_error&)
        {
            return failRead(std::errc::not_enough_memory, callback);
        }

        wampRxBytesRemaining_ = length;
        readMoreWampPayload(payload, callback);
    }

    template <typename F>
    void readMoreWampPayload(MessageBuffer& payload, F& callback)
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
        auto bytesRead = payload.size() - wampRxBytesRemaining_;
        auto ptr = payload.data() + bytesRead;
        socket_.async_read_some(
            boost::asio::buffer(ptr, wampRxBytesRemaining_),
            Read{std::move(callback), this});
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
    void readControlPayload(TransportFrameKind kind, size_t length,
                            MessageBuffer& wampPayload, F& callback)
    {
        struct Read
        {
            Decay<F> callback;
            MessageBuffer* wampPayload;
            RawsockStream* self;
            TransportFrameKind kind;

            void operator()(boost::system::error_code netEc, std::size_t)
            {
                self->onControlPayloadRead(netEc, kind, *wampPayload, callback);
            }
        };

        if (heartbeatFrameLimit_ != 0 && length > heartbeatFrameLimit_)
            return failRead(TransportErrc::inboundTooLong, callback);

        try
        {
            controlFramePayload_.resize(length);
        }
        catch (const std::bad_alloc&)
        {
            return failRead(std::errc::not_enough_memory, callback);
        }
        catch (const std::length_error&)
        {
            return failRead(std::errc::not_enough_memory, callback);
        }

        boost::asio::async_read(
            socket_,
            boost::asio::buffer(controlFramePayload_.data(), length),
            Read{std::move(callback), &wampPayload, this, kind});
    }

    template <typename F>
    void onControlPayloadRead(boost::system::error_code netEc,
                              TransportFrameKind kind,
                              MessageBuffer& wampPayload, F& callback)
    {
        if (!checkRead(netEc, callback))
            return;

        if (controlFrameHandler_ != nullptr)
        {
            postAny(socket_.get_executor(), controlFrameHandler_, kind,
                    controlFramePayload_.data(), controlFramePayload_.size());
        }

        readSome(wampPayload, callback);
    }

    template <typename F>
    bool checkRead(boost::system::error_code netEc, F& callback)
    {
        if (netEc)
            callback(rawsockErrorCodeToStandard(netEc), 0, false);
        return !netEc;
    }

    template <typename TErrc, typename F>
    void failRead(TErrc errc, F& callback)
    {
        callback(make_error_code(errc), 0, false);
    }

    Socket socket_;
    MessageBuffer controlFramePayload_;
    ControlFrameHandler controlFrameHandler_;
    std::size_t wampFrameLimit_ = std::numeric_limits<std::size_t>::max();
    std::size_t heartbeatFrameLimit_ = std::numeric_limits<std::size_t>::max();
    std::size_t wampRxBytesRemaining_ = 0;
    Header txHeader_ = 0;
    Header rxHeader_ = 0;
    bool headerSent_ = false;
};

//------------------------------------------------------------------------------
template <typename TTraits>
class RawsockAdmitter
    : public std::enable_shared_from_this<RawsockAdmitter<TTraits>>
{
public:
    using Traits         = TTraits;
    using Ptr            = std::shared_ptr<RawsockAdmitter>;
    using Stream         = RawsockStream<Traits>;
    using ListenerSocket = typename Traits::NetProtocol::socket;
    using Socket         = ListenerSocket;
    using Settings       = typename Traits::ServerSettings;
    using SettingsPtr    = std::shared_ptr<Settings>;
    using Handler        = AnyCompletionHandler<void (AdmitResult)>;

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
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(&handshake_, sizeof(handshake_)),
            [this, self](boost::system::error_code ec, size_t)
            {
                if (check(ec, "socket read"))
                    onHandshakeReceived(Handshake::fromBigEndian(handshake_));
            });
    }

    template <typename F>
    void shutdown(std::error_code /*reason*/, F&& callback)
    {
        boost::system::error_code netEc;
        socket_.shutdown(Socket::shutdown_send, netEc);
        auto ec = static_cast<std::error_code>(netEc);
        postAny(socket_.get_executor(), std::forward<F>(callback), ec);
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
            const auto rxLimit = settings_->limits().rxMsgSize();
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
        auto txLimit = settings_->limits().txMsgSize();
        txLimit = txLimit < peerSizeLimit_ ? txLimit : peerSizeLimit_;
        const auto rxLimit = settings_->limits().rxMsgSize();
        transportInfo_ = TransportInfo{codecId, txLimit, rxLimit};

        finish(AdmitResult::wamp(codecId));
    }

    bool check(boost::system::error_code netEc, const char* operation)
    {
        if (netEc)
        {
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
        if (result.status() != AdmitStatus::wamp)
        {
            if (result.status() != AdmitStatus::failed)
            {
                boost::system::error_code ec;
                socket_.shutdown(Socket::shutdown_send, ec);
            }
            socket_.close();
        }
    }

    Socket socket_;
    CodecIdSet codecIds_;
    TransportInfo transportInfo_;
    Handler handler_;
    SettingsPtr settings_;
    std::size_t peerSizeLimit_ = 0;
    uint32_t handshake_ = 0;
    bool isShedding_ = false;
};

//------------------------------------------------------------------------------
template <typename TTraits>
using RawsockClientTransport =
    BasicClientTransport<typename TTraits::ClientSettings,
                         RawsockStream<TTraits>>;

//------------------------------------------------------------------------------
template <typename TTraits>
using RawsockServerTransport =
    BasicServerTransport<typename TTraits::ServerSettings,
                         RawsockAdmitter<TTraits>>;

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_RAWSOCKTRANSPORT_HPP
