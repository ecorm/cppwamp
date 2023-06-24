/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TYPES_TUPLE_HPP
#define CPPWAMP_TYPES_TUPLE_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Provides facilities allowing Variant to interoperate
           with std::tuple. */
//------------------------------------------------------------------------------

#include <sstream>
#include "../api.hpp"
#include "../variant.hpp"
#include "../traits.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Converts a Variant::Array to a `std::tuple`.
    @pre The Array's element types must be convertible to the
         tuple's element types.
    @throws error::Conversion if one of the Array element types is not
            convertible to the target type. */
//------------------------------------------------------------------------------
template <typename... Ts>
CPPWAMP_API void toTuple(const wamp::Variant::Array& array,
                         std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Converts a `std::tuple` to a Variant::Array.
    @pre The tuple values must be convertible to Variant's bound types
         (statically checked). */
//------------------------------------------------------------------------------
template <typename... Ts>
CPPWAMP_API wamp::Variant::Array toArray(const std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Performs the conversion from an array variant to a `std::tuple`.
    Users should not use this function directly. Use Variant::to instead. */
//------------------------------------------------------------------------------
template <typename... Ts>
CPPWAMP_API void convert(FromVariantConverter& conv, std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Performs the conversion from a `std::tuple` to an array variant.
    Users should not use this function directly. Use Variant::from instead. */
//------------------------------------------------------------------------------
template <typename... Ts>
CPPWAMP_API void convert(ToVariantConverter& conv, std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Compares a Variant::Array and a `std::tuple` for equality. */
//------------------------------------------------------------------------------
template <typename... Ts>
CPPWAMP_API bool operator==(const Array& array, const std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Compares a `std::tuple` and a Variant::Array for equality. */
//------------------------------------------------------------------------------
template <typename... Ts>
CPPWAMP_API bool operator==(const std::tuple<Ts...>& tuple, const Array& array);

//------------------------------------------------------------------------------
/** Compares a Variant::Array and a `std::tuple` for inequality. */
//------------------------------------------------------------------------------
template <typename... Ts>
CPPWAMP_API bool operator!=(const Array& array, const std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Compares a `std::tuple` and a Variant::Array for inequality. */
//------------------------------------------------------------------------------
template <typename... Ts>
CPPWAMP_API bool operator!=(const std::tuple<Ts...>& tuple, const Array& array);

//------------------------------------------------------------------------------
/** Compares a Variant and a `std::tuple` for equality. */
//------------------------------------------------------------------------------
template <typename... Ts>
CPPWAMP_API bool operator==(const Variant& variant,
                            const std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Compares a `std::tuple` and a Variant for equality. */
//------------------------------------------------------------------------------
template <typename... Ts>
CPPWAMP_API bool operator==(const std::tuple<Ts...>& tuple,
                            const Variant& variant);

//------------------------------------------------------------------------------
/** Compares a Variant and a `std::tuple` for inequality. */
//------------------------------------------------------------------------------
template <typename... Ts>
CPPWAMP_API bool operator!=(const Variant& variant,
                            const std::tuple<Ts...>& tuple);

//------------------------------------------------------------------------------
/** Compares a `std::tuple` and a Variant for inequality. */
//------------------------------------------------------------------------------
template <typename... Ts>
CPPWAMP_API bool operator!=(const std::tuple<Ts...>& tuple,
                            const Variant& variant);


//******************************************************************************
// Internal helper types and functions
//******************************************************************************

namespace internal
{

//------------------------------------------------------------------------------
template <int N, typename...Ts>
int toTupleElement(const Array& array, std::tuple<Ts...>& tuple)
{
    auto& elem = std::get<N>(tuple);
    try
    {
        array.at(N).to(elem);
    }
    catch (const error::Conversion& e)
    {
        std::ostringstream oss;
        oss << e.what() << " (for tuple element #" << N << ")";
        throw error::Conversion(oss.str());
    }
    return 0;
}

template <int N, typename...Ts>
int fromTupleElement(Array& array, const std::tuple<Ts...>& tuple)
{
    const auto& elem = std::get<N>(tuple);
    array.emplace_back(Variant::from(elem));
    return 0;
}

template <typename... Ts, std::size_t... Seq>
void convertToTuple(const Array& array, std::tuple<Ts...>& tuple,
                    IndexSequence<Seq...>)
{
    if (array.size() != sizeof...(Ts))
        throw error::Conversion("Cannot convert variant array to tuple; "
                                "sizes do not match");
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)
    using swallow = int[]; // Guarantees left-to-right evaluation
    (void)swallow{0, toTupleElement<Seq>(array, tuple)...};
}

template <typename... Ts, std::size_t... Seq>
void convertFromTuple(Array& array, const std::tuple<Ts...>& tuple,
                      IndexSequence<Seq...>)
{
    array.reserve(sizeof...(Ts));
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays)
    using swallow = int[]; // Guarantees left-to-right evaluation
    (void)swallow{0, fromTupleElement<Seq>(array, tuple)...};
}

//------------------------------------------------------------------------------
template <std::size_t N, typename... Ts>
using NeedsTupleElement = Needs<N != sizeof...(Ts)>;

template <std::size_t N, typename... Ts>
using NeedsTupleEnd = Needs<N == sizeof...(Ts)>;

template <std::size_t N = 0, typename... Ts, NeedsTupleEnd<N, Ts...> = 0>
bool equalsTuple(const Array&, const std::tuple<Ts...>&)
{
    return true;
}

template <std::size_t N = 0, typename... Ts, NeedsTupleElement<N, Ts...> = 0>
bool equalsTuple(const Array& array, const std::tuple<Ts...>& tuple)
{
    const auto& arrayElem = array.at(N);
    const auto& tupleElem = std::get<N>(tuple);
    bool result = (arrayElem == tupleElem);
    return result && equalsTuple<N+1, Ts...>(array, tuple);
}

//------------------------------------------------------------------------------
template <std::size_t N = 0, typename... Ts, NeedsTupleEnd<N, Ts...> = 0>
bool notEqualsTuple(const Array&, const std::tuple<Ts...>&)
{
    return false;
}

template <std::size_t N = 0, typename... Ts, NeedsTupleElement<N, Ts...> = 0>
bool notEqualsTuple(const Array& array, const std::tuple<Ts...>& tuple)
{
    const auto& arrayElem = array.at(N);
    const auto& tupleElem = std::get<N>(tuple);
    bool result = (arrayElem != tupleElem);
    return result || notEqualsTuple<N+1, Ts...>(array, tuple);
}

} // namespace internal


//******************************************************************************
// Tuple conversion function implementations
//******************************************************************************

//------------------------------------------------------------------------------
template <typename... Ts>
void toTuple(const wamp::Variant::Array& array, std::tuple<Ts...>& tuple)
{
    internal::convertToTuple(array, tuple, IndexSequenceFor<Ts...>{});
}

//------------------------------------------------------------------------------
template <typename... Ts>
wamp::Variant::Array toArray(const std::tuple<Ts...>& tuple)
{
    Array array;
    internal::convertFromTuple(array, tuple, IndexSequenceFor<Ts...>{});
    return array;
}

//------------------------------------------------------------------------------
template <typename... Ts>
void convert(FromVariantConverter& conv, std::tuple<Ts...>& tuple)
{
    using A = internal::VariantUncheckedAccess;

    const auto& variant = conv.variant();
    if (!variant.is<Array>())
    {
        throw error::Conversion("Cannot convert variant to tuple; "
                                "the variant is not an array");
    }
    toTuple(A::alt<Array>(variant), tuple);
}

//------------------------------------------------------------------------------
template <typename... Ts>
void convert(ToVariantConverter& conv, std::tuple<Ts...>& tuple)
{
    conv.variant() = toArray(tuple);
}

//------------------------------------------------------------------------------
template <typename... Ts>
bool operator==(const Array& array, const std::tuple<Ts...>& tuple)
{
    auto N = std::tuple_size<std::tuple<Ts...>>::value;
    return array.size() == N ? internal::equalsTuple(array, tuple) : false;
}

//------------------------------------------------------------------------------
template <typename... Ts>
bool operator==(const std::tuple<Ts...>& tuple, const Array& array)
{
    return array == tuple;
}

//------------------------------------------------------------------------------
template <typename... Ts>
bool operator!=(const Array& array, const std::tuple<Ts...>& tuple)
{
    auto N = std::tuple_size<std::tuple<Ts...>>::value;
    return array.size() == N ? internal::notEqualsTuple(array, tuple) : true;
}

//------------------------------------------------------------------------------
template <typename... Ts>
bool operator!=(const std::tuple<Ts...>& tuple, const Array& array)
{
    return array != tuple;
}

//------------------------------------------------------------------------------
template <typename... Ts>
bool operator==(const Variant& variant, const std::tuple<Ts...>& tuple)
{
    using A = internal::VariantUncheckedAccess;
    return variant.is<Array>() && A::alt<Array>(variant) == tuple;
}

//------------------------------------------------------------------------------
template <typename... Ts>
bool operator==(const std::tuple<Ts...>& tuple, const Variant& variant)
{
    return variant == tuple;
}

//------------------------------------------------------------------------------
template <typename... Ts>
bool operator!=(const Variant& variant, const std::tuple<Ts...>& tuple)
{
    using A = internal::VariantUncheckedAccess;
    return !variant.is<Array>() || A::alt<Array>(variant) != tuple;
}

//------------------------------------------------------------------------------
template <typename... Ts>
bool operator!=(const std::tuple<Ts...>& tuple, const Variant& variant)
{
    return variant != tuple;
}

} // namespace wamp

#endif // CPPWAMP_TYPES_TUPLE_HPP
