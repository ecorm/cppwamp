/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <type_traits>
#include "varianttraits.hpp"

namespace wamp
{

namespace internal
{

template<typename TResult, std::size_t N, typename... Ts>
using TupleResult = typename std::enable_if<N != sizeof...(Ts), TResult>::type;

template<typename TResult, std::size_t N, typename... Ts>
using TupleLastResult =
    typename std::enable_if<N == sizeof...(Ts), TResult>::type;

//------------------------------------------------------------------------------
template <typename T>
void assignFromTupleElement(Variant& v, T&& elem)
{
    static_assert(ArgTraits<T>::isValid,
                  "wamp::fromTuple - Invalid tuple element type");
    v = std::move(elem);
}

template <typename... Ts>
void assignFromTupleElement(Variant& v, std::tuple<Ts...>&& tuple)
{
    v = toArray(std::move(tuple));
}

template<std::size_t N=0, typename... Ts>
TupleLastResult<void,N,Ts...> assignFromTuple(Array&, std::tuple<Ts...>&&) { }

template<std::size_t N=0, typename... Ts>
TupleResult<void,N,Ts...> assignFromTuple(Array& array,
                                          std::tuple<Ts...>&& tuple)
{
    array.push_back(Variant());
    assignFromTupleElement(array.back(), std::get<N>(std::move(tuple)));
    assignFromTuple<N+1, Ts...>(array, std::move(tuple));
}


//------------------------------------------------------------------------------
template <typename T>
void assignToTupleElement(const Variant& v, T& elem)
{
    static_assert(ArgTraits<T>::isValid,
                  "wamp::toTuple - Invalid tuple element type");
    v.to(elem);
}

template <typename... Ts>
void assignToTupleElement(const Variant& v, std::tuple<Ts...>& tuple)
{
    if (!v.is<Array>())
    {
        throw error::Conversion(
            "wamp::error::Conversion: Invalid conversion "
            "from " + typeNameOf(v) + "to tuple");
    }
    toTuple(v.as<Array>(), tuple);
}

template<std::size_t N=0, typename... Ts>
TupleLastResult<void,N,Ts...> assignToTuple(const Array&, std::tuple<Ts...>&) {}

template<std::size_t N=0, typename... Ts>
TupleResult<void,N,Ts...> assignToTuple(const Array& array,
                                        std::tuple<Ts...>& tuple)
{
    using ElemType = typename std::tuple_element<N, std::tuple<Ts...>>::type;
    static_assert(ArgTraits<ElemType>::isValid,
                  "wamp::toTuple - Invalid tuple element type");
    try
    {
        assignToTupleElement(array.at(N), std::get<N>(tuple));
    }
    catch (const error::Conversion& e)
    {
        std::ostringstream oss;
        oss << e.what() << " (for tuple element #" << N << ')';
        throw error::Conversion(oss.str());
    }

    assignToTuple<N+1, Ts...>(array, tuple);
}

//------------------------------------------------------------------------------
template <typename T> struct TupleTag {};

// Foward declaration
template<std::size_t N=0, typename... Ts>
TupleResult<bool,N,Ts...> isConvertibleToTuple(const Array& array);

template <typename T>
bool isConvertibleToTupleElement(const Variant& v, TupleTag<T>)
{
    return v.convertsTo<T>();
}

template <typename... Ts>
bool isConvertibleToTupleElement(const Variant& v, TupleTag<std::tuple<Ts...>>)
{
    return v.is<Array>() &&
           isConvertibleToTuple<0,Ts...>(v.as<Array>());
}

template<std::size_t N=0, typename... Ts>
TupleLastResult<bool,N,Ts...> isConvertibleToTuple(const Array&) {return true;}

template<std::size_t N=0, typename... Ts>
TupleResult<bool,N,Ts...> isConvertibleToTuple(const Array& array)
{
    using ElemType = typename std::tuple_element<N, std::tuple<Ts...>>::type;
    bool result = isConvertibleToTupleElement(array.at(N),
                                              TupleTag<ElemType>());
    return result && isConvertibleToTuple<N+1, Ts...>(array);
}


//------------------------------------------------------------------------------
template<std::size_t N=0, typename... Ts>
TupleLastResult<bool,N,Ts...> equalsTuple(const Array&,
                                          const std::tuple<Ts...>&)
{
    return true;
}

template<std::size_t N=0, typename... Ts>
TupleResult<bool,N,Ts...> equalsTuple(const Array& array,
                                      const std::tuple<Ts...>& tuple)
{
    using ElemType = typename std::tuple_element<N, std::tuple<Ts...>>::type;
    static_assert(ArgTraits<ElemType>::isValid,
                  "wamp::toTuple - Invalid tuple element type");
    const auto& arrayElem = array.at(N);
    const auto& tupleElem = std::get<N>(tuple);
    bool result = (arrayElem == tupleElem);
    return result && equalsTuple<N+1, Ts...>(array, tuple);
}

//------------------------------------------------------------------------------
template<std::size_t N=0, typename... Ts>
TupleLastResult<bool,N,Ts...> notEqualsTuple(const Array&,
                                             const std::tuple<Ts...>&)
{
    return false;
}

template<std::size_t N=0, typename... Ts>
TupleResult<bool,N,Ts...> notEqualsTuple(const Array& array,
                                         const std::tuple<Ts...>& tuple)
{
    using ElemType = typename std::tuple_element<N, std::tuple<Ts...>>::type;
    static_assert(ArgTraits<ElemType>::isValid,
                  "wamp::toTuple - Invalid tuple element type");
    const auto& arrayElem = array.at(N);
    const auto& tupleElem = std::get<N>(tuple);
    bool result = (arrayElem != tupleElem);
    return result || notEqualsTuple<N+1, Ts...>(array, tuple);
}

} // namespace internal


//------------------------------------------------------------------------------
template <typename... Ts>
wamp::Variant::Array toArray(std::tuple<Ts...> tuple)
{
    Array array;
    array.reserve(std::tuple_size<decltype(tuple)>::value);
    internal::assignFromTuple(array, std::move(tuple));
    return array;
}

//------------------------------------------------------------------------------
template <typename... Ts>
void toTuple(const Array& array, std::tuple<Ts...>& tuple)
{
    if (std::tuple_size<std::tuple<Ts...>>::value != array.size())
        throw error::Conversion("wamp::error::Conversion: "
                                "Tuple and array sizes do not match");
    internal::assignToTuple(array, tuple);
}

//------------------------------------------------------------------------------
template <typename... Ts>
bool convertsToTuple(const Array& array, const std::tuple<Ts...>&)
{
    auto N = std::tuple_size<std::tuple<Ts...>>::value;
    return N == array.size() ?
                    internal::isConvertibleToTuple<0,Ts...>(array) :
                    false;
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
    return variant.is<Array>() && variant.as<Array>() == tuple;
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
    return !variant.is<Array>() || variant.as<Array>() != tuple;
}

//------------------------------------------------------------------------------
template <typename... Ts>
bool operator!=(const std::tuple<Ts...>& tuple, const Variant& variant)
{
    return variant != tuple;
}

} // namespace wamp
