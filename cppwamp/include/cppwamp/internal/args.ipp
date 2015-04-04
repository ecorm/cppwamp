/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <utility>

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
inline Args::Args() {}

//------------------------------------------------------------------------------
/** @post `std::equal(positional.begin(), positional.end(), this->list) == true` */
//------------------------------------------------------------------------------
inline Args::Args(std::initializer_list<Variant> positional)
    : list(positional) {}

//------------------------------------------------------------------------------
/** @param withPairs Tag value used to distinguish from the other
                     `initializer_list` overload. Use the wamp::withPairs
                     constant for this parameter.
    @param pairs Braced initializer list of keyword/variant pairs. */
//------------------------------------------------------------------------------
inline Args::Args(WithPairs, PairInitializerList pairs)
    : map(pairs)
{}

//------------------------------------------------------------------------------
/** @details
    The array of variants is copied to the Args::list public member.
    @param with Tag value used to distinguish from the `initializer_list`
                overloads. Use the wamp::with constant for this parameter.
    @param list An array of variants to copy to the Args::list member.
    @post `this->list == list` */
//------------------------------------------------------------------------------
inline Args::Args(With, Array list)
    : list(std::move(list))
{}

//------------------------------------------------------------------------------
/** @details
    The map of variants is copied to the Args::map public member.
    @param with Tag value used to distinguish from the `initializer_list`
                overloads. Use the wamp::with constant for this parameter.
    @param map A map of variants to copy to the Args::map member.
    @post `this->map == map` */
//------------------------------------------------------------------------------
inline Args::Args(With, Object map)
    : map(std::move(map))
{}

//------------------------------------------------------------------------------
/** @details
    The array of variants is copied to the Args::list public member.
    The map of variants is copied to the Args::map public member.
    @param with Tag value used to distinguish from the `initializer_list`
                overloads. Use the wamp::with constant for this parameter.
    @param list An array of variants to copy to the Args::list member.
    @param map A map of variants to copy to the Args::map member.
    @post `this->list == array`
    @post `this->map == map` */
//------------------------------------------------------------------------------
inline Args::Args(With, Array list, Object map)
    : list(std::move(list)), map(std::move(map))
{}

//------------------------------------------------------------------------------
/** @return The number of elements that were converted
    @pre Args::list elements are convertible to their target types
    @post `std::min(this->list->size(), sizeof...(Ts))` elements are converted
    @throws error::Conversion if an argument cannot be converted to the
            target type. */
//------------------------------------------------------------------------------
template <typename... Ts>
size_t Args::to(Ts&... vars) const
{
    size_t index = 0;
    internal::unbundleTo(list, index, vars...);
    return index;
}

//------------------------------------------------------------------------------
/** @return The number of elements that were moved
    @pre Args::list elements are accessible as their target types
    @post `std::min(this->list->size(), sizeof...(Ts))` elements are moved
    @post The moved elements in Args::list are nullified
    @throws error::Access if an argument's dynamic type does not match its
            associated target type. */
//------------------------------------------------------------------------------
template <typename... Ts>
size_t Args::move(Ts&... vars)
{
    size_t index = 0;
    internal::unbundleAs(list, index, vars...);
    return index;
}

//------------------------------------------------------------------------------
/** @details
    This is a convenience function identical to using `args.list.at(index)`.
    @pre `this->list.size() > index`
    @throws std::out_of_range if the given index is not within the range
            of Args::list. */
//------------------------------------------------------------------------------
inline Variant& Args::operator[](size_t index) {return list.at(index);}

//------------------------------------------------------------------------------
/** @details
    This is a convenience function identical to using `args.list.at(index)`.
    @pre `this->list.size() > index`
    @throws std::out_of_range if the given index is not within the range
            of Args::list. */
//------------------------------------------------------------------------------
inline const Variant& Args::operator[](size_t index) const
    {return list.at(index);}

//------------------------------------------------------------------------------
/** @details
    This is a convenience function identical to using `args.map[key]`. */
//------------------------------------------------------------------------------
inline Variant& Args::operator[](const std::string& keyword)
    {return map[keyword];}

//------------------------------------------------------------------------------
/** @return `true` iff `this->list == rhs.list && this->map == rhs.map` */
//------------------------------------------------------------------------------
inline bool Args::operator==(const Args& rhs) const
{
    return (list == rhs.list) && (map == rhs.map);
}

//------------------------------------------------------------------------------
/** @return `true` iff `this->list != rhs.list || this->map != rhs.map` */
//------------------------------------------------------------------------------
inline bool Args::operator!=(const Args& rhs) const
{
    return (list != rhs.list) || (map != rhs.map);
}

//------------------------------------------------------------------------------
inline std::ostream& operator<<(std::ostream& out, const Args& args)
{
    return out << "Args{" << args.list << ',' << args.map << '}';
}

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
