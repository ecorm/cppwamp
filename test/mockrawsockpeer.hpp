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

    void load(std::vector<Frame> frames)
    {
        frames_ = std::move(frames);
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
        if (!frames_.empty())
            writeFrame();
    }

    void close() {socket_.close();}

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
        if (++frameIndex_ < frames_.size())
            writeFrame();
    }

    Resolver resolver_;
    Socket socket_;
    std::vector<Frame> frames_;
    Frame::Payload buffer_;
    unsigned frameIndex_ = 0;
    Handshake handshake_ = 0;
    Frame::Header header_ = 0;
    uint16_t port_ = 0;
};

} // namespace test

#endif // CPPWAMP_TEST_MOCKRAWSOCKPEER_HPP
