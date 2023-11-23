/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_PAYLOAD_HPP
#define CPPWAMP_PAYLOAD_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Provides facilities for accessing WAMP message payloads. */
//------------------------------------------------------------------------------

#include <initializer_list>
#include <ostream>
#include <sstream>
#include <tuple>
#include <utility>
#include "api.hpp"
#include "options.hpp"
#include "traits.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Wrapper around a WAMP message containing payload arguments and an
    options dictionary. */
//------------------------------------------------------------------------------
template <typename TDerived, internal::MessageKind K>
class Payload : public Options<TDerived, K>
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
    TDerived& withArgList(Array list);

    /** Sets the keyword arguments for this payload. */
    TDerived& withKwargs(Object dict);

    /** Determines is there are any positional or keyward arguments. */
    bool hasArgs() const;

    /** Accesses the positional arguments. */
    const Array& args() const &;

    /** Accesses the positional arguments. */
    Array& args() &;

    /** Moves the positional arguments. */
    Array&& args() &&;

    /** Accesses the dictionary of keyword arguments. */
    const Object& kwargs() const &;

    /** Accesses the dictionary of keyword arguments. */
    Object& kwargs() &;

    /** Moves the dictionary of keyword arguments. */
    Object&& kwargs() &&;

    /** Accesses a positional argument by index. */
    Variant& operator[](size_t index);

    /** Accesses a constant positional argument by index. */
    const Variant& operator[](size_t index) const;

    /** Accesses a keyword argument by key. */
    Variant& operator[](const String& keyword);

    /** Determines if a keyword argument exists. */
    bool hasKwarg(const String& key) const;

    /** Obtains a keyword argument by key, or a null variant if absent. */
    const Variant& kwargByKey(const String& key) const;

    /** Obtains a keyword argument by key, converted to the given type, or a
        fallback value. */
    template <typename T, typename U>
    T kwargOr(const String& key, U&& fallback) const;

    /** Obtains a keyword argument by key having the given type. */
    template <typename T>
    ErrorOr<T> kwargAs(const String& key) const &;

    /** Moves a keyword argument by key having the given type. */
    template <typename T>
    ErrorOr<T> kwargAs(const String& key) &&;

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
    template <typename... Ts>
    explicit Payload(in_place_t, Ts&&... fields);

    template <internal::MessageKind M>
    explicit Payload(internal::Command<M>&& command);

    explicit Payload(internal::Message&& msg);

private:
    using Base = Options<TDerived, K>;
    using KindTraits = internal::MessageKindTraits<K>;

    static constexpr unsigned argsPos_ = KindTraits::argsPos;
    static constexpr unsigned kwargsPos_ = KindTraits::argsPos + 1;

    CPPWAMP_HIDDEN static void bundle(Array&);

    template <typename T, typename... Ts>
    CPPWAMP_HIDDEN static void bundle(Array& array, T&& head, Ts&&... tail);

    template <typename TTuple, std::size_t... Seq>
    CPPWAMP_HIDDEN static Array
    bundleFromTuple(TTuple&& tuple, IndexSequence<Seq...>);

    CPPWAMP_HIDDEN static void unbundleTo(const Array&, size_t&);

    template <typename T, typename... Ts>
    CPPWAMP_HIDDEN static void unbundleTo(const Array& array, size_t& index,
                                          T& head, Ts&... tail);

    template <size_t I, typename... Ts>
    CPPWAMP_HIDDEN static size_t unbundleToTuple(
        const Array&, std::tuple<Ts...>& tuple, TrueType);

    template <size_t I, typename... Ts>
    CPPWAMP_HIDDEN static size_t unbundleToTuple(
        const Array&, std::tuple<Ts...>& tuple, FalseType);

    CPPWAMP_HIDDEN static void unbundleAs(Array&, size_t&);

    template <typename T, typename... Ts>
    CPPWAMP_HIDDEN static void unbundleAs(Array& array, size_t& index, T& head,
                                          Ts&... tail);

    template <size_t I, typename... Ts>
    CPPWAMP_HIDDEN static size_t unbundleAsTuple(
        const Array&, std::tuple<Ts...>& tuple, TrueType);

    template <size_t I, typename... Ts>
    CPPWAMP_HIDDEN static size_t unbundleAsTuple(
        const Array&, std::tuple<Ts...>& tuple, FalseType);

    CPPWAMP_HIDDEN void normalize();

    CPPWAMP_HIDDEN void setArgs();

    template <typename... Ts>
    void setArgs(Ts&&... args);

public:
    // Internal use only
    Array& args(internal::PassKey);
    Object& kwargs(internal::PassKey);
};


//******************************************************************************
// Payload implementation
//******************************************************************************

//------------------------------------------------------------------------------
/** Each argument is converted to a Variant using Variant::from. This allows
    custom types to be passed in, as long as the `convert` function is
    specialized for those custom types. */
//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
template <typename... Ts>
D& Payload<D,K>::withArgs(Ts&&... args)
{
    setArgs(std::forward<Ts>(args)...);
    return static_cast<D&>(*this);
}

