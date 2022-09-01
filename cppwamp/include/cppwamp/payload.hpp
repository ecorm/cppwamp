/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_PAYLOAD_HPP
#define CPPWAMP_PAYLOAD_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the declaration of Payload, which bundles together Variant
           arguments. */
//------------------------------------------------------------------------------

#include <initializer_list>
#include <ostream>
#include <sstream>
#include <tuple>
#include <utility>
#include "api.hpp"
#include "options.hpp"
#include "internal/integersequence.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Wrapper around a WAMP message containing payload arguments and an
    options dictionary. */
//------------------------------------------------------------------------------
template <typename TDerived, typename TMessage>
class CPPWAMP_API Payload : public Options<TDerived, TMessage>
{
public:
    /** Sets the positional arguments for this payload. */
    template <typename... Ts>
    TDerived& withArgs(Ts&&... args);

    /** Sets the positional arguments for this payload from a tuple. */
    template <typename... Ts>
    TDerived& withArgsTuple(const std::tuple<Ts...>& tuple);

    /** Sets the positional arguments for this payload from
        an array of variants. */
    TDerived& withArgList(Array args);

    /** Sets the keyword arguments for this payload. */
    TDerived& withKwargs(Object kwargs);

    /** Accesses the constant list of positional arguments. */
    const Array& args() const &;

    /** Returns the moved list of positional arguments. */
    Array args() &&;

    /** Accesses the constant map of keyword arguments. */
    const Object& kwargs() const &;

    /** Returns the moved map of keyword arguments. */
    Object kwargs() &&;

    /** Accesses a positional argument by index. */
    Variant& operator[](size_t index);

    /** Accesses a constant positional argument by index. */
    const Variant& operator[](size_t index) const;

    /** Accesses a keyword argument by key. */
    Variant& operator[](const String& keyword);

    /** Converts the payload's positional arguments to the given value types. */
    template <typename... Ts>
    size_t convertTo(Ts&... values) const;

    /** Converts the payload's positional arguments to the given std::tuple. */
    template <typename... Ts>
    size_t convertToTuple(std::tuple<Ts...>& tuple) const;

    /** Moves the payload's positional arguments to the given value
        references. */
    template <typename... Ts>
    size_t moveTo(Ts&... values);

    /** Moves the payload's positional arguments to the given std::tuple. */
    template <typename... Ts>
    size_t moveToTuple(std::tuple<Ts...>& tuple);

protected:
    /** Constructor taking message construction aruments. */
    template <typename... TArgs>
    explicit Payload(TArgs&&... args);

private:
    using Base = Options<TDerived, TMessage>;

    CPPWAMP_HIDDEN static void bundle(Array&);

    template <typename T, typename... Ts>
    CPPWAMP_HIDDEN static void bundle(Array& array, T&& head, Ts&&... tail);

    template <typename TTuple, int... Seq>
    CPPWAMP_HIDDEN static Array
    bundleFromTuple(TTuple&& tuple, internal::IntegerSequence<Seq...>);

    CPPWAMP_HIDDEN static void unbundleTo(const Array&, size_t&);

    template <typename T, typename... Ts>
    CPPWAMP_HIDDEN static void unbundleTo(const Array& array, size_t& index,
                                          T& head, Ts&... tail);

    template <size_t I, typename... Ts>
    CPPWAMP_HIDDEN static size_t unbundleToTuple(
        const Array&, std::tuple<Ts...>& tuple, std::true_type);

    template <size_t I, typename... Ts>
    CPPWAMP_HIDDEN static size_t unbundleToTuple(
        const Array&, std::tuple<Ts...>& tuple, std::false_type);

    CPPWAMP_HIDDEN static void unbundleAs(Array&, size_t&);

    template <typename T, typename... Ts>
    CPPWAMP_HIDDEN static void unbundleAs(Array& array, size_t& index, T& head,
                                          Ts&... tail);

    template <size_t I, typename... Ts>
    CPPWAMP_HIDDEN static size_t unbundleAsTuple(
        const Array&, std::tuple<Ts...>& tuple, std::true_type);

    template <size_t I, typename... Ts>
    CPPWAMP_HIDDEN static size_t unbundleAsTuple(
        const Array&, std::tuple<Ts...>& tuple, std::false_type);
};


//******************************************************************************
// Payload implementation
//******************************************************************************

//------------------------------------------------------------------------------
/** Each argument is converted to a Variant using Variant::from. This allows
    custom types to be passed in, as long as the `convert` function is
    specialized for those custom types. */
//------------------------------------------------------------------------------
template <typename D, typename M>
template <typename... Ts>
D& Payload<D,M>::withArgs(Ts&&... args)
{
    Array array;
    bundle(array, std::forward<Ts>(args)...);
    return withArgList(std::move(array));
}

//------------------------------------------------------------------------------
/** Each tuple element is converted to a Variant using Variant::from. This
    allows custom types to be passed in, as long as the `convert` function is
    specialized for those custom types. */
