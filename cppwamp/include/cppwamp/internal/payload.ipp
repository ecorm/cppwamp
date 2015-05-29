/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <utility>
#include "config.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
inline void bundle(Array&) {}

template <typename T, typename... Ts>
void bundle(Array& array, T&& head, Ts&&... tail)
{
    array.emplace_back(Variant::from(std::forward<T>(head)));
    internal::bundle(array, tail...);
}

//------------------------------------------------------------------------------
inline void unbundleTo(const Array&, size_t&) {}

template <typename T, typename... Ts>
void unbundleTo(const Array& array, size_t& index, T& head, Ts&... tail)
{
    if (index < array.size())
    {
        head = array[index].to<T>();
        internal::unbundleTo(array, ++index, tail...);
    }
}

//------------------------------------------------------------------------------
inline void unbundleAs(Array&, size_t&) {}

template <typename T, typename... Ts>
void unbundleAs(Array& array, size_t& index, T& head, Ts&... tail)
{
    if (index < array.size())
    {
        head = std::move(array[index].as<T>());
        internal::unbundleAs(array, ++index, tail...);
    }
}

} // namespace internal


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
    internal::bundle(array, std::forward<Ts>(args)...);
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
    ~~~
    Array mine = std::move(payload).args();
    ~~~
    @post this->args().empty() == true */
//------------------------------------------------------------------------------
template <typename D>
Array Payload<D>::args() &&
{
    Array result = std::move(args_);
    args_.clear();
    return std::move(result);
}

//------------------------------------------------------------------------------
template <typename D>
const Object& Payload<D>::kwargs() const & {return kwargs_;}

//------------------------------------------------------------------------------
/** @details
    This overload takes effect when `*this` is an r-value. For example:
    ~~~
    Object mine = std::move(payload).kwargs();
    ~~~
    @post this->kwargs().empty() == true */
//------------------------------------------------------------------------------
template <typename D>
Object Payload<D>::kwargs() &&
{
    Object result = std::move(kwargs_);
    kwargs_.clear();
    return std::move(result);
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
    {return args_.at(index);}

//------------------------------------------------------------------------------
/** @details
    If the key doesn't exist, a null variant is inserted under the key
    before the reference is returned. */
//------------------------------------------------------------------------------
template <typename D>
Variant& Payload<D>::operator[](const std::string& keyword)
    {return kwargs_[keyword];}

//------------------------------------------------------------------------------
/** @par Example
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Result derives from Payload
    Result result = session->call("rpc", yield);
    std::string s;
    int n = 0;
    result.convertTo(s, n);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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
    internal::unbundleTo(args_, index, values...);
    return index;
}

//------------------------------------------------------------------------------
/** @par Example
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Result derives from Payload
    Result result = session->call("rpc", yield);
    String s;
    Int n = 0;
    result.moveTo(s, n);
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

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
    internal::unbundleAs(args_, index, values...);
    return index;
}

//------------------------------------------------------------------------------
template <typename D>
Payload<D>::Payload() {}

//------------------------------------------------------------------------------
template <typename D>
Array& Payload<D>::args(internal::PassKey) {return args_;}

//------------------------------------------------------------------------------
template <typename D>
Object& Payload<D>::kwargs(internal::PassKey) {return kwargs_;}


} // namespace wamp
