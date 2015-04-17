/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <cassert>
#include <cstdio>
#include <sstream>
#include <rapidjson/reader.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/error/en.h>
#include "variantbuilder.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TStream>
void decodeJson(TStream& in, Variant& variant)
{
    Variant v;
    VariantBuilder builder(v);
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
        char str[32];
        auto length = std::snprintf(str, sizeof(str), "%.17e", x);
        assert(length < sizeof(str));
        buf.write(str, length);
    }

    void operator()(const std::string& s, TBuffer& buf) const
    {
        static const std::string quote("\"");
        write(buf, quote);
        write(buf, s);
        write(buf, quote);
    }

    void operator()(const Array& a, TBuffer& buf) const
    {
        static const std::string openBracket("[");
        static const std::string closeBracket("]");
        static const std::string comma(",");
        write(buf, openBracket);
        for (const auto& v: a)
        {
            if (&v != &a.front())
                write(buf, comma);
            applyWithOperand(*this, v, buf);
        }
        write(buf, closeBracket);
    }

    void operator()(const Object& o, TBuffer& buf) const
    {
        static const std::string openBrace("{");
        static const std::string closeBrace("}");
        static const std::string colon(":");
        static const std::string comma(",");
        write(buf, openBrace);
        for (auto kv = o.cbegin(); kv != o.cend(); ++kv)
        {
            if (kv != o.cbegin())
                write(buf, comma);
            this->operator()(kv->first, buf);
            write(buf, colon);
            applyWithOperand(*this, kv->second, buf);
        }
        write(buf, closeBrace);
    }

    static void write(TBuffer& buf, const std::string& s)
    {
        buf.write(s.data(), s.length());
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
inline void Json::decode(std::istream& from, Variant& to)
{
    internal::IStreamWrapper<std::istream> stream(from);
    internal::decodeJson(stream, to);
}

//------------------------------------------------------------------------------
/** @throws error::Decode if there is an error while parsing the JSON
            payload. */
//------------------------------------------------------------------------------
inline void Json::decode(const std::string& from, Variant& to)
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
inline void Json::encode(const Variant& from, std::ostream& to)
{
    applyWithOperand(internal::EncodeJson<std::ostream>(), from, to);
}

//------------------------------------------------------------------------------
/** @note The destination string is not cleared before serialization occurs.
          This is done intentionally to permit several variant objects being
          serialized to the same destination string. */
//------------------------------------------------------------------------------
inline void Json::encode(const Variant& from, std::string& to)
{
    internal::JsonStringBuffer buffer{to};
    applyWithOperand(internal::EncodeJson<decltype(buffer)>(), from, buffer);
}

} // namespace wamp
