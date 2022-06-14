/*------------------------------------------------------------------------------
              Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include "../msgpack.hpp"
#include <cassert>
#include <msgpack/pack.hpp>
#include <msgpack/unpack.hpp>
#include <msgpack/type.hpp>
#include "../../api.hpp"
#include "../../internal/variantbuilder.hpp"

#ifdef MSGPACK_USE_LEGACY_NAME_AS_FLOAT
    #define CPPWAMP_MSGPACK_FLOAT_NAME DOUBLE
#else
    #define CPPWAMP_MSGPACK_FLOAT_NAME FLOAT
#endif

namespace wamp
{

namespace internal
{

// Forward declaration
void decodeMsgpackObject(VariantBuilder& builder, const msgpack::object& obj);

//------------------------------------------------------------------------------
CPPWAMP_INLINE void decodeMsgpackArray(VariantBuilder& builder,
                                       const msgpack::object_array& array)
{
    builder.StartArray(array.size);
    for (decltype(array.size) i=0; i<array.size; ++i)
        decodeMsgpackObject(builder, array.ptr[i]);
    builder.EndArray(array.size);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void decodeMsgpackMap(VariantBuilder& builder,
                                     const msgpack::object_map& map)
{
    builder.StartObject();
    for (decltype(map.size) i=0; i<map.size; ++i)
    {
        const auto& kv = map.ptr[i];
        const auto& key = kv.key;
        if (key.type != msgpack::type::STR)
            throw error::Decode("Msgpack MAP non-string keys are not supported");
        builder.Key(key.via.str.ptr, key.via.str.size, true);
        decodeMsgpackObject(builder, kv.val);
    }
    builder.EndObject(map.size);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void decodeMsgpackObject(VariantBuilder& builder,
                                        const msgpack::object& obj)
{
    using namespace msgpack;
    switch (obj.type)
    {
        case type::NIL:
            builder.Null();
            break;

        case type::BOOLEAN:
            builder.Bool(obj.as<bool>());
            break;

        case type::POSITIVE_INTEGER:
            builder.Uint64(obj.as<uint64_t>());
            break;

        case type::NEGATIVE_INTEGER:
            builder.Int64(obj.as<int64_t>());
            break;

        case type::CPPWAMP_MSGPACK_FLOAT_NAME:
            builder.Double(obj.as<double>());
            break;

        case type::STR:
            builder.String(obj.via.str.ptr, obj.via.str.size, true);
            break;

        case type::BIN:
            builder.Bin(obj.via.bin.ptr, obj.via.bin.size);
            break;

        case type::ARRAY:
            decodeMsgpackArray(builder, obj.via.array);
            break;

        case type::MAP:
            decodeMsgpackMap(builder, obj.via.map);
            break;

        case type::EXT:
            throw error::Decode("Msgpack EXT format is not supported");

        default: assert(false);
    }
}

//------------------------------------------------------------------------------
template <typename TStream>
struct EncodeMsgpack : public Visitor<>
{
    using Packer = msgpack::packer<TStream>;

    void operator()(const Null&, Packer& packer) const {packer.pack_nil();}

    template <typename TField>
    void operator()(const TField& field, Packer& packer) const
        {packer << field;}

    void operator()(const Blob& blob, Packer& packer) const
    {
        packer.pack_bin(blob.data().size());
        const char* data = reinterpret_cast<const char*>(blob.data().data());
        packer.pack_bin_body(data, blob.data().size());
    }

    void operator()(const Array& array, Packer& packer) const
    {
        packer.pack_array(array.size());
        for (const auto& elem: array)
            applyWithOperand(*this, elem, packer);
    }

    void operator()(const Object& object, Packer& packer) const
    {
        packer.pack_map(object.size());
        for (const auto& kv: object)
        {
            packer << kv.first;
            applyWithOperand(*this, kv.second, packer);
        }
    }
};

//------------------------------------------------------------------------------
struct MsgpackStringBuffer
{
    void write(const char* data, size_t size) {str.append(data, size);}

    std::string& str;
};

} // namespace internal


//------------------------------------------------------------------------------
/** @throws error::Decode if there is an error while parsing the Msgpack
            payload. */
//------------------------------------------------------------------------------
template <typename TBuffer>
void Msgpack::decodeBuffer(const TBuffer& from, Variant& to)
{
    try
    {
        using namespace msgpack;
        auto result = unpack(from.data(), from.length(),
            [] (type::object_type, std::size_t, void*) {return true;});
        const auto& obj = result.get();

        Variant v;
        internal::VariantBuilder builder(v);
        decodeMsgpackObject(builder, obj);
        to.swap(v);
    }
    catch (const msgpack::unpack_error& e)
    {
        throw error::Decode(std::string("Failure parsing Msgpack: ") +
                            e.what());
    }
    catch(...)
    {
        throw;
    }
}

//------------------------------------------------------------------------------
/** @throws error::Decode if there is an error while parsing the Msgpack
            payload. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Msgpack::decode(const std::string& from, Variant& to)
{
    decodeBuffer<std::string>(from, to);
}

//------------------------------------------------------------------------------
template <typename TBuffer>
void Msgpack::encodeBuffer(const Variant& from, TBuffer& to)
{
    msgpack::packer<TBuffer> packer(to);
    applyWithOperand(internal::EncodeMsgpack<TBuffer>(), from, packer);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Msgpack::encode(const Variant& from, std::ostream& to)
{
    encodeBuffer<std::ostream>(from, to);
}

//------------------------------------------------------------------------------
/** @note The destination string is not cleared before serialization occurs.
          This is done intentionally to permit several variant objects being
          serialized to the same destination string. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Msgpack::encode(const Variant& from, std::string& to)
{
    internal::MsgpackStringBuffer buffer{to};
    encodeBuffer<internal::MsgpackStringBuffer>(from, buffer);
}

} // namespace wamp
