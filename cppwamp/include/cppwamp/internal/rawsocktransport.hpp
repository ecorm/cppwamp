/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_RAWSOCKTRANSPORT_HPP
#define CPPWAMP_INTERNAL_RAWSOCKTRANSPORT_HPP

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <utility>
#include <boost/asio/buffer.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include "../asiodefs.hpp"
#include "../codec.hpp"
#include "../errorcodes.hpp"
#include "../transport.hpp"
#include "pinger.hpp"
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

    RawsockFrame(RawsockMsgKind kind, MessageBuffer&& payload)
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

    void prepare(RawsockMsgKind kind, MessageBuffer&& payload)
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
    static Header computeHeader(RawsockMsgKind kind,
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
    static void preProcess(RawsockMsgKind&, MessageBuffer&) {}

    // Allows altering server handshake bytes for test purposes.
    static uint32_t hostOrderHandshakeBytes(int codecId,
                                            RawsockMaxLength maxRxLength)
    {
        return RawsockHandshake().setCodecId(codecId)
            .setMaxLength(maxRxLength)
            .toHostOrder();
    }

    // Allows mocking server unresponsiveness by not sending handshake bytes.
    // TODO: Remove if not needed
    static constexpr bool mockUnresponsiveness() {return false;}
};

//------------------------------------------------------------------------------
template <typename TConfig>
class RawsockTransport : public Transporting
{
public:
    using Config         = TConfig;
    using Traits         = typename Config::Traits;
    using NetProtocol    = typename Traits::NetProtocol;
    using Ptr            = std::shared_ptr<RawsockTransport>;
    using Socket         = typename NetProtocol::socket;
    using RxHandler      = typename Transporting::RxHandler;
    using TxErrorHandler = typename Transporting::TxErrorHandler;

protected:
    static std::error_code netErrorCodeToStandard(
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

    RawsockTransport(Socket&& socket, TransportInfo info)
        : Base(info, Traits::connectionInfo(socket.remote_endpoint())),
          strand_(boost::asio::make_strand(socket.get_executor())),
          socket_(std::move(socket))
    {
        if (timeoutIsDefinite(Base::info().heartbeatInterval()))
            pinger_ = std::make_shared<Pinger>(strand_, Base::info());
    }

    RawsockTransport(Socket&& socket)
        : strand_(boost::asio::make_strand(socket.get_executor())),
          socket_(std::move(socket))
    {}

    Socket& socket() {return socket_;}

    template <typename F, typename... Ts>
    void post(F&& handler, Ts&&... args) //
    {
        // NOLINTNEXTLINE(modernize-avoid-bind)
        boost::asio::post(strand_, std::bind(std::forward<F>(handler),
                                             std::forward<Ts>(args)...));
    }

private:
    using Base = Transporting;
    using TransmitQueue = std::deque<RawsockFrame::Ptr>;

    void onStart(RxHandler rxHandler, TxErrorHandler txErrorHandler) override
    {
        rxHandler_ = rxHandler;
        txErrorHandler_ = txErrorHandler;
        startPinger();
        receive();
    }

    void onSend(MessageBuffer message) override
    {
        if (!socket_.is_open())
            return;
        auto buf = enframe(RawsockMsgKind::wamp, std::move(message));
        sendFrame(std::move(buf));
    }

    void onSendNowAndStop(MessageBuffer message) override
    {
        if (!socket_.is_open())
            return;
        auto frame = enframe(RawsockMsgKind::wamp, std::move(message));
        assert((frame->payload().size() <= info().maxTxLength()) &&
               "Outgoing message is longer than allowed by peer");
        frame->poison();
        txQueue_.push_front(std::move(frame));
        transmit();
    }

    void onStop() override
    {
        rxHandler_ = nullptr;
        txErrorHandler_ = nullptr;
        txQueue_.clear();
        socket_.close();
        if (pinger_)
            pinger_->stop();
    }

    void startPinger()
    {
        if (pinger_)
        {
            std::weak_ptr<Transporting> self = shared_from_this();
            pinger_->start(
                [self, this](ErrorOr<PingBytes> pingBytes)
                {
                    auto me = self.lock();
                    if (me)
                        onPingFrame(pingBytes);
                });
        }
    }

    void onPingFrame(ErrorOr<PingBytes> pingBytes)
    {
        if (state() != Transporting::State::running)
            return;

        if (!pingBytes.has_value())
        {
            return fail(pingBytes.error());
        }

        MessageBuffer message{pingBytes->begin(), pingBytes->end()};
        auto buf = enframe(RawsockMsgKind::ping, std::move(message));
        sendFrame(std::move(buf));
    }

    RawsockFrame::Ptr enframe(RawsockMsgKind kind, MessageBuffer&& payload)
    {
        Config::preProcess(kind, payload);
        // TODO: Pool/reuse frames somehow
        return std::make_shared<RawsockFrame>(kind, std::move(payload));
    }

    void sendFrame(RawsockFrame::Ptr frame)
    {
        assert((frame->payload().size() <= info().maxTxLength()) &&
               "Outgoing message is longer than allowed by peer");
        txQueue_.push_back(std::move(frame));
        transmit();
    }

    // NOLINTBEGIN(misc-no-recursion)
    void transmit()
    {
        if (!isReadyToTransmit())
            return;

        txFrame_ = txQueue_.front();
        txQueue_.pop_front();

        auto self = this->shared_from_this();
        boost::asio::async_write(
            socket_,
            txFrame_->gatherBuffers(),
            [this, self](boost::system::error_code netEc, size_t)
            {
                txFrame_.reset();
                if (!checkTxError(netEc))
                    return;
                if (txFrame_ && txFrame_->isPoisoned())
                    onStop();
                else
                    transmit();
          });
    }
    // NOLINTEND(misc-no-recursion)

    bool isReadyToTransmit() const
    {
        return socket_.is_open() && // Socket is still open
               !txFrame_ &&         // No async_write is in progress
               !txQueue_.empty();   // One or more messages are enqueued
    }

    // NOLINTBEGIN(misc-no-recursion)
    void receive()
    {
        if (!socket_.is_open())
            return;

        rxFrame_.clear();
        auto self = this->shared_from_this();
        boost::asio::async_read(socket_, rxFrame_.headerBuffer(),
            [this, self](boost::system::error_code ec, size_t)
            {
                if (check(ec))
                    processHeader();
            });
    }
    // NOLINTEND(misc-no-recursion)

    // NOLINTNEXTLINE(misc-no-recursion)
    void processHeader()
    {
        const auto hdr = rxFrame_.header();
        const auto len  = hdr.length();
        const bool ok =
            check(len <= info().maxRxLength(), TransportErrc::inboundTooLong) &&
            check(hdr.msgTypeIsValid(), TransportErrc::badCommand);
        if (ok)
            receivePayload(hdr.msgKind(), len);
    }

    // NOLINTBEGIN(misc-no-recursion)
    void receivePayload(RawsockMsgKind kind, size_t length)
    {
        rxFrame_.resize(length);
        auto self = this->shared_from_this();
        boost::asio::async_read(socket_, rxFrame_.payloadBuffer(),
            [this, self, kind](boost::system::error_code ec, size_t)
            {
                if (ec)
                    rxFrame_.clear();

                if (check(ec) && state() == Transporting::State::running)
                {
                    switch (kind)
                    {
                    case RawsockMsgKind::wamp:
                        if (rxHandler_)
                            post(rxHandler_, std::move(rxFrame_).payload());
                        receive();
                        break;

                    case RawsockMsgKind::ping:
                        sendPong();
                        break;

                    case RawsockMsgKind::pong:
                        receivePong();
                        break;

                    default:
                        assert(false);
                        break;
                    }
                }
            });
    }
    // NOLINTEND(misc-no-recursion)

    // NOLINTNEXTLINE(misc-no-recursion)
    void sendPong()
    {
        auto f = enframe(RawsockMsgKind::pong, std::move(rxFrame_).payload());
        sendFrame(std::move(f));
        receive();
    }

    // NOLINTNEXTLINE(misc-no-recursion)
    void receivePong()
    {
        // Unsolicited pongs may serve as unidirectional heartbeats.
        // https://github.com/wamp-proto/wamp-proto/issues/274#issuecomment-288626150
        if (pinger_ != nullptr)
            pinger_->pong(rxFrame_.payload().data(), rxFrame_.payload().size());

        receive();
    }

    bool checkTxError(boost::system::error_code netEc)
    {
        if (!netEc)
            return true;
        if (txErrorHandler_)
            txErrorHandler_(netErrorCodeToStandard(netEc));
        cleanup();
        return false;
    }

    bool check(boost::system::error_code netEc)
    {
        if (netEc)
            fail(netErrorCodeToStandard(netEc));
        return !netEc;
    }

    template <typename TErrc>
    bool check(bool condition, TErrc errc)
    {
        if (!condition)
            fail(make_error_code(errc));
        return condition;
    }

    void fail(std::error_code ec)
    {
        if (rxHandler_)
            post(rxHandler_, makeUnexpected(ec));
        cleanup();
    }

    void cleanup()
    {
        Base::shutdown();
        rxHandler_ = nullptr;
        txErrorHandler_ = nullptr;
        rxFrame_.clear();
        txQueue_.clear();
        txFrame_ = nullptr;
        pingFrame_ = nullptr;
        socket_.close();
        pinger_.reset();
    }

    IoStrand strand_;
    Socket socket_;
    RxHandler rxHandler_;
    TxErrorHandler txErrorHandler_;
    RawsockFrame rxFrame_;
    TransmitQueue txQueue_;
    MessageBuffer pingBuffer_;
    RawsockFrame::Ptr txFrame_;
    RawsockFrame::Ptr pingFrame_;
    std::shared_ptr<Pinger> pinger_;
    uint32_t handshake_ = 0;
};


//------------------------------------------------------------------------------
template <typename TConfig>
class RawsockClientTransport : public RawsockTransport<TConfig>
{
public:
    using Ptr    = std::shared_ptr<RawsockClientTransport>;
    using Socket = typename TConfig::Traits::NetProtocol::socket;

