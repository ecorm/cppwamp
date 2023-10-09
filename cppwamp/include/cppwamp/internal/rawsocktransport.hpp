/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_RAWSOCKTRANSPORT_HPP
#define CPPWAMP_INTERNAL_RAWSOCKTRANSPORT_HPP

#include <array>
#include <cstdint>
#include <memory>
#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include "../basictransport.hpp"
#include "../codec.hpp"
#include "../routerlogger.hpp"
#include "rawsockheader.hpp"
#include "rawsockhandshake.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// Combines a raw socket transport header with an encoded message payload.
//------------------------------------------------------------------------------
class RawsockFrame
{
public:
    using Ptr        = std::shared_ptr<RawsockFrame>;
    using Header     = uint32_t;
    using GatherBufs = std::array<boost::asio::const_buffer, 2>;

    RawsockFrame() = default;

    RawsockFrame(TransportFrameKind kind, MessageBuffer&& payload)
        : payload_(std::move(payload)),
          header_(computeHeader(kind, payload_))
    {}

    void clear()
    {
        header_ = 0;
        payload_.clear();
        isPoisoned_ =false;
    }

    void resize(size_t length) {payload_.resize(length);}

    void prepare(TransportFrameKind kind, MessageBuffer&& payload)
    {
        header_ = computeHeader(kind, payload);
        payload_ = std::move(payload);
    }

    RawsockHeader header() const {return RawsockHeader::fromBigEndian(header_);}

    const MessageBuffer& payload() const & {return payload_;}

    MessageBuffer&& payload() && {return std::move(payload_);}

    void poison(bool poisoned = true) {isPoisoned_ = poisoned;}

    bool isPoisoned() const {return isPoisoned_;}

    GatherBufs gatherBuffers()
    {
        return GatherBufs{{ {&header_, sizeof(header_)},
                            {payload_.data(), payload_.size()} }};
    }

    boost::asio::mutable_buffer headerBuffer()
    {
        return boost::asio::buffer(&header_, sizeof(header_));
    }

    boost::asio::mutable_buffer payloadBuffer()
    {
        return boost::asio::buffer(&payload_.front(), payload_.size());
    }

private:
    static Header computeHeader(TransportFrameKind kind,
                                const MessageBuffer& payload)
    {
        return RawsockHeader().setMsgKind(kind)
                              .setLength(payload.size())
                              .toBigEndian();
    }

    MessageBuffer payload_;
    Header header_ = 0;
    bool isPoisoned_ = false;
};

//------------------------------------------------------------------------------
template <typename TTraits>
struct BasicRawsockTransportConfig
{
    using Traits = TTraits;

    // Allows pre-processing transport frame payloads for test purposes.
    static void preProcess(TransportFrameKind&, MessageBuffer&) {}

    // Allows altering server handshake bytes for test purposes.
    static uint32_t hostOrderHandshakeBytes(int codecId,
                                            RawsockMaxLength maxRxLength)
    {
        return RawsockHandshake().setCodecId(codecId)
            .setMaxLength(maxRxLength)
            .toHostOrder();
    }
};

//------------------------------------------------------------------------------
inline std::error_code rawsockErrorCodeToStandard(
    boost::system::error_code netEc)
{
    bool disconnected =
        netEc == boost::asio::error::broken_pipe ||
        netEc == boost::asio::error::connection_reset ||
        netEc == boost::asio::error::eof;
    auto ec = disconnected
                  ? make_error_code(TransportErrc::disconnected)
                  : static_cast<std::error_code>(netEc);
    if (netEc == boost::asio::error::operation_aborted)
        ec = make_error_code(TransportErrc::aborted);
    return ec;
}

