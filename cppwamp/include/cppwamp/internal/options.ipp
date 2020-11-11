/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <utility>

namespace wamp
{

//------------------------------------------------------------------------------
/** @returns A reference to the derived type to allow method chaining.
    @note If a value already exists under the key, it is not overwritten. */
//------------------------------------------------------------------------------
template <typename D>
D& Options<D>::withOption(String key, Variant value)
{
    options_.emplace(std::move(key), std::move(value));
    return static_cast<D&>(*this);
}

//------------------------------------------------------------------------------
/** @returns A reference to the derived type to allow method chaining.
    @note The entire dictionary is overwritten. */
//------------------------------------------------------------------------------
template <typename D>
D& Options<D>::withOptions(Object options)
{
    options_ = std::move(options);
    return static_cast<D&>(*this);
}

//------------------------------------------------------------------------------
template <typename D>
const Object& Options<D>::options() const {return options_;}

//------------------------------------------------------------------------------
/** @returns A Variant containing the value associated with the given key. If
             the key was not found, the returned Variant will be null. */
//------------------------------------------------------------------------------
template <typename D>
Variant Options<D>::optionByKey(const String& key) const
{
    Variant result;
    auto iter = options_.find(key);
    if (iter != options_.end())
        result = iter->second;
    return result;
}

//------------------------------------------------------------------------------
/** @returns If found, the option's value converted to T. Otherwise the given
             `fallback` value is returned.
    @throws error::Conversion if the found option cannot be converted to the
            target type. */
//------------------------------------------------------------------------------
template <typename D>
template <typename T>
#ifdef CPPWAMP_FOR_DOXYGEN
ValueTypeOf<T>
#else
typename Options<D>::template ValueTypeOf<T>
#endif
Options<D>::optionOr(
    const String& key, /**< The key to search under. */
    T&& fallback       /**< The fallback value to return if the key was
                            not found. */
) const
{
    auto iter = options_.find(key);
    if (iter != options_.end())
        return iter->second.template to<ValueTypeOf<T>>();
    else
        return std::forward<T>(fallback);
}

template <typename D>
Options<D>::Options() {}

template <typename D>
Options<D>::Options(Object options) : options_(std::move(options)) {}

template <typename D>
Object& Options<D>::options(wamp::internal::PassKey) {return options_;}


} // namespace wamp