//------------------------------------------------------------------------------
template <typename D, typename M>
template <typename... Ts>
D& Payload<D,M>::withArgsTuple(const std::tuple<Ts...>& tuple)
{
    using Seq = typename internal::GenIntegerSequence<sizeof...(Ts)>::type;
    return withArgList(bundleFromTuple(tuple, Seq{}));
}

//------------------------------------------------------------------------------
/** @post `std::equal(args.begin(), args.end(), this->args()) == true` */
//------------------------------------------------------------------------------
template <typename D, typename M>
D& Payload<D,M>::withArgList(Array list)
{
    this->message().args() = std::move(list);
    return static_cast<D&>(*this);
}

//------------------------------------------------------------------------------
/** @post `std::equal(kwargs.begin(), kwargs.end(), this->kwargs()) == true` */
//------------------------------------------------------------------------------
template <typename D, typename M>
D& Payload<D,M>::withKwargs(Object map)
{
    this->message().kwargs() = std::move(map);
    return static_cast<D&>(*this);
}

//------------------------------------------------------------------------------
template <typename D, typename M>
const Array& Payload<D,M>::args() const &
{
    return this->message().args();
}

//------------------------------------------------------------------------------
/** @details
This overload takes effect when `*this` is an r-value. For example:
```
Array mine = std::move(payload).args();
```
@post this->args().empty() == true */
//------------------------------------------------------------------------------
template <typename D, typename M>
Array Payload<D,M>::args() &&
{
    auto& array = this->message().args();
    Array result(std::move(array));
    array.clear();
    return result;
}

//------------------------------------------------------------------------------
template <typename D, typename M>
const Object& Payload<D,M>::kwargs() const &
{
    return this->message().kwargs();
}

//------------------------------------------------------------------------------
/** @details
This overload takes effect when `*this` is an r-value. For example:
```
Object mine = std::move(payload).kwargs();
```
@post this->kwargs().empty() == true */
//------------------------------------------------------------------------------
template <typename D, typename M>
Object Payload<D,M>::kwargs() &&
{
    auto& object = this->message().kwargs();
    Object result(std::move(object));
    object.clear();
    return result;
}

//------------------------------------------------------------------------------
/** @details
    @pre `this->args().size() > index`
    @throws std::out_of_range if the given index is not within the range
            of this->args(). */
//------------------------------------------------------------------------------
template <typename D, typename M>
Variant& Payload<D,M>::operator[](size_t index)
{
    return this->message().args().at(index);
}

//------------------------------------------------------------------------------
/** @details
    @pre `this->args().size() > index`
    @throws std::out_of_range if the given index is not within the range
            of this->args(). */
//------------------------------------------------------------------------------
template <typename D, typename M>
const Variant& Payload<D,M>::operator[](size_t index) const
{
    return this->message().args().at(index);
}

//------------------------------------------------------------------------------
/** @details
    If the key doesn't exist, a null variant is inserted under the key
    before the reference is returned. */
//------------------------------------------------------------------------------
template <typename D, typename M>
Variant& Payload<D,M>::operator[](const std::string& keyword)
{
    return this->message().kwargs()[keyword];
}

//------------------------------------------------------------------------------
/** @par Example
```
// Result derives from Payload
Result result = session.call("rpc", yield).value();
std::string s;
int n = 0;
result.convertTo(s, n);
```

@return The number of elements that were converted
@pre `this->args()` elements are convertible to their target types
@post `std::min(this->args()->size(), sizeof...(Ts))` elements are converted
@throws error::Conversion if an argument cannot be converted to the
        target type. */
//------------------------------------------------------------------------------
template <typename D, typename M>
template <typename... Ts>
size_t Payload<D,M>::convertTo(Ts&... values) const
{
    size_t index = 0;
    unbundleTo(args(), index, values...);
    return index;
}

template <typename D, typename M>
template <typename... Ts>
size_t Payload<D, M>::convertToTuple(std::tuple<Ts...>& tuple) const
{
    using More = std::integral_constant<bool, sizeof...(Ts) != 0>;
    return unbundleToTuple<0>(args(), tuple, More{});
}

//------------------------------------------------------------------------------
/** @par Example
```
// Result derives from Payload
Result result = session.call("rpc", yield).value();
String s;
Int n = 0;
result.moveTo(s, n);
```

@return The number of elements that were moved
@pre Args::list elements are accessible as their target types
@post `std::min(this->list->size(), sizeof...(Ts))` elements are moved
@post The moved elements in this->args() are nullified
@throws error::Access if an argument's dynamic type does not match its
        associated target type. */
//------------------------------------------------------------------------------
template <typename D, typename M>
template <typename... Ts>
size_t Payload<D,M>::moveTo(Ts&... values)
{
    size_t index = 0;
    unbundleAs(this->message().args(), index, values...);
    return index;
}

