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
#include "../rawsockoptions.hpp"
#include "endian.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class RawsockHandshake
{
public:
    static size_t byteLengthOf(RawsockMaxLength len)
    {
        return 1u << (static_cast<int>(len) + byteLengthPos_);
    }

    static RawsockHandshake fromBigEndian(uint32_t big)
    {
        return RawsockHandshake(endian::bigToNative32(big));
    }

    static RawsockHandshake eUnsupportedFormat()
    {
        return RawsockHandshake(magicOctet_ | eUnsupportedFormatBits_);
    }

    static RawsockHandshake eUnacceptableLength()
    {
        return RawsockHandshake(magicOctet_ | eUnacceptableLengthBits_);
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

    int codecId() const
        {return get<int>(codecMask_, codecPos_);}

    RawsockMaxLength maxLength() const
        {return get<RawsockMaxLength>(lengthMask_, lengthPos_);}

    size_t maxLengthInBytes() const {return byteLengthOf(maxLength());}

    bool hasError() const {return get<>(codecMask_) == 0;}

    TransportErrc errorCode() const
    {
        using TE = TransportErrc;
        static const std::array<TransportErrc, 16> table{
        {
            TE::success, TE::badSerializer, TE::badLengthLimit, TE::badFeature,
             TE::overloaded, TE::failed, TE::failed, TE::failed,
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

    RawsockHandshake& setMaxLength(RawsockMaxLength length)
        {return put(length, lengthPos_);}

private:
    static constexpr uint32_t reservedMask_            = 0x0000ffff;
    static constexpr uint32_t codecMask_               = 0x000f0000;
    static constexpr uint32_t lengthMask_              = 0x00f00000;
    static constexpr uint32_t errorMask_               = 0x00f00000;
    static constexpr uint32_t magicMask_               = 0xff000000;
    static constexpr uint32_t magicOctet_              = 0x7f000000;
    static constexpr uint32_t eUnsupportedFormatBits_  = 0x00100000;
    static constexpr uint32_t eUnacceptableLengthBits_ = 0x00200000;
    static constexpr uint32_t eReservedBitsUsedBits_   = 0x00300000;
    static constexpr uint32_t eMaxConnectionsBits_     = 0x00400000;
    static constexpr int byteLengthPos_ = 9;
    static constexpr int codecPos_      = 16;
    static constexpr int lengthPos_     = 20;
    static constexpr int errorPos_      = 20;

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
