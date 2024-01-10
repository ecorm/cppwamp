/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TEST_MOCKRAWSOCKPEER_HPP
#define CPPWAMP_TEST_MOCKRAWSOCKPEER_HPP

#include <array>
#include <cstdint>
#include <memory>
#include <system_error>
#include <vector>
#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cppwamp/codec.hpp>
#include <cppwamp/timeout.hpp>
#include <cppwamp/wampdefs.hpp>
#include <cppwamp/internal/endian.hpp>
#include <cppwamp/internal/rawsockheader.hpp>
#include <cppwamp/internal/rawsockhandshake.hpp>

namespace test
{

//------------------------------------------------------------------------------
struct MockRawsockFrame
{
    using Payload = std::string;
    using FrameKind = wamp::TransportFrameKind;
    using Header = uint32_t;
    using Timeout = wamp::Timeout;

    MockRawsockFrame(Payload p, FrameKind k = FrameKind::wamp,
                     Timeout delay = Timeout{0})
        : MockRawsockFrame(std::move(p), computeHeader(p, k, p.size()), delay)
    {}

    MockRawsockFrame(Payload p, FrameKind k, std::size_t length)
        : MockRawsockFrame(std::move(p), computeHeader(p, k, length),
                           Timeout{0})
    {}

    MockRawsockFrame(Payload p, Header h, Timeout delay = Timeout{0})
        : payload(std::move(p)),
          header(wamp::internal::endian::nativeToBig32(h)),
          delay(delay)
    {}

    Payload payload;
    Header header;
    Timeout delay;
    std::size_t readLimit = 0; // Used to stall reading of the payload

private:
    static Header computeHeader(const Payload& p, FrameKind k,
                                std::size_t length)
    {
        return wamp::internal::RawsockHeader{}
            .setFrameKind(k).setLength(length).toHostOrder();
    }
};

//------------------------------------------------------------------------------
class MockRawsockClient : public std::enable_shared_from_this<MockRawsockClient>
{
public:
    using Ptr = std::shared_ptr<MockRawsockClient>;
    using Handshake = uint32_t;
    using Frame = MockRawsockFrame;

    template <typename E>
    static Ptr create(E&& exec, uint16_t port)
    {
        auto handshake =
            wamp::internal::RawsockHandshake{}
                .setCodecId(wamp::KnownCodecIds::json())
                .setSizeLimit(64*1024)
                .toHostOrder();
        return create(std::forward<E>(exec), port, handshake);
    }

    template <typename E>
    static Ptr create(E&& exec, uint16_t port, Handshake hs)
    {
        return Ptr(new MockRawsockClient(std::forward<E>(exec), port, hs));
    }

    void clear()
    {
        outFrames_.clear();
        inFrames_.clear();
        readError_.clear();
        frameIndex_ = 0;
        connected_ = false;
        inhibitHandshake_ = false;
        inhibitLingeringClose_ = false;
    }

    void load(std::vector<Frame> frames) {outFrames_ = std::move(frames);}

    void inhibitHandshake(bool inhibited = true)
    {
        inhibitHandshake_ = inhibited;
    }

    void inhibitLingeringClose(bool inhibited = true)
    {
        inhibitLingeringClose_ = inhibited;
    }

    void setHandshake(Handshake hs)
    {
        handshake_ = wamp::internal::endian::nativeToBig32(hs);
    }

    void connect()
    {
        auto self = shared_from_this();
        resolver_.async_resolve(
            "localhost",
            std::to_string(port_),
            [this, self](boost::system::error_code ec,
                         Resolver::results_type endpoints)
            {
                if (check(ec))
                    onResolved(endpoints);
            });
    }

    void start()
    {
        if (!outFrames_.empty())
            delayAndWriteFrame();
    }

    void close()
    {
        socket_.close();
        connected_ = false;
    }

    bool connected() const {return connected_;}

    const std::vector<Frame>& inFrames() const {return inFrames_;}

    boost::system::error_code readError() const {return readError_;}

    Handshake peerHandshake() const {return peerHandshake_;}

private:
    using Resolver = boost::asio::ip::tcp::resolver;
    using Socket = boost::asio::ip::tcp::socket;

