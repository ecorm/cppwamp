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
template <typename TTraits>
class RawsockStream
{
public:
    using Traits      = TTraits;
    using NetProtocol = typename Traits::NetProtocol;
    using Socket      = typename NetProtocol::socket;
    using Buffer      = MessageBuffer;
    using ControlFrameHandler =
        std::function<void (TransportFrameKind, const uint8_t* data,
                            std::size_t size)>;

    explicit RawsockStream(Socket&& socket) : socket_(std::move(socket)) {}

    AnyIoExecutor executor() {return socket_.get_executor();}

    bool isOpen() const {return socket_.is_open();}

    template <typename L>
    void setLimits(const L& limits)
    {
        wampFrameLimit_ = limits.bodySizeLimit();
        controlFrameLimit_ = limits.controlSizeLimit();
    }

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
    void write(const uint8_t* data, std::size_t size, F&& callback)
    {
        if (!headerSent_)
            return writeWampHeader(data, size, std::forward<F>(callback));
        writeMoreWampPayload(data, size, std::forward<F>(callback));
    }

    template <typename F>
    void writeWampHeader(const uint8_t* data, std::size_t size, F&& callback)
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
                    callback(rawsockErrorCodeToStandard(netEc));
                    return;
                }

                self->writeMoreWampPayload(data, size, callback);
            }
        };

        txHeader_ = computeHeader(TransportFrameKind::wamp, size);
        boost::asio::async_write(
            socket_,
            boost::asio::const_buffer{&txHeader_, sizeof(txHeader_)},
            Written{std::move(callback)});
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
    void read(Buffer& buffer, F&& callback)
    {
        if (wampFrameReadInProgress_)
            return readMoreWampPayload(buffer, std::forward<F>(callback));
        readHeader(buffer, std::forward<F>(callback));
    }

    std::error_code shutdown(std::error_code reason = {})
    {
        boost::system::error_code netEc;
        socket_.shutdown(Socket::shutdown_send, netEc);
        return netEc;
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
    void readHeader(Buffer& wampPayload, F&& callback)
    {
        struct Received
        {
            Decay<F> callback;
            MessageBuffer* payload;
            RawsockStream* self;

            void operator()(boost::system::error_code netEc, std::size_t)
            {
                self->onHeaderRead(netEc, *payload, callback);
            }
        };

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
            callback(makeUnexpectedError(TransportErrc::ended));
            return;
        }
        if (!check(netEc, callback))
            return;

        const auto header = RawsockHeader::fromBigEndian(rxHeader_);
        if (!header.msgTypeIsValid())
            return fail(TransportErrc::badCommand, callback);

        auto kind = header.msgKind();
        auto length = header.length();
        auto limit = kind == TransportFrameKind::wamp ? wampFrameLimit_
                                                      : controlFrameLimit_;
        if (limit != 0 && length > limit)
            return fail(TransportErrc::inboundTooLong, callback);

        if (kind == TransportFrameKind::wamp)
            readWampPayload(length, wampPayload, callback);

        readControlPayload(kind, length, wampPayload, callback);
    }

    template <typename F>
    void readWampPayload(size_t length, Buffer& payload, F& callback)
    {
        if (wampFrameLimit_ != 0 && length > wampFrameLimit_)
            return fail(TransportErrc::inboundTooLong, callback);

        try
        {
            payload.reserve(length);
        }
        catch (const std::bad_alloc&)
        {
            return fail(std::errc::not_enough_memory);
        }
        catch (const std::length_error&)
        {
            return fail(std::errc::not_enough_memory);
        }

        wampFrameReadInProgress_ = true;
        wampRxBytesRemaining_ = length;
        readMoreWampPayload(payload, callback);
    }

    template <typename F>
    void readMoreWampPayload(TransportFrameKind kind, MessageBuffer& payload,
                             F& callback)
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

        auto self = this->shared_from_this();
        socket_.async_read_some(
            boost::asio::dynamic_buffer(payload),
            Read{std::move(callback), this});
    }

    template <typename F>
    void onWampPayloadRead(boost::system::error_code netEc,
                           std::size_t bytesRead, F& callback)
    {
        assert(bytesRead <= wampRxBytesRemaining_);
        wampRxBytesRemaining_ -= bytesRead;
        bool done = wampRxBytesRemaining_ == 0;
        wampFrameReadInProgress_ = !done;
        callback(rawsockErrorCodeToStandard(netEc), bytesRead, done);
    }

    template <typename F>
    void readControlPayload(TransportFrameKind kind, size_t length,
                            Buffer& wampPayload, F& callback)
    {
        struct Read
        {
            Decay<F> callback;
            Buffer* wampPayload;
            RawsockStream* self;
            TransportFrameKind kind;

            void operator()(boost::system::error_code netEc, std::size_t)
            {
                self->onControlPayloadRead(netEc, kind, *wampPayload, callback);
            }
        };

        if (controlFrameLimit_ != 0 && length > controlFrameLimit_)
            return fail(TransportErrc::inboundTooLong, callback);

        try
        {
            controlFramePayload_.resize(length);
        }
        catch (const std::bad_alloc&)
        {
            return fail(std::errc::not_enough_memory);
        }
        catch (const std::length_error&)
        {
            return fail(std::errc::not_enough_memory);
        }

        auto self = this->shared_from_this();
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(controlFramePayload_.data(), length),
            Read{std::move(callback), this, kind});
    }

    template <typename F>
    void onControlPayloadRead(boost::system::error_code netEc,
                              TransportFrameKind kind,
                              MessageBuffer& wampPayload, F& callback)
    {
        if (!check(netEc, callback))
            return;

        if (controlFrameHandler_ != nullptr)
        {
            postAny(socket_->get_executor(), controlFrameHandler_, kind,
                    controlFramePayload_.data(), controlFramePayload_.size());
        }

        read(wampPayload, callback);
    }

    template <typename F>
    bool check(boost::system::error_code netEc, F& callback)
    {
        if (netEc)
            callback(makeUnexpected(rawsockErrorCodeToStandard(netEc)));
        return !netEc;
    }

    template <typename TErrc, typename F>
    void fail(TErrc errc, F& callback)
    {
        callback(makeUnexpectedError(errc));
    }

    Socket socket_;
    std::vector<uint8_t> controlFramePayload_;
    ControlFrameHandler controlFrameHandler_;
    std::size_t wampFrameLimit_ = std::numeric_limits<std::size_t>::max();
    std::size_t controlFrameLimit_ = std::numeric_limits<std::size_t>::max();
    std::size_t wampRxBytesRemaining_ = 0;
    Header txHeader_ = 0;
    Header rxHeader_ = 0;
    bool headerSent_ = false;
    bool wampFrameReadInProgress_ = false;
};

