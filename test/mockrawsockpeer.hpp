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
#include <boost/asio/strand.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cppwamp/codec.hpp>
#include <cppwamp/wampdefs.hpp>
#include <cppwamp/internal/endian.hpp>
#include <cppwamp/internal/rawsockheader.hpp>
#include <cppwamp/internal/rawsockhandshake.hpp>

namespace test
{

//------------------------------------------------------------------------------
struct MockRawsockClientFrame
{
    using Payload = std::vector<uint8_t>;
    using FrameKind = wamp::TransportFrameKind;
    using Header = uint32_t;

    MockRawsockClientFrame(Payload p, FrameKind k = FrameKind::wamp)
        : MockRawsockClientFrame(std::move(p), computeHeader(p, k))
    {}

    MockRawsockClientFrame(Payload p, Header h)
        : payload(std::move(p)),
          header(wamp::internal::endian::nativeToBig32(h)) {}

    Payload payload;
    Header header;

private:
    static Header computeHeader(const Payload& p, FrameKind k)
    {
        return wamp::internal::RawsockHeader{}
            .setFrameKind(k).setLength(p.size()).toHostOrder();
    }
};

//------------------------------------------------------------------------------
class MockRawsockClient : public std::enable_shared_from_this<MockRawsockClient>
{
public:
    using Ptr = std::shared_ptr<MockRawsockClient>;
    using Handshake = uint32_t;
    using Frame = MockRawsockClientFrame;

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

    void load(std::vector<Frame> frames) {outFrames_ = std::move(frames);}

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
            writeFrame();
    }

    void close() {socket_.close();}

    const std::vector<Frame>& inFrames() const {return inFrames_;}

private:
    using Resolver = boost::asio::ip::tcp::resolver;
    using Socket = boost::asio::ip::tcp::socket;

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

    template <typename E>
    MockRawsockClient(E&& exec, uint16_t port, Handshake hs)
        : resolver_(boost::asio::make_strand(exec)),
          socket_(resolver_.get_executor()),
          handshake_(wamp::internal::endian::nativeToBig32(hs)),
          port_(port)
    {}

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

    void onHandshakeWritten()
    {
        handshake_ = 0;
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(&handshake_, sizeof(handshake_)),
            [](boost::system::error_code ec, std::size_t) {check(ec);});
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
            boost::asio::buffer(buffer_.data(), length),
            [this, self](boost::system::error_code ec, std::size_t)
            {
                if (check(ec))
                    onPayloadRead();
            });
    }

    void onPayloadRead()
    {
        auto kind =
            wamp::internal::RawsockHeader::fromBigEndian(header_).frameKind();
        inFrames_.emplace_back(buffer_, kind);
        if (++frameIndex_ < outFrames_.size())
            writeFrame();
    }

    Resolver resolver_;
    Socket socket_;
    std::vector<Frame> outFrames_;
    std::vector<Frame> inFrames_;
    Frame::Payload buffer_;
    unsigned frameIndex_ = 0;
    Handshake handshake_ = 0;
    Frame::Header header_ = 0;
    uint16_t port_ = 0;
};

//------------------------------------------------------------------------------
class MockRawsockSession
    : public std::enable_shared_from_this<MockRawsockSession>
{
public:
    using Ptr = std::shared_ptr<MockRawsockSession>;
    using Socket = boost::asio::ip::tcp::socket;
    using Handshake = uint32_t;
    using Frame = MockRawsockClientFrame;
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
            boost::asio::buffer(buffer_.data(), length),
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
                if (check(ec))
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
    using Frame = MockRawsockClientFrame;
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
