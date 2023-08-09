/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_PINGER_HPP
#define CPPWAMP_INTERNAL_PINGER_HPP

#include <array>
#include <cstdint>
#include <memory>
#include <boost/asio/steady_timer.hpp>
#include "../asiodefs.hpp"
#include "../errorcodes.hpp"
#include "../erroror.hpp"
#include "../messagebuffer.hpp"
#include "../transport.hpp"
#include "endian.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
using PingBytes = std::array<uint8_t, 2*sizeof(uint64_t)>;

//------------------------------------------------------------------------------
class PingFrame
{
public:
    explicit PingFrame(uint64_t randomId)
        : baseId_(endian::nativeToBig64(randomId))
    {}

    uint64_t count() const {return sequentialId_;}

    void serialize(PingBytes& bytes) const
    {
        auto* ptr = bytes.data();
        std::memcpy(ptr, &baseId_, sizeof(baseId_));
        ptr += sizeof(baseId_);
        uint64_t n = endian::nativeToBig64(sequentialId_);
        std::memcpy(ptr, &n, sizeof(sequentialId_));
    }

    void increment() {++sequentialId_;}

private:
    uint64_t baseId_ = 0;
    uint64_t sequentialId_ = 0;
};

//------------------------------------------------------------------------------
class Pinger : public std::enable_shared_from_this<Pinger>
{
public:
    using Handler = std::function<void (ErrorOr<PingBytes>)>;
    using Byte = MessageBuffer::value_type;

    Pinger(IoStrand strand, const TransportInfo& info)
        : timer_(strand),
          frame_(info.transportId()),
          interval_(info.heartbeatInterval())
    {}

    void start(Handler handler)
    {
        handler_ = std::move(handler);
        startTimer();
    }

    void stop()
    {
        handler_ = nullptr;
        interval_ = {};
        timer_.cancel();
    }

    void pong(const Byte* bytes, std::size_t length)
    {
        if (frame_.count() == 0 || length != frameBytes_.size())
            return;

        auto cmp = std::memcmp(bytes, frameBytes_.data(), length);
        if (cmp == 0)
            matchingPongReceived_ = true;
    }

private:
    void startTimer()
    {
        std::weak_ptr<Pinger> self = shared_from_this();
        timer_.expires_after(interval_);
        timer_.async_wait(
            [self, this](boost::system::error_code ec)
            {
                static constexpr auto cancelled =
                    boost::asio::error::operation_aborted;
                auto me = self.lock();

                if (!me || ec == cancelled || !handler_)
                    return;

                if (ec)
                {
                    handler_(makeUnexpected(static_cast<std::error_code>(ec)));
                    return;
                }

                if ((frame_.count() > 0) && !matchingPongReceived_)
                {
                    handler_(makeUnexpectedError(
                        TransportErrc::heartbeatTimeout));
                    return;
                }

                matchingPongReceived_ = false;
                frame_.increment();
                frame_.serialize(frameBytes_);
                handler_(frameBytes_);
                startTimer();
            });
    }

    boost::asio::steady_timer timer_;
    Handler handler_;
    PingFrame frame_;
    PingBytes frameBytes_ = {};
    Timeout interval_ = {};
    bool matchingPongReceived_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_PINGER_HPP