    template <typename E>
    MockRawsockClient(E&& exec, uint16_t port, Handshake hs)
        : resolver_(boost::asio::make_strand(exec)),
          socket_(resolver_.get_executor()),
          timer_(resolver_.get_executor()),
          handshake_(wamp::internal::endian::nativeToBig32(hs)),
          port_(port)
    {}

    bool check(boost::system::error_code ec, bool reading = true)
    {
        if (!ec)
            return true;

        if (reading)
            readError_ = ec;

        namespace error = boost::asio::error;
        if (ec == error::eof ||
            ec == error::operation_aborted ||
            ec == error::connection_reset )
        {
            return false;
        }

        throw std::system_error{ec};
        return false;
    }

    void onResolved(const Resolver::results_type& endpoints)
    {
        auto self = shared_from_this();
        boost::asio::async_connect(
            socket_,
            endpoints,
            [this, self](boost::system::error_code ec, Socket::endpoint_type)
            {
                if (check(ec))
                    onConnected();
            });
    }

    void onConnected()
    {
        connected_ = true;

        if (inhibitHandshake_)
            return flush();

        auto self = shared_from_this();
        boost::asio::async_write(
            socket_,
            boost::asio::const_buffer(&handshake_, sizeof(handshake_)),
            [this, self](boost::system::error_code ec, std::size_t)
            {
                if (check(ec))
                    onHandshakeWritten();
            });
    }

    void flush()
    {
        buffer_.clear();
        auto self = shared_from_this();

        boost::asio::async_read(
            socket_,
            boost::asio::dynamic_buffer(buffer_),
            [this, self](boost::system::error_code ec, std::size_t)
            {
                if (check(ec))
                {
                    flush();
                }
                else if (ec == boost::asio::error::eof)
                {
                    if (!inhibitLingeringClose_)
                        socket_.close();
                }
                else
                {
                    socket_.close();
                }
            });
    }

    void onHandshakeWritten()
    {
        peerHandshake_ = 0;
        auto self = shared_from_this();

        boost::asio::async_read(
            socket_,
            boost::asio::buffer(&peerHandshake_, sizeof(peerHandshake_)),
            [this, self](boost::system::error_code ec, std::size_t)
            {
                check(ec);
            });
    }

    void delayAndWriteFrame()
    {
        const auto& frame = outFrames_.at(frameIndex_);
        if (frame.delay.count() == 0)
            return writeFrame();

        std::weak_ptr<MockRawsockClient> self = shared_from_this();
        timer_.expires_from_now(frame.delay);
        timer_.async_wait(
            [this, self](boost::system::error_code ec)
            {
                if (ec == boost::asio::error::operation_aborted)
                    return;
                auto me = self.lock();
                if (!me)
                    return;
                if (ec)
                    throw std::system_error(ec);
                writeFrame();
            });
    }

    void writeFrame()
    {
        const auto& frame = outFrames_.at(frameIndex_);
        std::array<boost::asio::const_buffer, 2> buffers{{
            {&frame.header, sizeof(frame.header)},
            {frame.payload.data(), frame.payload.size()}}};
        auto self = shared_from_this();
        boost::asio::async_write(
            socket_,
            buffers,
            [this, self](boost::system::error_code ec, std::size_t)
            {
                if (check(ec, false))
                    readHeader();
            });
    }

    void readHeader()
    {
        auto self = shared_from_this();
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(&header_, sizeof(header_)),
            [this, self](boost::system::error_code ec, std::size_t)
            {
                if (check(ec))
                    readPayload();
            });
    }

