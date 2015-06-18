/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_VARIANTCONVERSION_HPP
#define CPPWAMP_VARIANTCONVERSION_HPP

#include <string>
#include "traits.hpp"

//------------------------------------------------------------------------------
/** @file
    Contains generic facilities for converting custom types to/from variants. */
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
/** Splits the `convert` free function for the given custom type.
    When split, the user must provide overloads for the `covertFrom` and
    `convertTo` functions. This can be useful when different behavior is
    required when converting to/from custom types.

    The `convertFrom` function converts from a Variant to the custom type, and
    should have the following signature:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    void convertFrom(FromVariantConverter&, Type&)
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    The `convertTo` function converts to a Variant from a custom type, and
    should have the following signature:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    void convertTo(ToVariantConverter&, const Type&)
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
//------------------------------------------------------------------------------
#define CPPWAMP_CONVERSION_SPLIT_FREE(Type)                     \
inline void convert(::wamp::FromVariantConverter& c, Type& obj) \
{                                                               \
    convertFrom(c, obj);                                        \
}                                                               \
                                                                \
inline void convert(::wamp::ToVariantConverter& c, Type& obj)   \
{                                                               \
    convertTo(c, const_cast<const Type&>(obj));                 \
}

//------------------------------------------------------------------------------
/** Splits the `convert` member function for the given custom type.
    When split, the user must provide the `covertFrom` and `convertTo`
    member functions. This can be useful when different behavior is
    required when converting to/from custom types.

    The `convertFrom` member function converts from a Variant to the custom
    type, and should have the following signature:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    void CustomType::convertFrom(FromVariantConverter&)
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    The `convertTo` member function converts to a Variant from a custom type,
    and should have the following signature:
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    void CustomType::convertTo(ToVariantConverter&) const
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
//------------------------------------------------------------------------------
#define CPPWAMP_CONVERSION_SPLIT_MEMBER(Type)                   \
inline void convert(::wamp::FromVariantConverter& c, Type& obj) \
{                                                               \
    wamp::ConversionAccess::convertFrom(c, obj);                \
}                                                               \
                                                                \
inline void convert(::wamp::ToVariantConverter& c, Type& obj)   \
{                                                               \
    const auto& constObj = obj;                                 \
    wamp::ConversionAccess::convertTo(c, constObj);             \
}

namespace wamp
{

class Variant; // Forward declaration

//------------------------------------------------------------------------------
/** Wrapper around a destination Variant, used for conversions.
    This wrapper provides a convenient, uniform syntax for inserting values
    into a destination variant. */
//------------------------------------------------------------------------------
class ToVariantConverter
{
public:
    /// Integer type used to represent the size of array variants.
    using SizeType = size_t;

    /// String type used to represent an object variant key.
    using String = std::string;

    /** Indicates that this converter is used to convert **to** a variant. */
    static constexpr bool convertingToVariant = true;

    /** Constructor taking a variant reference. */
    explicit ToVariantConverter(Variant& var);

    /** Makes the variant become an Array variant. */
    ToVariantConverter& size(SizeType n);

    /** Assigns a value to the variant. */
    template <typename T>
    ToVariantConverter& operator()(T&& value);

    /** Appends an array element to the variant. */
    template <typename T>
    ToVariantConverter& operator[](T&& value);

    /** Appends an object member to the variant. */
    template <typename T>
    ToVariantConverter& operator()(String key, T&& value);

    /** Appends an object member to the variant. */
    template <typename T, typename U>
    ToVariantConverter& operator()(String key, T&& value, U&& ignored);

    /** Returns a reference to the wrapped variant. */
    Variant& variant();

private:
    Variant& var_;
};

//------------------------------------------------------------------------------
/** Wrapper around a source Variant, used for conversions.
    This wrapper provides a convenient, uniform syntax for retrieving values
    from a source variant. */
//------------------------------------------------------------------------------
class FromVariantConverter
{
public:
    /// Integer type used to represent the size of array variants.
    using SizeType = size_t;

    /// String type used to represent an object variant key.
    using String = std::string;

    /** Indicates that this converter is used to convert **from** a variant. */
    static constexpr bool convertingToVariant = false;

    /** Constructor taking a constant variant reference. */
    explicit FromVariantConverter(const Variant& var);

    /** Obtains the current size of the variant. */
    SizeType size() const;

    /** Obtains the current size of the variant. */
    FromVariantConverter& size(SizeType& n);

    /** Retrieves a non-composite value from the variant. */
    template <typename T>
    FromVariantConverter& operator()(T& value);

    /** Retrieves the next element from an Array variant. */
    template <typename T>
    FromVariantConverter& operator[](T& value);

    /** Retrieves a member from an Object variant. */
    template <typename T>
    FromVariantConverter& operator()(const String& key, T& value);

    /** Retrieves a member from an Object variant, with a fallback value
        if the member is not found. */
    template <typename T, typename U>
    FromVariantConverter& operator()(const String& key, T& value, U&& fallback);

    /** Returns a constant reference to the wrapped variant. */
    const Variant& variant() const;

private:
    const Variant& var_;
    SizeType index_ = 0;
};

//------------------------------------------------------------------------------
/** Helper class used to gain access to private conversion member functions.
    If you make your conversion member functions private, then you must grant
    friendship to the ConversionAccess class. Other than granting friendship,
    users should not have to use this class. */
//------------------------------------------------------------------------------
class ConversionAccess
{
public:
    template <typename TConverter, typename TObject>
    static void convert(TConverter& c, TObject& obj);

    template <typename TObject>
    static void convertFrom(FromVariantConverter& c, TObject& obj);

    template <typename TObject>
    static void convertTo(ToVariantConverter& c, const TObject& obj);

private:
    CPPWAMP_GENERATE_HAS_MEMBER(convert)
    CPPWAMP_GENERATE_HAS_MEMBER(convertFrom)
    CPPWAMP_GENERATE_HAS_MEMBER(convertTo)
};

//------------------------------------------------------------------------------
/** General function for converting custom types to/from Variant.
    You must overload this function for custom types that you want to be
    convertible to/from Variant. Alternatively, you may also:
    - provide a `convert` member function within your custom type,
    - use @ref CPPWAMP_CONVERSION_SPLIT_FREE and provide split `convertTo`
      and `convertFrom` free functions, or,
    - use @ref CPPWAMP_CONVERSION_SPLIT_MEMBER and provide split `convertTo`
      and `convertFrom` member functions. */
//------------------------------------------------------------------------------
template <typename TConverter, typename TValue>
void convert(TConverter& c, TValue& val);

} // namespace wamp

#include "./internal/conversion.ipp"

#endif // CPPWAMP_VARIANTCONVERSION_HPP
