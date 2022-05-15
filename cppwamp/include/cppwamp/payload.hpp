/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
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
#include <utility>
#include "api.hpp"
#include "variant.hpp"
#include "./internal/passkey.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains a payload of positional and keyword arguments exchanged with a
    WAMP peer. */
//------------------------------------------------------------------------------
template <typename TDerived>
class CPPWAMP_API Payload
{
public:
    /** Converting constructor taking a braced initializer list of positional
        variant arguments. */
    Payload(std::initializer_list<Variant> list);

    /** Sets the positional arguments for this payload. */
    template <typename... Ts>
    TDerived& withArgs(Ts&&... args);

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
    template <typename... Ts> size_t convertTo(Ts&... values) const;

    /** Moves the payload's positional arguments to the given value
        references. */
    template <typename... Ts> size_t moveTo(Ts&... values);

protected:
    Payload();

private:
    CPPWAMP_HIDDEN static void bundle(Array&);

    template <typename T, typename... Ts>
    CPPWAMP_HIDDEN static void bundle(Array& array, T&& head, Ts&&... tail);

    CPPWAMP_HIDDEN static void unbundleTo(const Array&, size_t&);

    template <typename T, typename... Ts>
    CPPWAMP_HIDDEN static void unbundleTo(const Array& array, size_t& index,
                                          T& head, Ts&... tail);

    CPPWAMP_HIDDEN static void unbundleAs(Array&, size_t&);

    template <typename T, typename... Ts>
    CPPWAMP_HIDDEN static void unbundleAs(Array& array, size_t& index, T& head,
                                          Ts&... tail);

    Array args_;    // List of positional arguments.
    Object kwargs_; // Dictionary of keyword arguments.

public:
    // Internal use only
    CPPWAMP_HIDDEN Array& args(internal::PassKey);
    CPPWAMP_HIDDEN Object& kwargs(internal::PassKey);
};


//******************************************************************************
// Payload implementation
//******************************************************************************

//------------------------------------------------------------------------------
/** @post `std::equal(list.begin(), list.end(), this->args()) == true` */
//------------------------------------------------------------------------------
template <typename D>
Payload<D>::Payload(std::initializer_list<Variant> list)
    : args_(list) {}

//------------------------------------------------------------------------------
/** Each argument is converted to a Variant using Variant::from. This allows
    custom types to be passed in, as long as the `convert` function is
    specialized for those custom types. */
//------------------------------------------------------------------------------
template <typename D>
template <typename... Ts>
D& Payload<D>::withArgs(Ts&&... args)
{
    Array array;
    bundle(array, std::forward<Ts>(args)...);
    return withArgList(std::move(array));
}

//------------------------------------------------------------------------------
/** @post `std::equal(list.begin(), list.end(), this->args()) == true` */
//------------------------------------------------------------------------------
template <typename D>
D& Payload<D>::withArgList(Array args)
{
    args_ = std::move(args);
    return static_cast<D&>(*this);
}

//------------------------------------------------------------------------------
/** @post `std::equal(map.begin(), map.end(), this->kwargs()) == true` */
//------------------------------------------------------------------------------
template <typename D>
D& Payload<D>::withKwargs(Object kwargs)
{
    kwargs_ = std::move(kwargs);
    return static_cast<D&>(*this);
}

//------------------------------------------------------------------------------
template <typename D>
const Array& Payload<D>::args() const & {return args_;}

//------------------------------------------------------------------------------
/** @details
This overload takes effect when `*this` is an r-value. For example:
```
Array mine = std::move(payload).args();
```
@post this->args().empty() == true */
//------------------------------------------------------------------------------
template <typename D>
Array Payload<D>::args() &&
{
    Array result = std::move(args_);
    args_.clear();
    return result;
}

//------------------------------------------------------------------------------
template <typename D>
const Object& Payload<D>::kwargs() const & {return kwargs_;}

//------------------------------------------------------------------------------
/** @details
This overload takes effect when `*this` is an r-value. For example:
```
Object mine = std::move(payload).kwargs();
```
@post this->kwargs().empty() == true */
//------------------------------------------------------------------------------
template <typename D>
Object Payload<D>::kwargs() &&
{
    Object result = std::move(kwargs_);
    kwargs_.clear();
    return result;
}

//------------------------------------------------------------------------------
/** @details
    @pre `this->args().size() > index`
    @throws std::out_of_range if the given index is not within the range
            of this->args(). */
//------------------------------------------------------------------------------
template <typename D>
Variant& Payload<D>::operator[](size_t index) {return args_.at(index);}

//------------------------------------------------------------------------------
/** @details
    @pre `this->args().size() > index`
    @throws std::out_of_range if the given index is not within the range
            of this->args(). */
//------------------------------------------------------------------------------
template <typename D>
const Variant& Payload<D>::operator[](size_t index) const
{
    return args_.at(index);
}

//------------------------------------------------------------------------------
/** @details
    If the key doesn't exist, a null variant is inserted under the key
    before the reference is returned. */
//------------------------------------------------------------------------------
template <typename D>
Variant& Payload<D>::operator[](const std::string& keyword)
{
    return kwargs_[keyword];
}

//------------------------------------------------------------------------------
/** @par Example
```
// Result derives from Payload
Result result = session->call("rpc", yield);
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
template <typename D>
template <typename... Ts>
size_t Payload<D>::convertTo(Ts&... values) const
{
    size_t index = 0;
    unbundleTo(args_, index, values...);
    return index;
}

//------------------------------------------------------------------------------
/** @par Example
```
// Result derives from Payload
Result result = session->call("rpc", yield);
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
template <typename D>
template <typename... Ts>
size_t Payload<D>::moveTo(Ts&... values)
{
    size_t index = 0;
    unbundleAs(args_, index, values...);
    return index;
}

//------------------------------------------------------------------------------
template <typename D>
Payload<D>::Payload() {}

//------------------------------------------------------------------------------
template <typename D>
void Payload<D>::bundle(Array&) {}

template <typename D>
template <typename T, typename... Ts>
void Payload<D>::bundle(Array& array, T&& head, Ts&&... tail)
{
    array.emplace_back(Variant::from(std::forward<T>(head)));
    bundle(array, tail...);
}

//------------------------------------------------------------------------------
template <typename D>
void Payload<D>::unbundleTo(const Array&, size_t&) {}

template <typename D>
template <typename T, typename... Ts>
void Payload<D>::unbundleTo(const Array& array, size_t& index, T& head,
                            Ts&... tail)
{
    if (index < array.size())
    {
        head = array[index].to<T>();
        unbundleTo(array, ++index, tail...);
    }
}

//------------------------------------------------------------------------------
template <typename D>
void Payload<D>::unbundleAs(Array&, size_t&) {}

template <typename D>
template <typename T, typename... Ts>
void Payload<D>::unbundleAs(Array& array, size_t& index, T& head, Ts&... tail)
{
    if (index < array.size())
    {
        head = std::move(array[index].as<T>());
        unbundleAs(array, ++index, tail...);
    }
}

//------------------------------------------------------------------------------
template <typename D>
Array& Payload<D>::args(internal::PassKey) {return args_;}

//------------------------------------------------------------------------------
template <typename D>
Object& Payload<D>::kwargs(internal::PassKey) {return kwargs_;}

} // namespace wamp

#endif // CPPWAMP_PAYLOAD_HPP