    void readPayload()
    {
        auto length =
            wamp::internal::RawsockHeader::fromBigEndian(header_).length();
        auto limit = outFrames_.at(frameIndex_).readLimit;
        bool stalled = limit != 0;
        length = stalled ? limit : length;

        buffer_.resize(length);
        auto self = shared_from_this();
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(&(buffer_.front()), length),
            [this, self, stalled](boost::system::error_code ec, std::size_t)
            {
                if (check(ec) && !stalled)
                    onPayloadRead();
            });
    }

    void onPayloadRead()
    {
        auto kind =
            wamp::internal::RawsockHeader::fromBigEndian(header_).frameKind();
        inFrames_.emplace_back(buffer_, kind);
        if (++frameIndex_ < outFrames_.size())
            delayAndWriteFrame();
        else
            flush();
    }

    Resolver resolver_;
    Socket socket_;
    boost::asio::steady_timer timer_;
    std::vector<Frame> outFrames_;
    std::vector<Frame> inFrames_;
    Frame::Payload buffer_;
    boost::system::error_code readError_;
    unsigned frameIndex_ = 0;
    Handshake handshake_ = 0;
    Handshake peerHandshake_ = 0;
    Frame::Header header_ = 0;
    uint16_t port_ = 0;
    bool inhibitHandshake_ = false;
    bool inhibitLingeringClose_ = false;
    bool connected_;
};

//------------------------------------------------------------------------------
class MockRawsockSession
    : public std::enable_shared_from_this<MockRawsockSession>
{
public:
    using Ptr = std::shared_ptr<MockRawsockSession>;
    using Socket = boost::asio::ip::tcp::socket;
    using Handshake = uint32_t;
    using Frame = MockRawsockFrame;
    using Payload = Frame::Payload;

    MockRawsockSession(Socket&& socket, std::vector<Frame> frames, Handshake hs)
        : socket_(std::move(socket)),
          frames_(std::move(frames)),
          handshake_(hs)
    {}

    void start() {readHandshake();}

    void setPong(Payload canned) {cannedPong_ = canned;}

    void close() {socket_.close();}

    const std::vector<Payload>& pings() const {return pings_;}

private:
    static Handshake makeDefaultHandshake()
    {
        return wamp::internal::RawsockHandshake{}
            .setCodecId(wamp::KnownCodecIds::json())
            .setSizeLimit(64*1024)
            .toHostOrder();
    }

    static bool check(boost::system::error_code ec)
    {
        if (!ec)
            return true;

        namespace error = boost::asio::error;
        if (ec == error::eof ||
            ec == error::operation_aborted ||
            ec == error::connection_reset )
        {
            return false;
        }

        throw std::system_error{ec};
        return false;
    }

    void readHandshake()
    {
        auto self = shared_from_this();
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(&peerHandshake_, sizeof(peerHandshake_)),
            [this, self](boost::system::error_code ec, std::size_t)
            {
                if (check(ec))
                    writeHandshake();
            });
    }

    void writeHandshake()
    {
        auto self = shared_from_this();
        boost::asio::async_write(
            socket_,
            boost::asio::const_buffer{&handshake_, sizeof(handshake_)},
            [this, self](boost::system::error_code ec, std::size_t)
            {
                if (check(ec))
                    readHeader();
            });
    }

    void readHeader()
    {
        auto self = shared_from_this();
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(&header_, sizeof(header_)),
            [this, self](boost::system::error_code ec, std::size_t)
            {
                if (check(ec))
                    readPayload();
            });
    }

    void readPayload()
    {
        auto length =
            wamp::internal::RawsockHeader::fromBigEndian(header_).length();
        buffer_.resize(length);
        auto self = shared_from_this();
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(&(buffer_.front()), length),
            [this, self](boost::system::error_code ec, std::size_t)
            {
                if (check(ec))
                    onPayloadRead();
            });
    }

    void onPayloadRead()
    {
        auto hdr = wamp::internal::RawsockHeader::fromBigEndian(header_);
        if (hdr.frameKind() == wamp::TransportFrameKind::ping)
            return sendPong();
        if (hdr.frameKind() == wamp::TransportFrameKind::pong)
            return readHeader();
        writeFrame();
    }

    void sendPong()
    {
        pings_.push_back(buffer_);
        Payload* pong = cannedPong_.empty() ? &buffer_ : &cannedPong_;

        header_ = wamp::internal::RawsockHeader{}
                      .setFrameKind(wamp::TransportFrameKind::pong)
                      .setLength(buffer_.size())
                      .toBigEndian();
        std::array<boost::asio::const_buffer, 2> buffers{{
            {&header_, sizeof(header_)},
            {pong->data(), pong->size()} }};
        auto self = shared_from_this();
        boost::asio::async_write(
            socket_,
            buffers,
            [this, self](boost::system::error_code ec, std::size_t)
            {
                if (check(ec))
                    readHeader();
            });
    }

    void writeFrame()
    {
        if (frameIndex_ >= frames_.size())
            return readHeader();

        const auto& frame = frames_.at(frameIndex_);
        std::array<boost::asio::const_buffer, 2> buffers{{
            {&frame.header, sizeof(frame.header)},
            {frame.payload.data(), frame.payload.size()}}};
        auto self = shared_from_this();
        boost::asio::async_write(
            socket_,
            buffers,
            [this, self](boost::system::error_code ec, std::size_t)
            {
                ++frameIndex_;
                if (check(ec) && socket_.is_open())
                    readHeader();
            });
    }

    Socket socket_;
    std::vector<Frame> frames_;
    std::vector<Payload> pings_;
    Payload buffer_;
    Payload cannedPong_;
    unsigned frameIndex_ = 0;
    Handshake peerHandshake_ = 0;
    Handshake handshake_ = 0;
    Frame::Header header_ = 0;
};

