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

//------------------------------------------------------------------------------
template <size_t N, typename T, typename... Ts>
struct Unmarshall
{
    template <typename TFunctor, typename... TArgs>
    static void apply(TFunctor&& fn, const Array& array);

    template <typename TFunctor, typename... TArgs>
    static void apply(TFunctor&& fn, const Array& array, TArgs&&... args);
};

template <size_t N, typename T>
struct Unmarshall<N, T>
{
    template <typename TFunctor>
    static void apply(TFunctor&& fn, const Array& array)
    {
        fn(array.at(N).to<T>());
    }

    template <typename TFunctor, typename... TArgs>
    static void apply(TFunctor&& fn, const Array& array, TArgs&&... args)
    {
        fn(std::forward<TArgs>(args)..., array.at(N).to<T>());
    }
};

template <size_t N, typename T, typename... Ts>
template <typename TFunctor, typename... TArgs>
void Unmarshall<N,T,Ts...>::apply(TFunctor&& fn, const Array& array)
{
    using std::forward;
    Unmarshall<N+1, Ts...>::apply(forward<TFunctor>(fn), array,
                                  array.at(N).to<T>());
}

template <size_t N, typename T, typename... Ts>
template <typename TFunctor, typename... TArgs>
void Unmarshall<N,T,Ts...>::apply(TFunctor&& fn, const Array& array,
                                  TArgs&&... args)
{
    using std::forward;
    Unmarshall<N+1, Ts...>::apply(forward<TFunctor>(fn), array,
            forward<TArgs>(args)..., array.at(N).to<T>());
}

} // namespace internal


//------------------------------------------------------------------------------
/** @post `std::equal(list.begin(), list.end(), this->args()) == true` */
//------------------------------------------------------------------------------
template <typename D>
Payload<D>::Payload(std::initializer_list<Variant> list)
    : args_(list) {}

//------------------------------------------------------------------------------
/** @post `std::equal(list.begin(), list.end(), this->args()) == true` */
//------------------------------------------------------------------------------
template <typename D>
D& Payload<D>::withArgs(Array args)
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

#if CPPWAMP_HAS_REF_QUALIFIERS

//------------------------------------------------------------------------------
template <typename D>
const Array& Payload<D>::args() const & {return args_;}

#else

//------------------------------------------------------------------------------
template <typename D>
const Array& Payload<D>::args() const {return args_;}

#endif

#if CPPWAMP_HAS_REF_QUALIFIERS
//------------------------------------------------------------------------------
/** @details
    This overload takes effect when `*this` is an r-value. For example:
    ~~~
    Array mine = std::move(payload).args();
    ~~~
    @post this->args().empty() == true */
//------------------------------------------------------------------------------
template <typename D>
Array Payload<D>::args() && {return moveArgs();}
#endif

//------------------------------------------------------------------------------
/** @details
    This function is provided as a workaround for platforms that don't support
    ref qualifiers.
    @post this->args().empty() == true */
//------------------------------------------------------------------------------
template <typename D>
Array Payload<D>::moveArgs()
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
Object Payload<D>::kwargs() &&  {return moveKwargs();}

//------------------------------------------------------------------------------
/** @post this->kwargs().empty() == true */
//------------------------------------------------------------------------------
template <typename D>
Object Payload<D>::moveKwargs()
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


//------------------------------------------------------------------------------
/** @tparam TFunction (Deduced) The type of the callable target
    @pre The Array elements must be convertible to `TArgs...`
    @throws error::Conversion if an Array element is not convertible to its
            associated function argument types. */
//------------------------------------------------------------------------------
template <typename... TArgs>
template <typename TFunction>
void Unmarshall<TArgs...>::apply(
    TFunction&& fn,     /**< The target function to call. */
    const Array& array  /**< An array of variants representing positional
                             arguments. */
)
{
    internal::Unmarshall<0,TArgs...>::apply(std::forward<TFunction>(fn), array);
}

//------------------------------------------------------------------------------
/** @details
    This overload allows the caller to pass extra arguments (`preargs`)
    _before_ the ones generated from the Array elements.
    @tparam TFunction (Deduced) The type of the callable target
    @pre The Array elements must be convertible to `TArgs...`
    @throws error::Conversion if an Array element is not convertible to its
            associated function argument types. */
//------------------------------------------------------------------------------
template <typename... TArgs>
template <typename TFunction, typename... TPreargs>
void Unmarshall<TArgs...>::apply(
    TFunction&& fn,       /**< The target function to call. */
    const Array& array,   /**< An array of variants representing positional
                               arguments. */
    TPreargs&&... preargs /**< Extra arguments to be passed before the ones
                               generated from the Array elements. */
)
{
    internal::Unmarshall<0,TArgs...>::apply(std::forward<TFunction>(fn), array,
                                            std::forward<TPreargs>(preargs)...);
}

} // namespace wamp