    static Ptr create(Socket&& s, TransportInfo i)
    {
        return Ptr(new RawsockClientTransport(std::move(s), i));
    }

private:
    using Base = RawsockTransport<TConfig>;

    RawsockClientTransport(Socket&& s, TransportInfo info)
        : Base(std::move(s), info)
    {}
};

//------------------------------------------------------------------------------
template <typename TConfig>
class RawsockServerTransport : public RawsockTransport<TConfig>
{
public:
    using Ptr           = std::shared_ptr<RawsockServerTransport>;
    using Socket        = typename TConfig::Traits::NetProtocol::socket;
    using Settings      = typename TConfig::Traits::ServerSettings;
    using AcceptHandler = Transporting::AcceptHandler;

    static Ptr create(Socket&& s, const Settings& t, const CodecIdSet& c)
    {
        return Ptr(new RawsockServerTransport(std::move(s), std::move(t),
                                              std::move(c)));
    }

private:
    using Base = RawsockTransport<TConfig>;
    using Handshake = internal::RawsockHandshake;

    // Only used once to perform accept operation
    struct Data
    {
        explicit Data(const Settings& s, const CodecIdSet& c)
            : settings(s),
              codecIds(c)
        {}

        Settings settings;
        CodecIdSet codecIds;
        AcceptHandler handler;
        uint32_t handshake = 0;
        RawsockMaxLength maxTxLength = {};
    };

