/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_RAWSOCKHEADER_HPP
#define CPPWAMP_INTERNAL_RAWSOCKHEADER_HPP

#include <cassert>
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

    bool frameKindIsValid() const
    {
        auto kind = frameKind();
        return kind == TransportFrameKind::wamp ||
               kind == TransportFrameKind::ping ||
               kind == TransportFrameKind::pong;
    }

    TransportFrameKind frameKind() const
    {
        return get<TransportFrameKind>(frameKindMask_, frameKindPos_);
    }

    std::size_t length() const
    {
        std::size_t n = get<std::size_t>(lengthMask_);
        if ((hdr_ & extraLengthBit_) != 0)
            n += extraLength_;
        return n;
    }

    uint32_t toBigEndian() const {return endian::nativeToBig32(hdr_);}

    uint32_t toHostOrder() const {return hdr_;}

    RawsockHeader& setFrameKind(TransportFrameKind kind)
    {
        put(kind, frameKindPos_);
        return *this;
    }

    RawsockHeader& setLength(std::size_t length)
    {
        assert(length <= lengthHardLimit_);
        if (length >= extraLength_)
        {
            put(length % extraLength_, lengthPos_);
            hdr_ |= extraLengthBit_;
        }
        else
        {
            put(length, lengthPos_);
        }
        return *this;
    }

private:
    static constexpr std::size_t lengthHardLimit_ = 32*1024*1024 - 1;
    static constexpr std::size_t extraLength_ = 16*1024*1024;
    static constexpr uint32_t extraLengthBit_ = 0x08000000;
    static constexpr uint32_t frameKindMask_  = 0x07000000;
    static constexpr uint32_t lengthMask_     = 0x00ffffff;
    static constexpr int frameKindPos_ = 24;
    static constexpr int lengthPos_ = 0;

    template <typename T = uint32_t>
    T get(uint32_t mask, int pos = 0) const
    {
        return static_cast<T>((hdr_ & mask) >> pos);
    }

    template <typename T>
    void put(T value, int pos = 0)
    {
        hdr_ |= (static_cast<uint32_t>(value) << pos);
    }

    uint32_t hdr_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_RAWSOCKHEADER_HPP