//------------------------------------------------------------------------------
template <typename TConfig>
class RawsockTransport : public BasicTransport<RawsockTransport<TConfig>>
{
public:
    using Config       = TConfig;
    using Traits       = typename Config::Traits;
    using NetProtocol  = typename Traits::NetProtocol;
    using Ptr          = std::shared_ptr<RawsockTransport>;
    using Socket       = typename NetProtocol::socket;
    using CloseHandler = typename Transporting::CloseHandler;

protected:
    // Constructor for client transports
    RawsockTransport(Socket&& socket, TransportInfo info)
        : Base(boost::asio::make_strand(socket.get_executor()),
               Traits::connectionInfo(socket.remote_endpoint(), {}),
               info),
          socket_(std::move(socket))
    {}

    // Constructor for server transports
    RawsockTransport(Socket& socket, const std::string& server)
        : Base(boost::asio::make_strand(socket.get_executor()),
               Traits::connectionInfo(socket.remote_endpoint(), server)),
          socket_(socket.get_executor())
    {}

    Socket& socket() {return socket_;}

    void assignSocket(Socket&& socket, TransportInfo info)
    {
        socket_ = std::move(socket);
        Base::completeAdmission(info);
    }

private:
    using Base       = BasicTransport<RawsockTransport<TConfig>>;
    using Header     = uint32_t;
    using GatherBufs = std::array<boost::asio::const_buffer, 2>;

    static Header computeHeader(TransportFrameKind kind,
                                const MessageBuffer& payload)
    {
        return RawsockHeader().setMsgKind(kind)
                              .setLength(payload.size())
                              .toBigEndian();
    }

    bool socketIsOpen() const {return socket_.is_open();}

    void enablePinging() {}

    void disablePinging() {}

    void stopTransport() {socket_.close();}

    void closeTransport(CloseHandler handler)
    {
        stopTransport();
        Base::post(std::move(handler), true);
    }

    void cancelClose() {}

    void failTransport(std::error_code ec) {stopTransport();}

    template <typename F>
    void transmitMessage(TransportFrameKind kind, MessageBuffer& payload,
                         F&& callback)
    {
        struct Sent
        {
            Decay<F> callback;

            void operator()(boost::system::error_code netEc, std::size_t)
            {
                callback(rawsockErrorCodeToStandard(netEc));
            }
        };

        Config::preProcess(kind, payload);
        txHeader_ = computeHeader(kind, payload);
        auto buffers = GatherBufs{{ {&txHeader_, sizeof(txHeader_)},
                                    {payload.data(), payload.size()} }};
        boost::asio::async_write(socket_, buffers, Sent{std::move(callback)});
    }

    template <typename F>
    void receiveMessage(MessageBuffer& payload, F&& callback)
    {
        struct Received
        {
            Decay<F> callback;
            MessageBuffer* payload;
            RawsockTransport* self;

            void operator()(boost::system::error_code netEc, std::size_t)
            {
                self->onHeaderReceived(netEc, payload, callback);
            }
        };

        boost::asio::async_read(
            socket_,
            boost::asio::buffer(&rxHeader_, sizeof(rxHeader_)),
            Received{std::move(callback), &payload, this});
    }

    template <typename F>
    void onHeaderReceived(boost::system::error_code netEc,
                          MessageBuffer* payload, F& callback)
    {
        if (!check(netEc, callback))
            return;

        const auto hdr = RawsockHeader::fromBigEndian(rxHeader_);
        const auto len  = hdr.length();
        const auto maxRxLen = Base::info().maxRxLength();
        const bool ok =
            check(len <= maxRxLen, TransportErrc::inboundTooLong, callback) &&
            check(hdr.msgTypeIsValid(), TransportErrc::badCommand, callback);
        if (ok)
            receivePayload(hdr.msgKind(), len, payload, callback);
    }

