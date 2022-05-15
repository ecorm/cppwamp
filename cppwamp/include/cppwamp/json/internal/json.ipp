/*------------------------------------------------------------------------------
              Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include "../json.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <utility>
#include <rapidjson/reader.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/error/en.h>
#include "../../api.hpp"
#include "../../internal/base64.hpp"
#include "../../internal/variantbuilder.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class JsonVariantBuilder : public VariantBuilder
{
public:
    using VariantBuilder::VariantBuilder;

    bool String(const char* str, SizeType length, bool /*copy*/)
    {
        if ( (length > 0) && (str[0] == '\0') )
        {
            Blob::Data data;
            Base64::decode(str + 1, length - 1, data);
            return Base::Bin(std::move(data));
        }
        else
            return Base::String(str, length, true);
    }

    // Only used if kParseNumbersAsStringsFlag is set
    bool RawNumber(const char* /*str*/, SizeType /*length*/, bool /*copy*/)
    {
        // kParseNumbersAsStringsFlag should never be set in CppWAMP.
        assert(false && "RapidJSON kParseNumbersAsStringsFlag not supported");
        return false;
    }

private:
    using Base = VariantBuilder;
};

//------------------------------------------------------------------------------
template <typename TStream>
void decodeJson(TStream& in, Variant& variant)
{
    Variant v;
    JsonVariantBuilder builder(v);
    auto result = rapidjson::Reader().Parse(in, builder);
    if (result.IsError())
    {
        std::ostringstream oss;
        oss << "Failure parsing JSON: "
            << rapidjson::GetParseError_En(result.Code())
            << " (at offset " << result.Offset() << ")";
        throw error::Decode(oss.str());
    }
    variant.swap(v);
}

//------------------------------------------------------------------------------
template <typename TIStream>
class IStreamWrapper
{
public:
    using Ch = typename TIStream::char_type;

    IStreamWrapper(TIStream& is) : is_(is) {}

    Ch Peek() const
    {
        auto c = is_.peek();
        return c == std::char_traits<Ch>::eof() ? '\0' : (Ch)c;
    }

    Ch Take()
    {
        auto c = is_.get();
        return c == std::char_traits<Ch>::eof() ? '\0' : (Ch)c;
    }

    size_t Tell() const { return (size_t)is_.tellg(); }

    // Unused
    Ch* PutBegin()      {assert(false); return 0;}
    void Put(Ch)        {assert(false);}
    void Flush()        {assert(false);}
    size_t PutEnd(Ch*)  {assert(false); return 0;}

private:
    TIStream& is_;
};

//------------------------------------------------------------------------------
template <typename TBuffer>
struct EncodeJson : public Visitor<>
{
    void operator()(const Null&, TBuffer& buf) const
    {
        static const std::string nullStr("null");
        write(buf, nullStr);
    }

    void operator()(const Bool& b, TBuffer& buf) const
    {
        static const std::string trueStr("true");
        static const std::string falseStr("false");
        write(buf, b ? trueStr : falseStr);
    }

    template <typename TInteger>
    void operator()(TInteger n, TBuffer& buf) const
    {
        write(buf, std::to_string(n));
    }

    void operator()(Real x, TBuffer& buf) const
    {
        static const std::string nullStr("null");

        if (std::isfinite(x))
        {
            char str[32];
            auto length = std::snprintf(str, sizeof(str), "%.17e", x);
            assert(length > 0 && length < int(sizeof(str)));
            buf.write(str, length);
        }
        else
        {
            // ECMA-262, NOTE 4, p.208:
            // "Finite numbers are stringified as if by calling
            //  ToString(number). NaN and Infinity regardless of sign are
            // represented as the String null.
            write(buf, nullStr);
        }
    }

    void operator()(const std::string& s, TBuffer& buf) const
    {
        writeChar(buf, '\"');
        for (char c: s)
            writeEncodedChar(buf, c);
        writeChar(buf, '\"');
    }