//------------------------------------------------------------------------------
template <typename TTraits>
class RawsockAdmitter
    : public std::enable_shared_from_this<RawsockAdmitter<TTraits>>
{
public:
    using Traits      = TTraits;
    using Ptr         = std::shared_ptr<RawsockAdmitter>;
    using Stream      = RawsockStream<Traits>;
    using Socket      = typename Traits::NetProtocol::socket;
    using Settings    = typename Traits::ServerSettings;
    using SettingsPtr = std::shared_ptr<Settings>;
    using Handler     = AnyCompletionHandler<void (AdmitResult)>;

    explicit RawsockAdmitter(Socket&& s, SettingsPtr p, const CodecIdSet& c)
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

    void cancel() {socket_.close();}

    const TransportInfo& transportInfo() const {return transportInfo_;}

    Stream releaseStream() {return Stream{std::move(socket_)};}

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
            maxTxLength_ = hs.maxLength();
            sendHandshake(Handshake().setCodecId(peerCodec)
                                     .setMaxLength(settings_->maxRxLength()));
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
        auto codecId = hs.codecId();
        transportInfo_ =
            TransportInfo{codecId,
                          Handshake::byteLengthOf(maxTxLength_),
                          Handshake::byteLengthOf(settings_->maxRxLength())};
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
    uint32_t handshake_ = 0;
    RawsockMaxLength maxTxLength_ = {};
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
