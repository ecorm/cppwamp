/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_BASE64_HPP
#define CPPWAMP_BASE64_HPP

#include <array>
#include <cassert>
#include <cstdint>
#include <vector>
#include "../config.hpp"
#include "../error.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <bool urlSafe, bool withPadding, bool paddingExpected>
class BasicBase64
{
public:
    template <typename TSink>
    static void encode(const void* data, std::size_t size, TSink& sink)
    {
        Quad quad;
        unsigned quadSize = 4;
        Byte sextet;
        auto byte = static_cast<const Byte*>(data);
        auto end = byte + size;
        while (byte != end)
        {
            quad[0] = charFromSextet( (*byte >> 2) & 0x3f );
            sextet = (*byte << 4) & 0x30;
            ++byte;

            if (byte != end)
            {
                sextet |= (*byte >> 4) & 0x0f;
                quad[1] = charFromSextet(sextet);
                sextet = (*byte << 2) & 0x3c;
                ++byte;

                if (byte != end)
                {
                    sextet |= (*byte >> 6) & 0x03;
                    quad[2] = charFromSextet(sextet);
                    quad[3] = charFromSextet( *byte & 0x3f );
                    if (!withPadding)
                        quadSize = 4;
                    ++byte;
                }
                else
                {
                    quad[2] = charFromSextet(sextet);
                    quad[3] = pad;
                    if (!withPadding)
                        quadSize = 3;
                }
            }
            else
            {
                quad[1] = charFromSextet(sextet);
                quad[2] = pad;
                quad[3] = pad;
                if (!withPadding)
                    quadSize = 2;
            }

            using SinkByte = typename TSink::value_type;
            auto ptr = reinterpret_cast<const SinkByte*>(quad.data());
            sink.append(ptr, quadSize);
        }
    }

    template <typename TOutputByteContainer>
    CPPWAMP_NODISCARD static std::error_code
    decode(const void* data, size_t length, TOutputByteContainer& output)
    {
        if (length == 0)
            return {};
        if (paddingExpected && (length % 4) != 0)
            return make_error_code(DecodingErrc::badBase64Length);

        auto str = static_cast<const char*>(data);
        const char* ptr = str;
        auto end = str + length;
        while (end - ptr > 4)
        {
            auto errc = decodeFullQuad(ptr, output);
            if (errc != DecodingErrc::success)
                return make_error_code(errc);
            ptr += 4;
        }

        if (end - ptr < 2)
            return make_error_code(DecodingErrc::badBase64Length);

        Quad lastQuad;
        lastQuad.fill(+pad);
        auto iter = lastQuad.begin();
        while (ptr != end)
        {
            *iter = *ptr;
            ++ptr;
            ++iter;
        }

        auto errc = decodeLastQuad(lastQuad, output);
        if (errc != DecodingErrc::success)
            return make_error_code(errc);

        return {};
    }

private:
    using Byte    = uint8_t;
    using Triplet = std::array<Byte, 3>;
    using Quad    = std::array<char, 4>;

    static constexpr char pad = '=';

    static char charFromSextet(uint8_t sextet)
    {
        static constexpr char c62 = urlSafe ? '-' : '+';
        static constexpr char c63 = urlSafe ? '_' : '/';

        static const char alphabet[] =
        {
            'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', // 0-7
            'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', // 8-15
            'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', // 16-23
            'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', // 24-31
            'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', // 32-39
            'o', 'p', 'q', 'r', 's', 't', 'u', 'v', // 40-47
            'w', 'x', 'y', 'z', '0', '1', '2', '3', // 48-55
            '4', '5', '6', '7', '8', '9', c62, c63  // 56-63
        };
        assert(sextet < sizeof(alphabet));
        return alphabet[sextet];
    }

    template <typename TOutputByteContainer>
    static DecodingErrc decodeFullQuad(const char* quad,
                                       TOutputByteContainer& out)
    {
        Triplet triplet;
        auto errc = tripletFromQuad(quad, false, triplet);
        if (errc != DecodingErrc::success)
            return errc;
        append(triplet, triplet.size(), out);
        return DecodingErrc::success;
    }