    RawsockServerTransport(Socket&& s, const Settings& t, const CodecIdSet& c)
        : Base(std::move(s)),
          data_(new Data(t, c))
    {}

    void onAccept(AcceptHandler handler) override
    {
        // TODO: Timeout waiting for handshake
        assert((data_ != nullptr) && "Accept already performed");
        data_->handler = std::move(handler);
        auto self = this->shared_from_this();
        boost::asio::async_read(
            Base::socket(),
            boost::asio::buffer(&data_->handshake, sizeof(Handshake)),
            [this, self](boost::system::error_code ec, size_t)
            {
                if (check(ec))
                {
                    onHandshakeReceived(
                        Handshake::fromBigEndian(data_->handshake));
                }
            });
    }

    void onCancelAccept() override {Base::socket().close();}

    void onHandshakeReceived(Handshake hs)
    {
        auto peerCodec = hs.codecId();

        if (!hs.hasMagicOctet())
        {
            fail(TransportErrc::badHandshake);
        }
        else if (hs.reserved() != 0)
        {
            sendHandshake(Handshake::eReservedBitsUsed());
        }
        else if (data_->codecIds.count(peerCodec) != 0)
        {
            data_->maxTxLength = hs.maxLength();
            auto bytes = TConfig::hostOrderHandshakeBytes(
                peerCodec, data_->settings.maxRxLength());
            if (!TConfig::mockUnresponsiveness())
                sendHandshake(Handshake(bytes));
        }
        else
        {
            sendHandshake(Handshake::eUnsupportedFormat());
        }
    }

    void sendHandshake(Handshake hs)
    {
        if (!Base::socket().is_open())
            return;

        data_->handshake = hs.toBigEndian();
        auto self = this->shared_from_this();
        boost::asio::async_write(
            Base::socket(),
            boost::asio::buffer(&data_->handshake, sizeof(Handshake)),
            [this, self, hs](boost::system::error_code ec, size_t)
            {
                onHandshakeSent(hs, ec);
            });
    }

    void onHandshakeSent(Handshake hs, boost::system::error_code ec)
    {
        if (!check(ec))
            return;
        if (!hs.hasError())
            complete(hs);
        else
            fail(hs.errorCode());
    }

    void complete(Handshake hs)
    {
        auto codecId = hs.codecId();
        const TransportInfo i{
            codecId,
            Handshake::byteLengthOf(data_->maxTxLength),
            Handshake::byteLengthOf(data_->settings.maxRxLength())};
        Base::completeAccept(i);
        data_->handler(codecId);
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
        // TODO: Check if post is needed
        Base::post(data_->handler, makeUnexpected(ec));
        data_.reset();
        Base::socket().close();
        Base::shutdown();
    }

    template <typename TErrc>
    void fail(TErrc errc) {fail(make_error_code(errc));}

    std::unique_ptr<Data> data_; // Only used once for accept operation
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_RAWSOCKTRANSPORT_HPP
