/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <sstream>
#include <utility>
#include "../error.hpp"
#include "../variant.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
inline ToVariantConverter::ToVariantConverter(Variant& var) : var_(var) {}

/** @details
    The array will `reserve` space for `n` elements.
    @post `this->variant().is<Array> == true`
    @post `this->variant().as<Array>.capacity() >= n` */
inline ToVariantConverter& ToVariantConverter::size(SizeType n)
{
    Array array;
    array.reserve(n);
    var_ = std::move(array);
    return *this;
}

/** @details
    The given value is converted to a Variant via Variant::from before
    being assigned. */
template <typename T>
ToVariantConverter& ToVariantConverter::operator()(T&& value)
{
    var_ = Variant::from(std::forward<T>(value));
    return *this;
}

/** @details
    If the destination Variant is not already an Array, it will be transformed
    into an array and all previously stored values will be cleared.
    @post `this->variant().is<Array> == true` */
template <typename T>
ToVariantConverter& ToVariantConverter::operator[](T&& value)
{
    if (!var_.is<Array>())
        var_ = Array();
    auto& array = var_.as<Array>();
    array.emplace_back(Variant::from(std::forward<T>(value)));
    return *this;
}

/** @details
    If the destination Variant is not already an Object, it will be transformed
    into an object and all previously stored values will be cleared.
    @post `this->variant().is<Object> == true` */
template <typename T>
ToVariantConverter& ToVariantConverter::operator()(String key, T&& value)
{
    if (!var_.is<Object>())
        var_ = Object();
    auto& object = var_.as<Object>();
    object.emplace( std::move(key),
                    Variant::from(std::forward<T>(value)) );
    return *this;
}

/** @details
    If the destination Variant is not already an Object, it will be transformed
    into an object and all previously stored values will be cleared.
    @post `this->variant().is<Object> == true` */
template <typename T, typename U>
ToVariantConverter& ToVariantConverter::operator()(String key, T&& value, U&&)
{
    return operator()(std::move(key), std::forward<T>(value));
}

inline Variant& ToVariantConverter::variant() {return var_;}


//------------------------------------------------------------------------------
inline FromVariantConverter::FromVariantConverter(const Variant& var)
    : var_(var)
{}

/** @details
    Returns this->variant()->size().
    @see Variant::size */
inline FromVariantConverter::SizeType FromVariantConverter::size() const
{
    return var_.size();
}

/** @details
    Returns this->variant()->size().
    @see Variant::size */
inline FromVariantConverter& FromVariantConverter::size(SizeType& n)
{
    n = var_.size();
    return *this;
}

/** @details
    The variant's value is converted to the destination type via
    Variant::to.
    @pre The variant is convertible to the destination type.
    @throws error::Conversion if the variant is not convertible to the
            destination type. */
template <typename T>
FromVariantConverter& FromVariantConverter::operator()(T& value)
{
    var_.to(value);
    return *this;
}

/** @details
    The element is converted to the destination type via Variant::to.
    @pre The variant is an array.
    @pre There are elements left in the variant array.
    @pre The element is convertible to the destination type.
    @throws error::Conversion if the variant is not an array.
    @throws error::Conversion if there are no elements left in the
            variant array.
    @throws error::Conversion if the element is not convertible to the
            destination type. */
template <typename T>
FromVariantConverter& FromVariantConverter::operator[](T& value)
{
    try
    {
        var_.at(index_).to(value);
        ++index_;
    }
    catch (const error::Conversion& e)
    {
        std::ostringstream oss;
        oss << e.what() << ", for array index " << index_;
        throw error::Conversion(oss.str());
    }
    catch (const error::Access&)
    {
        std::ostringstream oss;
        oss << "wamp::error::Conversion: Attemping to access field type "
            << typeNameOf(var_) << "as array";
        throw error::Conversion(oss.str());
    }
    catch (const std::out_of_range&)
    {
        std::ostringstream oss;
        oss << "wamp::error::Conversion: Cannot extract more than " << index_
            << " elements from the array";
        throw error::Conversion(oss.str());
    }

    return *this;
}

/** @details
    The member is converted to the destination type via Variant::to.
    @pre The variant is an object.
    @pre There exists a member with the given key.
    @pre The member is convertible to the destination type.
    @throws error::Conversion if the variant is not an object.
    @throws error::Conversion if there doesn't exist a member with the
            given key.
    @throws error::Conversion if the member is not convertible to the
            destination type. */
template <typename T>
FromVariantConverter& FromVariantConverter::operator()(const String& key,
                                                       T& value)
{
    try
    {
        var_.at(key).to(value);
    }
    catch (const error::Conversion& e)
    {
        std::ostringstream oss;
        oss << e.what() << ", for object member \"" << key << '"';
        throw error::Conversion(oss.str());
    }
    catch (const error::Access&)
    {
        std::ostringstream oss;
        oss << "wamp::error::Conversion: Attemping to access field type "
            << typeNameOf(var_) << "as object using key \"" << key << '"';
        throw error::Conversion(oss.str());
    }
    catch (const std::out_of_range&)
    {
        std::ostringstream oss;
        oss << "wamp::error::Conversion: Key \"" << key
            << "\" not found in object";
        throw error::Conversion(oss.str());
    }

    return *this;
}

/** @details
    The member is converted to the destination type via Variant::to.
    @pre The variant is an object.
    @pre The member, if it exists, is convertible to the destination type.
    @throws error::Conversion if the variant is not an object.
    @throws error::Conversion if the existing member is not convertible to the
            destination type. */
template <typename T, typename U>
FromVariantConverter& FromVariantConverter::operator()(const String& key,
                                                       T& value, U&& fallback)
{
    try
    {
        auto& obj = var_.as<Object>();
        auto kv = obj.find(key);
        if (kv != obj.end())
            kv->second.to(value);
        else
            value = std::forward<U>(fallback);
    }
    catch (const error::Conversion& e)
    {
        std::ostringstream oss;
        oss << e.what() << ", for object member \""  << key << '"';
        throw error::Conversion(oss.str());
    }
    catch (const error::Access&)
    {
        std::ostringstream oss;
        oss << "wamp::error::Conversion: Attemping to access field type "
            << typeNameOf(var_) << "as object using key \"" << key << '"';
        throw error::Conversion(oss.str());
    }

    return *this;
}

inline const Variant& FromVariantConverter::variant() const {return var_;}


//------------------------------------------------------------------------------
template <typename TConverter, typename TObject>
void ConversionAccess::convert(TConverter& c, TObject& obj)
{
    static_assert(has_member_convert<TObject>(),
        "The 'convert' function has not been specialized for this type. "
        "Either provide a 'convert' free function specialization, or "
        "a 'convert' member function.");
    obj.convert(c);
}

template <typename TObject>
void ConversionAccess::convertFrom(FromVariantConverter& c, TObject& obj)
{
    static_assert(has_member_convertFrom<TObject>(),
        "The 'convertFrom' member function has not been provided "
        "for this type.");
    obj.convertFrom(c);
}

template <typename TObject>
void ConversionAccess::convertTo(ToVariantConverter& c, const TObject& obj)
{
    static_assert(has_member_convertTo<TObject>(),
        "The 'convertTo' member function has not been provided for this type.");
    obj.convertTo(c);
}

template <typename TObject>
TObject ConversionAccess::defaultConstruct() {return TObject();}


//------------------------------------------------------------------------------
template <typename TConverter, typename TValue>
void convert(TConverter& c, TValue& val)
{
    // Fall back to intrusive conversion if 'convert' was not specialized
    // for the given type.
    ConversionAccess::convert(c, val);
}

} // namespace wamp