    template <typename F>
    void receivePayload(TransportFrameKind kind, size_t length,
                        MessageBuffer* payload, F& callback)
    {
        struct Received
        {
            Decay<F> callback;
            MessageBuffer* payload;
            RawsockTransport* self;
            TransportFrameKind kind;

            void operator()(boost::system::error_code netEc, std::size_t)
            {
                self->onPayloadReceived(netEc, payload, kind, callback);
            }
        };

        payload->resize(length);
        auto self = this->shared_from_this();
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(payload->data(), length),
            Received{std::move(callback), payload, this, kind});
    }

    template <typename F>
    void onPayloadReceived(boost::system::error_code netEc,
                           MessageBuffer* payload, TransportFrameKind kind,
                           F& callback)
    {
        if (check(netEc, callback))
        {
            switch (kind)
            {
            case TransportFrameKind::wamp:
                callback(true);
                break;

            case TransportFrameKind::ping:
                Base::enqueuePong(std::move(*payload));
                callback(false);
                break;

            case TransportFrameKind::pong:
                Base::onPong(payload->data(), payload->size());
                callback(false);
                break;

            default:
                assert(false);
                break;
            }
        }
    }

    template <typename F>
    bool check(boost::system::error_code netEc, F& callback)
    {
        if (netEc)
            callback(makeUnexpected(rawsockErrorCodeToStandard(netEc)));
        return !netEc;
    }

    template <typename TErrc, typename F>
    bool check(bool condition, TErrc errc, F& callback)
    {
        if (!condition)
            callback(makeUnexpectedError(errc));
        return condition;
    }

    Socket socket_;
    Header txHeader_ = 0;
    Header rxHeader_ = 0;

    friend class BasicTransport<RawsockTransport<TConfig>>;
};


//------------------------------------------------------------------------------
template <typename TConfig>
class RawsockClientTransport : public RawsockTransport<TConfig>
{
public:
    using Ptr      = std::shared_ptr<RawsockClientTransport>;
    using Socket   = typename TConfig::Traits::NetProtocol::socket;
    using Settings = typename TConfig::Traits::ClientSettings;

    RawsockClientTransport(Socket&& s, const Settings&, TransportInfo t)
        : Base(std::move(s), t)
    {}

private:
    using Base = RawsockTransport<TConfig>;
};

//------------------------------------------------------------------------------
template <typename TConfig>
class RawsockAdmitter
    : public std::enable_shared_from_this<RawsockAdmitter<TConfig>>
{
public:
    using Ptr         = std::shared_ptr<RawsockAdmitter>;
    using Socket      = typename TConfig::Traits::NetProtocol::socket;
    using Settings    = typename TConfig::Traits::ServerSettings;
    using SettingsPtr = std::shared_ptr<Settings>;
    using Handler     = AnyCompletionHandler<void (ErrorOr<Socket*>)>;

    explicit RawsockAdmitter(Socket&& s, SettingsPtr p, const CodecIdSet& c)
        : socket_(std::move(s)),
          timer_(socket_.get_executor()),
          codecIds_(c),
          settings_(p)
    {}

    void admit(bool isShedding, Timeout timeout, Handler handler)
    {
        isShedding_ = isShedding;
        handler_ = std::move(handler);

        auto self = this->shared_from_this();

        if (timeoutIsDefinite(timeout))
        {
            timer_.expires_after(timeout);
            timer_.async_wait(
                [this, self](boost::system::error_code ec) {onTimeout(ec);});
        }

        boost::asio::async_read(
            socket_,
            boost::asio::buffer(&handshake_, sizeof(handshake_)),
            [this, self](boost::system::error_code ec, size_t)
            {
                timer_.cancel();
                if (check(ec))
                    onHandshakeReceived(Handshake::fromBigEndian(handshake_));
            });
    }

    void cancel() {socket_.close();}

    const TransportInfo& transportInfo() const {return transportInfo_;}

private:
    using Handshake = internal::RawsockHandshake;

    void onTimeout(boost::system::error_code ec)
    {
        if (!ec)
            return fail(TransportErrc::timeout);
        if (ec != boost::asio::error::operation_aborted)
            fail(static_cast<std::error_code>(ec));
    }

    void onHandshakeReceived(Handshake hs)
    {
        timer_.cancel();
        auto peerCodec = hs.codecId();

        if (isShedding_)
        {
            sendRefusal();
        }
        else if (!hs.hasMagicOctet())
        {
            fail(TransportErrc::badHandshake);
        }
        else if (hs.reserved() != 0)
        {
            sendHandshake(Handshake::eReservedBitsUsed());
        }
        else if (codecIds_.count(peerCodec) != 0)
        {
            maxTxLength_ = hs.maxLength();
            auto bytes = TConfig::hostOrderHandshakeBytes(
                peerCodec, settings_->maxRxLength());
            sendHandshake(Handshake(bytes));
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
                if (check(ec))
                    fail(TransportErrc::shedded);
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
                if (check(ec))
                    onHandshakeSent(hs, ec);
            });
    }

    void onHandshakeSent(Handshake hs, boost::system::error_code ec)
    {
        if (!hs.hasError())
            complete(hs);
        else
            fail(hs.errorCode());
    }

    void complete(Handshake hs)
    {
        auto codecId = hs.codecId();
        transportInfo_ =
            TransportInfo{codecId,
                          Handshake::byteLengthOf(maxTxLength_),
                          Handshake::byteLengthOf(settings_->maxRxLength())};
        handler_(&socket_);
        handler_ = nullptr;
    }

    bool check(boost::system::error_code netEc)
    {
        if (netEc)
            fail(rawsockErrorCodeToStandard(netEc));
        return !netEc;
    }

    void fail(std::error_code ec)
    {
        if (!handler_)
            return;
        handler_(makeUnexpected(ec));
        handler_ = nullptr;
        socket_.close();
    }

    template <typename TErrc>
    void fail(TErrc errc) {fail(make_error_code(errc));}

    Socket socket_;
    boost::asio::steady_timer timer_;
    CodecIdSet codecIds_;
    TransportInfo transportInfo_;
    Handler handler_;
    SettingsPtr settings_;
    uint32_t handshake_ = 0;
    RawsockMaxLength maxTxLength_ = {};
    bool isShedding_ = false;
};

