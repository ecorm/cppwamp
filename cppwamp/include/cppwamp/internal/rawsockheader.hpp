/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_RAWSOCKHEADER_HPP
#define CPPWAMP_INTERNAL_RAWSOCKHEADER_HPP

#include <cstddef>
#include <cstdint>
#include "endian.hpp"
#include "../wampdefs.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class RawsockHeader
{
public:
    static RawsockHeader fromBigEndian(uint32_t big)
        {return RawsockHeader(endian::bigToNative32(big));}

    RawsockHeader() : hdr_(0) {}

    explicit RawsockHeader(uint32_t hostOrder) : hdr_(hostOrder) {}

    bool msgTypeIsValid() const
    {
        auto kind = msgKind();
        return kind == TransportFrameKind::wamp ||
               kind == TransportFrameKind::ping ||
               kind == TransportFrameKind::pong;
    }

    TransportFrameKind msgKind() const
    {
        return get<TransportFrameKind>(msgKindMask_, msgKindPos_);
    }

    std::size_t length() const {return get<std::size_t>(lengthMask_);}

    uint32_t toBigEndian() const {return endian::nativeToBig32(hdr_);}

    uint32_t toHostOrder() const {return hdr_;}

    RawsockHeader& setMsgKind(TransportFrameKind kind)
    {
        return put(kind, msgKindPos_);
    }

    RawsockHeader& setLength(std::size_t length)
    {
        return put(length, lengthPos_);
    }

private:
    static constexpr uint32_t msgKindMask_ = 0xff000000;
    static constexpr uint32_t lengthMask_  = 0x00ffffff;
    static constexpr int msgKindPos_       = 24;
    static constexpr int lengthPos_        = 0;

    template <typename T = uint32_t>
    T get(uint32_t mask, int pos = 0) const
        {return static_cast<T>((hdr_ & mask) >> pos);}

    template <typename T>
    RawsockHeader& put(T value, int pos = 0)
        {hdr_ |= (static_cast<uint32_t>(value) << pos); return *this;}

    uint32_t hdr_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_RAWSOCKHEADER_HPP
