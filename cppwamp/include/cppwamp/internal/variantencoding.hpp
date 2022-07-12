/*------------------------------------------------------------------------------
                  Copyright Butterfly Energy Systems 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_VARIANTENCODING_HPP
#define CPPWAMP_INTERNAL_VARIANTENCODING_HPP

#include <jsoncons/byte_string.hpp>
#include <jsoncons/sink.hpp>
#include "../codec.hpp"
#include "../variant.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TEncoder>
class VariantEncodingVisitor : public Visitor<>
{
public:
    explicit VariantEncodingVisitor(TEncoder& encoder)
        : encoder_(encoder)
    {}

    void operator()(Null)
    {
        encoder_.null_value();
    }

    void operator()(Bool b)
    {
        encoder_.bool_value(b);
    }

    void operator()(Int n)
    {
        encoder_.int64_value(n);
    }

    void operator()(UInt n)
    {
        encoder_.uint64_value(n);
    }

    void operator()(Real x)
    {
        encoder_.double_value(x);
    }

    void operator()(const String& s)
    {
        encoder_.string_value({s.data(), s.size()});
    }

    void operator()(const Blob& b)
    {
        jsoncons::byte_string_view bsv(b.data().data(), b.data().size());
        encoder_.byte_string_value(bsv);
    }

    void operator()(const Array& a)
    {
        encoder_.begin_array(a.size());
        for (const auto& v: a)
            apply(*this, v);
        encoder_.end_array();
    }

    void operator()(const Object& o)
    {
        encoder_.begin_object(o.size());
        for (const auto& kv: o)
        {
            const auto& key = kv.first;
            encoder_.key({key.data(), key.size()});
            apply(*this, kv.second);
        }
        encoder_.end_object();
    }

private:
    TEncoder& encoder_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_VARIANTENCODING_HPP