    void operator()(const Blob& b, TBuffer& buf) const
    {
        writeChar(buf, '\"');
        writeEncodedChar(buf, '\0');
        Base64::encode(b.data(), buf);
        writeChar(buf, '\"');
    }

    void operator()(const Array& a, TBuffer& buf) const
    {
        writeChar(buf, '[');
        for (const auto& v: a)
        {
            if (&v != &a.front())
                writeChar(buf, ',');
            applyWithOperand(*this, v, buf);
        }
        writeChar(buf, ']');
    }

    void operator()(const Object& o, TBuffer& buf) const
    {
        writeChar(buf, '{');
        for (auto kv = o.cbegin(); kv != o.cend(); ++kv)
        {
            if (kv != o.cbegin())
                writeChar(buf, ',');
            this->operator()(kv->first, buf);
            writeChar(buf, ':');
            applyWithOperand(*this, kv->second, buf);
        }
        writeChar(buf, '}');
    }

    static void write(TBuffer& buf, const std::string& s)
    {
        buf.write(s.data(), s.length());
    }

    static void writeChar(TBuffer& buf, char c)
    {
        buf.write(&c, 1);
    }

    static void writeEncodedChar(TBuffer& buf, char c)
    {
        switch (c)
        {
        case '\"': writeChar(buf, '\\'); writeChar(buf, '\"'); return;
        case '\\': writeChar(buf, '\\'); writeChar(buf, '\\'); return;
        case '\b': writeChar(buf, '\\'); writeChar(buf, 'b'); return;
        case '\f': writeChar(buf, '\\'); writeChar(buf, 'f'); return;
        case '\n': writeChar(buf, '\\'); writeChar(buf, 'n'); return;
        case '\r': writeChar(buf, '\\'); writeChar(buf, 'r'); return;
        case '\t': writeChar(buf, '\\'); writeChar(buf, 't'); return;
        }

        auto n = static_cast<unsigned int>(c);
        if (n <= 0x1f)
        {
            char str[8];
            auto length = std::snprintf(str, sizeof(str), "\\u%04X", n);
            assert(length > 0 && length < int(sizeof(str)));
            buf.write(str, length);
        }
        else
            writeChar(buf, c);
    }
};

struct JsonStringBuffer
{
    void write(const char* data, size_t length) {s.append(data, length);}
    std::string& s;
};

} // namespace internal


//------------------------------------------------------------------------------
/** @throws error::Decode if there is an error while parsing the JSON
            payload. */
//------------------------------------------------------------------------------
template <typename TBuffer>
void Json::decodeBuffer(const TBuffer& from, Variant& to)
{
    rapidjson::StringStream stream(from.data());
    internal::decodeJson(stream, to);
}

//------------------------------------------------------------------------------
/** @throws error::Decode if there is an error while parsing the JSON
            payload. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Json::decode(std::istream& from, Variant& to)
{
    internal::IStreamWrapper<std::istream> stream(from);
    internal::decodeJson(stream, to);
}

//------------------------------------------------------------------------------
/** @throws error::Decode if there is an error while parsing the JSON
            payload. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Json::decode(const std::string& from, Variant& to)
{
    decodeBuffer<std::string>(from, to);
}

//------------------------------------------------------------------------------
template <typename TBuffer>
void Json::encodeBuffer(const Variant& from, TBuffer& to)
{
    applyWithOperand(internal::EncodeJson<TBuffer>(), from, to);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Json::encode(const Variant& from, std::ostream& to)
{
    applyWithOperand(internal::EncodeJson<std::ostream>(), from, to);
}

//------------------------------------------------------------------------------
/** @note The destination string is not cleared before serialization occurs.
          This is done intentionally to permit several variant objects being
          serialized to the same destination string. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Json::encode(const Variant& from, std::string& to)
{
    internal::JsonStringBuffer buffer{to};
    applyWithOperand(internal::EncodeJson<decltype(buffer)>(), from, buffer);
}

} // namespace wamp