//------------------------------------------------------------------------------
template <typename D, typename M>
template <typename... Ts>
size_t Payload<D,M>::moveToTuple(std::tuple<Ts...>& tuple)
{
    using More = std::integral_constant<bool, sizeof...(Ts) != 0>;
    return unbundleAsTuple<0>(args(), tuple, More{});
}

//------------------------------------------------------------------------------
template <typename D, typename M>
template <typename... TArgs>
Payload<D,M>::Payload(TArgs&&... args)
    : Base(std::forward<TArgs>(args)...)
{}

//------------------------------------------------------------------------------
template <typename D, typename M>
void Payload<D,M>::bundle(Array&) {}

template <typename D, typename M>
template <typename T, typename... Ts>
void Payload<D,M>::bundle(Array& array, T&& head, Ts&&... tail)
{
    array.emplace_back(Variant::from(std::forward<T>(head)));
    bundle(array, tail...);
}

//------------------------------------------------------------------------------
template <typename D, typename M>
template <typename TTuple, int... Seq>
Array Payload<D,M>::bundleFromTuple(TTuple&& tuple,
                                    internal::IntegerSequence<Seq...>)
{
    return Array{Variant::from(std::get<Seq>(std::forward<TTuple>(tuple)))...};
}

//------------------------------------------------------------------------------
template <typename D, typename M>
void Payload<D,M>::unbundleTo(const Array&, size_t&) {}

template <typename D, typename M>
template <typename T, typename... Ts>
void Payload<D,M>::unbundleTo(const Array& array, size_t& index, T& head,
                              Ts&... tail)
{
    if (index < array.size())
    {
        try
        {
            head = array[index].to<T>();
        }
        catch (const error::Conversion& e)
        {
            std::ostringstream oss;
            oss << "Payload element at index " << index
                << " is not convertible to the target type: " << e.what();
            throw error::Conversion(oss.str());
        }

        unbundleTo(array, ++index, tail...);
    }
}

//------------------------------------------------------------------------------
template <typename D, typename M>
template <size_t I, typename... Ts>
size_t Payload<D,M>::unbundleToTuple(
    const Array& array, std::tuple<Ts...>& tuple, std::true_type)
{
    if (I < array.size())
    {
        using T = NthTypeOf<I, Ts...>;
        try
        {
            std::get<I>(tuple) = array[I].to<T>();
        }
        catch (const error::Conversion& e)
        {
            std::ostringstream oss;
            oss << "Payload element at index " << I
                << " is not convertible to the target type: " << e.what();
            throw error::Conversion(oss.str());
        }

        using More = std::integral_constant<bool, I+1 != sizeof...(Ts)>;
        return unbundleToTuple<I+1, Ts...>(array, tuple, More{});
    }
    return I;
}

template <typename D, typename M>
template <size_t I, typename... Ts>
size_t Payload<D,M>::unbundleToTuple(
    const Array& array, std::tuple<Ts...>& tuple, std::false_type)
{
    return I;
}

//------------------------------------------------------------------------------
template <typename D, typename M>
void Payload<D,M>::unbundleAs(Array&, size_t&) {}

template <typename D, typename M>
template <typename T, typename... Ts>
void Payload<D,M>::unbundleAs(Array& array, size_t& index, T& head, Ts&... tail)
{
    if (index < array.size())
    {
        auto& arg = array[index];
        if (!arg.template is<T>())
        {
            std::ostringstream oss;
            oss << "Payload element of type " << typeNameOf(arg)
                << " at index " << index
                << " is not of type: " << typeNameOf<T>();
            throw error::Access(oss.str());
        }
        head = std::move(arg.as<T>());
        unbundleAs(array, ++index, tail...);
    }
}

//------------------------------------------------------------------------------
template <typename D, typename M>
template <size_t I, typename... Ts>
size_t Payload<D,M>::unbundleAsTuple(
    const Array& array, std::tuple<Ts...>& tuple, std::true_type)
{
    if (I < array.size())
    {
        using T = NthTypeOf<I, Ts...>;
        auto& arg = array[I];
        if (!arg.template is<T>())
        {
            std::ostringstream oss;
            oss << "Payload element of type " << typeNameOf(arg)
                << " at index " << I
                << " is not of type: " << typeNameOf<T>();
            throw error::Access(oss.str());
        }
        std::get<I>(tuple) = std::move(arg.as<T>());
        using More = std::integral_constant<bool, I+1 != sizeof...(Ts)>;
        return unbundleAsTuple<I+1, Ts...>(array, tuple, More{});
    }
    return I;
}

template <typename D, typename M>
template <size_t I, typename... Ts>
size_t Payload<D,M>::unbundleAsTuple(
    const Array& array, std::tuple<Ts...>& tuple, std::false_type)
{
    return I;
}

} // namespace wamp

#endif // CPPWAMP_PAYLOAD_HPP