//------------------------------------------------------------------------------
template <typename TConfig>
class RawsockServerTransport : public RawsockTransport<TConfig>
{
public:
    using Ptr           = std::shared_ptr<RawsockServerTransport>;
    using Socket        = typename TConfig::Traits::NetProtocol::socket;
    using Settings      = typename TConfig::Traits::ServerSettings;
    using SettingsPtr   = std::shared_ptr<Settings>;
    using AdmitHandler = Transporting::AdmitHandler;

    RawsockServerTransport(Socket&& s, SettingsPtr p, const CodecIdSet& c,
                           const std::string& server, RouterLogger::Ptr l)
        : Base(s, server),
          admitter_(std::make_shared<Admitter>(std::move(s), std::move(p), c))
    {}

private:
    using Base        = RawsockTransport<TConfig>;
    using Handshake   = internal::RawsockHandshake;
    using Admitter    = RawsockAdmitter<TConfig>;
    using AdmitterPtr = typename Admitter::Ptr;

    void onAdmit(Timeout timeout, AdmitHandler handler) override
    {
        assert((admitter_ != nullptr) && "Admit already performed");

        struct Admitted
        {
            AdmitHandler handler;
            Ptr self;

            void operator()(ErrorOr<Socket*> s)
            {
                self->onAdmitCompletion(s, handler);
            }
        };

        auto self = std::dynamic_pointer_cast<RawsockServerTransport>(
            this->shared_from_this());
        bool isShedding = Base::state() == TransportState::shedding;
        admitter_->admit(isShedding, timeout,
                         Admitted{std::move(handler), std::move(self)});
    }

    void onCancelAdmission() override
    {
        if (admitter_)
            admitter_->cancel();
    }

    void onAdmitCompletion(ErrorOr<Socket*> socket, AdmitHandler& handler)
    {
        if (socket.has_value())
        {
            const auto& info = admitter_->transportInfo();
            Base::assignSocket(std::move(**socket), info);
            Base::post(std::move(handler), info.codecId());
        }
        else
        {
            Base::shutdown();
            Base::post(std::move(handler), makeUnexpected(socket.error()));
        }

        admitter_.reset();
    }

    typename RawsockAdmitter<TConfig>::Ptr admitter_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_RAWSOCKTRANSPORT_HPP