    template <typename TOutputByteContainer>
    static DecodingErrc decodeLastQuad(Quad quad, TOutputByteContainer& out)
    {
        unsigned remaining = 1;
        if (quad[0] == pad || quad[1] == pad)
            return DecodingErrc::badBase64Padding;
        if (quad[2] != pad)
        {
            remaining = (quad[3] == pad) ? 2 : 3;
        }
        else if (quad[3] != pad)
        {
            return DecodingErrc::badBase64Padding;
        }

        Triplet triplet;
        auto errc = tripletFromQuad(quad.data(), true, triplet);
        if (errc != DecodingErrc::success)
            return errc;

        append(triplet, remaining, out);
        return DecodingErrc::success;
    }

    static DecodingErrc tripletFromQuad(const char* quad, bool last,
                                        Triplet& triplet)
    {
        std::array<Byte, 4> sextets;
        for (unsigned i=0; i<4; ++i)
        {
            Byte s;
            auto errc = sextetFromChar(quad[i], last, s);
            if (errc != DecodingErrc::success)
                return errc;
            sextets[i] = s;
        }

        triplet[0] = ((sextets[0] << 2) & 0xfc) | ((sextets[1] >> 4) & 0x03);
        triplet[1] = ((sextets[1] << 4) & 0xf0) | ((sextets[2] >> 2) & 0x0f);
        triplet[2] = ((sextets[2] << 6) & 0xc0) | ((sextets[3]     ) & 0x3f);

        return DecodingErrc::success;
    }

    static DecodingErrc sextetFromChar(char c, bool padAllowedHere,
                                       Byte& sextet)
    {
        static constexpr Byte s43 = urlSafe ? 0xff : 0x3e;
        static constexpr Byte s45 = urlSafe ? 0x3e : 0xff;
        static constexpr Byte s47 = urlSafe ? 0xff : 0x3f;
        static constexpr Byte s95 = urlSafe ? 0x3f : 0xff;

        static const Byte table[] =
        {
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 0-7
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 8-15
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 16-23
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 24-31
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 32-39

        //                     '+'         '-'         '/'
            0xff, 0xff, 0xff,  s43, 0xff,  s45, 0xff,  s47, // 40-47

        //  '0'   '1'   '2'   '3'   '4'   '5'   '6'   '7'
            0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, // 48-55

        //  '8'   '9'                     '='
            0x3c, 0x3d, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, // 56-63

        //        'A'   'B'   'C'   'D'   'E'   'F'   'G'
            0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // 64-71

        //  'H'   'I'   'J'   'K'   'L'   'M'   'N'   'O'
            0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, // 72-79

        //  'P'   'Q'   'R'   'S'   'T'   'U'   'V'   'W'
            0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, // 80-87

        //  'X'   'Y'   'Z'                            '_'
            0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff,  s95, // 88-95

        //        'a'   'b'   'c'   'd'   'e'   'f'   'g'
            0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, // 96-103

        //  'h'   'i'   'j'   'k'   'l'   'm'   'n'   'o'
            0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, // 104-111

        //  'p'   'q'   'r'   's'   't'   'u'   'v'   'w'
            0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, // 112-119

        //  'x'   'y'   'z'
            0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff, // 120-127

            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 128-135
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 136-143
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 144-151
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 152-159
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 160-167
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 168-175
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 176-183
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 184-191
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 192-199
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 200-207
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 208-215
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 216-223
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 224-231
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 232-239
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 240-247
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff  // 248-255
        };

        if (!padAllowedHere && c == pad)
            return DecodingErrc::badBase64Padding;
        uint8_t index = c;
        sextet = table[index];
        if (sextet == 0xff)
            return DecodingErrc::badBase64Char;
        assert(sextet < 64);
        return DecodingErrc::success;
    }

    template <typename TOutputByteContainer>
    static void append(Triplet triplet, size_t length,
                       TOutputByteContainer& output)
    {
        using OutputByte = typename TOutputByteContainer::value_type;
        auto data = reinterpret_cast<const OutputByte*>(triplet.data());
        output.insert(output.end(), data, data + length);
    }
};

using Base64 = BasicBase64<false, true, false>;
using Base64Url = BasicBase64<true, false, false>;

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_BASE64_HPP