//------------------------------------------------------------------------------
class MockRawsockServer : public std::enable_shared_from_this<MockRawsockServer>
{
public:
    using Ptr = std::shared_ptr<MockRawsockServer>;
    using Handshake = uint32_t;
    using Frame = MockRawsockFrame;
    using SessionList = std::vector<std::weak_ptr<MockRawsockSession>>;

    template <typename E>
    static Ptr create(E&& exec, uint16_t port)
    {
        auto handshake =
            wamp::internal::RawsockHandshake{}
                .setCodecId(wamp::KnownCodecIds::json())
                .setSizeLimit(64*1024)
                .toHostOrder();
        return create(std::forward<E>(exec), port, handshake);
    }

    template <typename E>
    static Ptr create(E&& exec, uint16_t port, Handshake hs)
    {
        return Ptr(new MockRawsockServer(std::forward<E>(exec), port, hs));
    }

    void load(std::vector<Frame> frames) {frames_ = std::move(frames);}

    void start() {accept();}

    void close()
    {
        acceptor_.close();
        for (auto s: sessions_)
        {
            auto session = s.lock();
            if (session)
                session->close();
        }
    }

    const SessionList& sessions() const {return sessions_;}

private:
    using Acceptor = boost::asio::ip::tcp::acceptor;
    using Socket = boost::asio::ip::tcp::socket;
    using Endpoint = boost::asio::ip::tcp::endpoint;

    static bool check(boost::system::error_code ec)
    {
        if (!ec)
            return true;

        namespace error = boost::asio::error;
        if (ec == error::eof ||
            ec == error::operation_aborted ||
            ec == error::connection_reset )
        {
            return false;
        }

        throw std::system_error{ec};
        return false;
    }

    static Endpoint makeEndpoint(uint16_t port)
    {
        return Endpoint{boost::asio::ip::tcp::v4(), port};
    }

    template <typename E>
    MockRawsockServer(E&& exec, uint16_t port, Handshake hs)
        : acceptor_(std::forward<E>(exec),
                    Endpoint{boost::asio::ip::tcp::v4(), port}),
          handshake_(wamp::internal::endian::nativeToBig32(hs))
    {}

    void accept()
    {
        auto self = shared_from_this();
        acceptor_.async_accept(
            [this, self](boost::system::error_code ec, Socket socket)
            {
                if (!check(ec))
                    return;
                auto session = std::make_shared<MockRawsockSession>(
                    std::move(socket), frames_, handshake_);
                sessions_.push_back(session);
                session->start();
                accept();
            });
    }

    Acceptor acceptor_;
    std::vector<Frame> frames_;
    SessionList sessions_;
    Frame::Payload buffer_;
    Handshake handshake_ = 0;
};

} // namespace test

#endif // CPPWAMP_TEST_MOCKRAWSOCKPEER_HPP
