/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_BASE64_HPP
#define CPPWAMP_BASE64_HPP

#include <array>
#include <cassert>
#include <cstdint>
#include <vector>
#include "../codec.hpp"
#include "config.hpp"

namespace wamp
{

namespace internal
{

class Base64
{
public:
    using Byte    = uint8_t;
    using ByteVec = std::vector<uint8_t>;

    template <typename TBuffer>
    static void encode(const ByteVec& data, TBuffer& out)
    {
        Quad quad;
        Byte sextet;
        auto byte = data.cbegin();
        while (byte != data.cend())
        {
            quad[0] = charFromSextet( (*byte >> 2) & 0x3f );
            sextet = (*byte << 4) & 0x30;
            ++byte;

            if (byte != data.cend())
            {
                sextet |= (*byte >> 4) & 0x0f;
                quad[1] = charFromSextet(sextet);
                sextet = (*byte << 2) & 0x3c;
                ++byte;

                if (byte != data.cend())
                {
                    sextet |= (*byte >> 6) & 0x03;
                    quad[2] = charFromSextet(sextet);
                    quad[3] = charFromSextet( *byte & 0x3f );
                    ++byte;
                }
                else
                {
                    quad[2] = charFromSextet(sextet);
                    quad[3] = pad;
                }
            }
            else
            {
                quad[1] = charFromSextet(sextet);
                quad[2] = pad;
                quad[3] = pad;
            }

            out.write(quad.data(), quad.size());
        }
    }

    static void decode(const char* str, size_t length, ByteVec& data)
    {
        if (length == 0)
            return;
        if (length % 4 != 0)
            throw error::Decode("Invalid JSON Base64 payload length");

        Triplet triplet;
        const char* quad = str;
        auto end = str + length - 4;
        assert(end >= str);
        for (; quad < end; quad += 4)
        {
            triplet = tripletFromQuad(quad, false);
            data.insert(data.end(), triplet.begin(), triplet.end());
        }

        assert((quad + 4) == (str + length));

        unsigned lastTripletCount = 1;
        triplet = tripletFromQuad(quad, true);
        if (quad[0] == pad || quad[1] == pad)
            throw error::Decode("Invalid JSON Base64 padding");

        if (quad[2] == pad)
        {
            if (quad[3] != pad)
                throw error::Decode("Invalid JSON Base64 padding");
        }
        else
            lastTripletCount = (quad[3] == pad) ? 2 : 3;

        data.insert(data.end(), triplet.begin(),
                    triplet.begin() + lastTripletCount);
    }

private:
    using Triplet = std::array<Byte, 3>;
    using Quad    = std::array<char, 4>;

    static constexpr char pad = '=';

    static char charFromSextet(uint8_t sextet)
    {
        static const char alphabet[] =
        {
            'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', // 0-7
            'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', // 8-15
            'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', // 16-23
            'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', // 24-31
            'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', // 32-39
            'o', 'p', 'q', 'r', 's', 't', 'u', 'v', // 40-47
            'w', 'x', 'y', 'z', '0', '1', '2', '3', // 48-55
            '4', '5', '6', '7', '8', '9', '+', '/'  // 56-63
        };
        assert(sextet < sizeof(alphabet));
        return alphabet[sextet];
    }

    static Triplet tripletFromQuad(const char* quad, bool padAllowed)
    {
        std::array<Byte, 4> sextet;
        for (unsigned i=0; i<4; ++i)
            sextet[i] = sextetFromChar(quad[i], padAllowed);
        Triplet triplet;
        triplet[0] = ((sextet[0] << 2) & 0xfc) | ((sextet[1] >> 4) & 0x03);
        triplet[1] = ((sextet[1] << 4) & 0xf0) | ((sextet[2] >> 2) & 0x0f);
        triplet[2] = ((sextet[2] << 6) & 0xc0) | ((sextet[3]     ) & 0x3f);
        return triplet;
    }

    static Byte sextetFromChar(char c, bool padAllowed)
    {
        static const uint8_t table[] =
        {
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 0-7
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 8-15
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 16-23
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 24-31
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 32-39

        //                    '+'                     '/'
            0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f, // 40-47

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

        //  'X'   'Y'   'Z'
            0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff, // 88-95

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

        if (!padAllowed && c == pad)
            throw error::Decode("Invalid JSON Base64 padding");
        uint8_t index = c;
        uint8_t sextet = table[index];
        if (sextet == 0xff)
            throw error::Decode("Invalid JSON Base64 character");
        assert(sextet < 64);
        return sextet;
    }

}; // class Base64

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_BASE64_HPP