//------------------------------------------------------------------------------
/** Each tuple element is converted to a Variant using Variant::from. This
    allows custom types to be passed in, as long as the `convert` function is
    specialized for those custom types. */
//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
template <typename... Ts>
D& Payload<D,K>::withArgsTuple(const std::tuple<Ts...>& tuple)
{
    return withArgList(bundleFromTuple(tuple, IndexSequenceFor<Ts...>{}));
}

//------------------------------------------------------------------------------
/** @post `std::equal(args.begin(), args.end(), this->args()) == true` */
//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
D& Payload<D,K>::withArgList(Array list)
{
    Array& a = args();
    a = std::move(list);
    return static_cast<D&>(*this);
}

//------------------------------------------------------------------------------
/** @post `std::equal(kwargs.begin(), kwargs.end(), this->kwargs()) == true` */
//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
D& Payload<D,K>::withKwargs(Object dict)
{
    Object& o = kwargs();
    o = std::move(dict);
    return static_cast<D&>(*this);
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
bool Payload<D,K>::hasArgs() const
{
    return !args().empty() || !kwargs().empty();
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
const Array& Payload<D,K>::args() const &
{
    return this->message().template as<Array>(argsPos_);
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
Array& Payload<D,K>::args() &
{
    return this->message().template as<Array>(argsPos_);
}

//------------------------------------------------------------------------------
/** @details
This overload takes effect when `*this` is an r-value. For example:
```
Array mine = std::move(payload).args();
``` */
//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
Array&& Payload<D,K>::args() &&
{
    return std::move(this->message().template as<Array>(argsPos_));;
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
const Object& Payload<D,K>::kwargs() const &
{
    return this->message().template as<Object>(kwargsPos_);
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
Object& Payload<D,K>::kwargs() &
{
    return this->message().template as<Object>(kwargsPos_);
}

//------------------------------------------------------------------------------
/** @details
This overload takes effect when `*this` is an r-value. For example:
```
Object mine = std::move(payload).kwargs();
``` */
//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
Object&& Payload<D,K>::kwargs() &&
{
    return std::move(this->message().template as<Object>(kwargsPos_));
}

//------------------------------------------------------------------------------
/** @details
    @pre `this->args().size() > index`
    @throws std::out_of_range if the given index is not within the range
            of this->args(). */
//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
Variant& Payload<D,K>::operator[](size_t index)
{
    return args().at(index);
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
bool Payload<D, K>::hasKwarg(const String& key) const
{
    return kwargs().count(key) != 0;
}

//------------------------------------------------------------------------------
/** @details
    @pre `this->args().size() > index`
    @throws std::out_of_range if the given index is not within the range
            of this->args(). */
//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
const Variant& Payload<D,K>::operator[](size_t index) const
{
    return args().at(index);
}

//------------------------------------------------------------------------------
/** @details
    If the key doesn't exist, a null variant is inserted under the key
    before the reference is returned. */
//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
Variant& Payload<D,K>::operator[](const std::string& keyword)
{
    return kwargs()[keyword];
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
const Variant& Payload<D,K>::kwargByKey(const String& key) const
{
    static const Variant nullVariant;
    auto iter = kwargs().find(key);
    if (iter != kwargs().end())
        return iter->second;
    return nullVariant;
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
template <typename T, typename U>
T Payload<D,K>::kwargOr(
    const String& key, /**< The key to search under. */
    U&& fallback       /**< The fallback value to return if the key was
                                not found or cannot be converted. */
    ) const
{
    auto iter = kwargs().find(key);
    if (iter == kwargs().end())
        return std::forward<U>(fallback);

    try
    {
        return iter->second.template to<ValueTypeOf<T>>();
    }
    catch (const error::Conversion&)
    {
        return std::forward<U>(fallback);
    }
}

//------------------------------------------------------------------------------
/** @returns The option value, or an error code of either
             MiscErrc::absent or MiscErrc::badType. */
//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
template <typename T>
ErrorOr<T> Payload<D,K>::kwargAs(
    const String& key /**< The key to search under. */
    ) const &
{
    auto iter = kwargs().find(key);
    if (iter == kwargs().end())
        return makeUnexpectedError(MiscErrc::absent);
    if (!iter->second.template is<T>())
        return makeUnexpectedError(MiscErrc::badType);
    return iter->second.template as<T>();
}

//------------------------------------------------------------------------------
/** @returns The option value, or an error code of either
             MiscErrc::absent or MiscErrc::badType. */
//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
template <typename T>
ErrorOr<T> Payload<D,K>::kwargAs(
    const String& key /**< The key to search under. */
    ) &&
{
    auto iter = kwargs().find(key);
    if (iter == kwargs().end())
        return makeUnexpectedError(MiscErrc::absent);
    if (!iter->second.template is<T>())
        return makeUnexpectedError(MiscErrc::badType);
    return std::move(iter->second.template as<T>());
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
template <typename D, internal::MessageKind K>
template <typename... Ts>
size_t Payload<D,K>::convertTo(Ts&... values) const
{
    size_t index = 0;
    unbundleTo(args(), index, values...);
    return index;
}

template <typename D, internal::MessageKind K>
template <typename... Ts>
size_t Payload<D,K>::convertToTuple(std::tuple<Ts...>& tuple) const
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
template <typename D, internal::MessageKind K>
template <typename... Ts>
size_t Payload<D,K>::moveTo(Ts&... values)
{
    size_t index = 0;
    unbundleAs(args(), index, values...);
    return index;
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
template <typename... Ts>
size_t Payload<D,K>::moveToTuple(std::tuple<Ts...>& tuple)
{
    using More = std::integral_constant<bool, sizeof...(Ts) != 0>;
    return unbundleAsTuple<0>(args(), tuple, More{});
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
template <typename... Ts>
Payload<D,K>::Payload(in_place_t, Ts&&... fields)
    : Base(in_place, std::forward<Ts>(fields)...)
{
    // TODO: Don't add unused payload fields to outbound messages
    normalize();
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
template <internal::MessageKind M>
Payload<D,K>::Payload(internal::Command<M>&& command)
    : Base(std::move(command))
{
    normalize();
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
Payload<D,K>::Payload(internal::Message&& msg)
    : Base(std::move(msg))
{
    normalize();
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
void Payload<D,K>::bundle(Array&) {}

template <typename D, internal::MessageKind K>
template <typename T, typename... Ts>
void Payload<D,K>::bundle(Array& array, T&& head, Ts&&... tail)
{
    array.emplace_back(Variant::from(std::forward<T>(head)));
    bundle(array, tail...);
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
template <typename TTuple, std::size_t... Seq>
Array Payload<D,K>::bundleFromTuple(TTuple&& tuple, IndexSequence<Seq...>)
{
    return Array{Variant::from(std::get<Seq>(std::forward<TTuple>(tuple)))...};
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
void Payload<D,K>::unbundleTo(const Array&, size_t&) {}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
template <typename T, typename... Ts>
void Payload<D,K>::unbundleTo(const Array& array, size_t& index, T& head,
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
template <typename D, internal::MessageKind K>
template <size_t I, typename... Ts>
size_t Payload<D,K>::unbundleToTuple(
    const Array& array, std::tuple<Ts...>& tuple, TrueType)
{
    if (I < array.size())
    {
        using T = typename std::tuple_element<I, std::tuple<Ts...>>::type;
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

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
template <size_t I, typename... Ts>
size_t Payload<D,K>::unbundleToTuple(const Array&, std::tuple<Ts...>&,
                                     FalseType)
{
    return I;
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
void Payload<D,K>::unbundleAs(Array&, size_t&) {}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
void Payload<D,K>::normalize()
{
    auto& f = this->message().fields();
    assert(f.size() >= argsPos_);
    if (f.size() <= argsPos_)
        f.emplace_back(Array{});
    if (f.size() <= kwargsPos_)
        f.emplace_back(Object{});
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
void Payload<D,K>::setArgs() {}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
template <typename... Ts>
void Payload<D,K>::setArgs(Ts&&... args)
{
    Array array;
    bundle(array, std::forward<Ts>(args)...);
    withArgList(std::move(array));
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
template <typename T, typename... Ts>
void Payload<D,K>::unbundleAs(Array& array, size_t& index, T& head, Ts&... tail)
{
    using A = internal::VariantUncheckedAccess;

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
        head = std::move(A::alt<T>(arg));
        unbundleAs(array, ++index, tail...);
    }
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
template <size_t I, typename... Ts>
size_t Payload<D,K>::unbundleAsTuple(
    const Array& array, std::tuple<Ts...>& tuple, TrueType)
{
    using A = internal::VariantUncheckedAccess;

    if (I < array.size())
    {
        using T = typename std::tuple_element<I, std::tuple<Ts...>>::type;
        const auto& arg = array[I];
        if (!arg.template is<T>())
        {
            std::ostringstream oss;
            oss << "Payload element of type " << typeNameOf(arg)
                << " at index " << I
                << " is not of type: " << typeNameOf<T>();
            throw error::Access(oss.str());
        }
        std::get<I>(tuple) = std::move(A::alt<T>(arg));
        using More = std::integral_constant<bool, I+1 != sizeof...(Ts)>;
        return unbundleAsTuple<I+1, Ts...>(array, tuple, More{});
    }
    return I;
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
template <size_t I, typename... Ts>
size_t Payload<D,K>::unbundleAsTuple(const Array&, std::tuple<Ts...>&,
                                     FalseType)
{
    return I;
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
Array& Payload<D,K>::args(internal::PassKey)
{
    return args();
}

//------------------------------------------------------------------------------
template <typename D, internal::MessageKind K>
Object& Payload<D,K>::kwargs(internal::PassKey)
{
    return kwargs();
}

} // namespace wamp

#endif // CPPWAMP_PAYLOAD_HPP
