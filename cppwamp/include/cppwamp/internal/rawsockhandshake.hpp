/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_RAWSOCKHANDSHAKE_HPP
#define CPPWAMP_INTERNAL_RAWSOCKHANDSHAKE_HPP

#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include "../errorcodes.hpp"
#include "endian.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class RawsockHandshake
{
public:
    static RawsockHandshake fromBigEndian(uint32_t big)
    {
        return RawsockHandshake(endian::bigToNative32(big));
    }

    static RawsockHandshake eUnsupportedFormat()
    {
        return RawsockHandshake(magicOctet_ | eUnsupportedFormatBits_);
    }

    static RawsockHandshake eUnacceptableLimit()
    {
        return RawsockHandshake(magicOctet_ | eUnacceptableLimitBits_);
    }

    static RawsockHandshake eReservedBitsUsed()
    {
        return RawsockHandshake(magicOctet_ | eReservedBitsUsedBits_);
    }

    static RawsockHandshake eMaxConnections()
    {
        return RawsockHandshake(magicOctet_ | eMaxConnectionsBits_);
    }

    RawsockHandshake() : hs_(magicOctet_) {}

    explicit RawsockHandshake(uint32_t hostOrder) : hs_(hostOrder) {}

    uint16_t reserved() const {return get<uint16_t>(reservedMask_);}

    int codecId() const {return get<int>(codecMask_, codecPos_);}

    size_t sizeLimit() const
    {
        auto bits = get<unsigned>(limitMask_, limitPos_);
        auto limit = 1u << (bits + limitBase_);
        if (bits == maxLimitBits)
            --limit;
        return limit;
    }

    bool hasError() const {return get<>(codecMask_) == 0;}

    TransportErrc errorCode() const
    {
        using TE = TransportErrc;
        static const std::array<TransportErrc, 16> table{
        {
            TE::success, TE::badSerializer, TE::badLengthLimit, TE::badFeature,
            TE::shedded, TE::failed, TE::failed, TE::failed,
            TE::failed, TE::failed, TE::failed, TE::failed,
            TE::failed, TE::failed, TE::failed, TE::failed
        }};
        auto code = get<>(errorMask_, errorPos_);
        return table.at(code);
    }

    bool hasMagicOctet() const {return get<>(magicMask_) == magicOctet_;}

    uint32_t toBigEndian() const {return endian::nativeToBig32(hs_);}

    uint32_t toHostOrder() const {return hs_;}

    RawsockHandshake& setCodecId(int codecId)
        {return put(codecId, codecPos_);}

    RawsockHandshake& setSizeLimit(std::size_t limit)
    {
        return put(sizeLimitToBits(limit), limitPos_);
    }

private:
    static constexpr uint32_t reservedMask_           = 0x0000ffff;
    static constexpr uint32_t codecMask_              = 0x000f0000;
    static constexpr uint32_t limitMask_              = 0x00f00000;
    static constexpr uint32_t errorMask_              = 0x00f00000;
    static constexpr uint32_t magicMask_              = 0xff000000;
    static constexpr uint32_t magicOctet_             = 0x7f000000;
    static constexpr uint32_t eUnsupportedFormatBits_ = 0x00100000;
    static constexpr uint32_t eUnacceptableLimitBits_ = 0x00200000;
    static constexpr uint32_t eReservedBitsUsedBits_  = 0x00300000;
    static constexpr uint32_t eMaxConnectionsBits_    = 0x00400000;
    static constexpr int codecPos_  = 16;
    static constexpr int limitPos_  = 20;
    static constexpr int errorPos_  = 20;
    static constexpr int limitBase_ = 9; // 2^9=512 bytes minimum limit
    static constexpr unsigned maxLimitBits = 0x0F;

    static unsigned sizeLimitToBits(std::size_t size)
    {
        // The WAMP raw sockets message limit starts at 512 bytes and
        // increases by powers of two up to 16MiB - 1B.

        // If desired limit exceeds 16MiB - 1B, clamp the handshake limit to
        // 16MiB - 1B.
        if (size > 8*1024*1024)
            return 0x0F;

        // Compute the handshake limit bits that match or exceed the
        // desired limit.
        uint_fast32_t limit = 512;
        unsigned n = 0;
        while (n < 0x0F)
        {
            if (size <= limit)
                break;
            ++n;
            limit *= 2;
        }

        return n;
    }

    template <typename T = uint32_t>
    T get(uint32_t mask, int pos = 0) const
        {return static_cast<T>((hs_ & mask) >> pos);}

    template <typename T>
    RawsockHandshake& put(T value, int pos = 0)
        {hs_ |= (static_cast<uint32_t>(value) << pos); return *this;}

    uint32_t hs_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_RAWSOCKHANDSHAKE_HPP
