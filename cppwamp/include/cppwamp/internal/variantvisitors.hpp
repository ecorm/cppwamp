/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_VARIANT_VISITORS_HPP
#define CPPWAMP_INTERNAL_VARIANT_VISITORS_HPP

#include "../blob.hpp"
#include "../error.hpp"
#include "../null.hpp"
#include "../traits.hpp"
#include "../variantdefs.hpp"
#include "../visitor.hpp"
#include "varianttraits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TVariant>
class VariantEquivalentTo : public Visitor<bool>
{
private:
    template <typename T>
    static constexpr bool isBoolProxy()
    {
        // The !isSameType is required because std::vector<bool>::const_reference
        // may or may not just be bool.
        return !isSameType<T, bool>() &&
               (isSameType<T, std::vector<bool>::reference>() ||
                isSameType<T, std::vector<bool>::const_reference>());
    }

public:
    using ArrayType = typename TVariant::Array;
    using ObjectType = typename TVariant::Object;

    template <typename TField>
    bool operator()(const TField& lhs, const TField& rhs) const
    {
        return lhs == rhs;
    }

    template <typename TArg, Needs<isBoolProxy<TArg>()> = 0>
    bool operator()(const bool lhs, const TArg rhs) const
    {
        return lhs == bool(rhs);
    }

    template <typename TField, typename TArg>
    bool operator()(const TField& lhs, const TArg& rhs) const
    {
        using BothAreNumbers = MetaBool<isNumber<TField>() && isNumber<TArg>()>;
        return compare(BothAreNumbers{}, lhs, rhs);
    }

    template <typename TElem, Needs<!isSameType<TElem, Variant>()> = 0>
    bool operator()(const ArrayType& lhs, const std::vector<TElem>& rhs) const
    {
        using VecConstRef = typename std::vector<TElem>::const_reference;
        // clang libc++ parameterizes equality comparison for default binary
        // predicate on iterator value type instead of const reference type,
        // which leads to an ambiguous function resolution.
        return (lhs.size() != rhs.size()) ? false :
                   std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(),
                              [](const Variant& lElem, VecConstRef rElem)
                              {return lElem == rElem;});
    }

    template <typename TValue, Needs<!isSameType<TValue, Variant>()> = 0>
    bool operator()(const ObjectType& lhs,
                    const std::map<String, TValue>& rhs) const
    {
        using Map        = std::map<String, TValue>;
        using MapPair    = typename Map::value_type;
        using ObjectPair = typename ObjectType::value_type;
        return (lhs.size() != rhs.size()) ? false :
                   std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(),
                              [](const ObjectPair& lPair, const MapPair& rPair)
                              {
                                  return lPair.first == rPair.first &&
                                         lPair.second == rPair.second;
                              });
    }

private:
    template <typename TField, typename TArg>
    static bool compare(FalseType, const TField&, const TArg&)
    {
        return false;
    }

    template <typename TField, typename TArg>
    static bool compare(TrueType, const TField& lhs, const TArg& rhs)
    {
        // Avoid directly comparing mixed signed/unsigned numbers
        using LhsIsSigned = typename std::is_signed<TField>;
        using RhsIsSigned = typename std::is_signed<TArg>;
        return compareNumbers(LhsIsSigned(), RhsIsSigned(), lhs, rhs);
    }

    template <typename TField, typename TArg>
    static bool compareNumbers(FalseType, FalseType,
                               const TField lhs, const TArg rhs)
    {
        return lhs == rhs;
    }

    template <typename TField, typename TArg>
    static bool compareNumbers(FalseType, TrueType,
                               const TField lhs, const TArg rhs)
    {
        if (rhs < 0)
            return false;
        using CT = typename std::common_type<TField, TArg>::type;
        return static_cast<CT>(lhs) == static_cast<CT>(rhs);
    }

    template <typename TField, typename TArg>
    static bool compareNumbers(TrueType, FalseType,
                               const TField lhs, const TArg rhs)
    {
        if (lhs < 0)
            return false;
        using CT = typename std::common_type<TField, TArg>::type;
        return static_cast<CT>(lhs) == static_cast<CT>(rhs);
    }

    template <typename TField, typename TArg>
    static bool compareNumbers(TrueType, TrueType,
                               const TField lhs, const TArg rhs)
    {
        return lhs == rhs;
    }
};

//------------------------------------------------------------------------------
template <typename TVariant>
class VariantNotEquivalentTo : public Visitor<bool>
{
private:
    template <typename T>
    static constexpr bool isBoolProxy()
    {
        // The !isSameType is required because std::vector<bool>::const_reference
        // may or may not just be bool.
        return !isSameType<T, bool>() &&
               (isSameType<T, std::vector<bool>::reference>() ||
                isSameType<T, std::vector<bool>::const_reference>());
    }

public:
    using ArrayType = typename TVariant::Array;
    using ObjectType = typename TVariant::Object;

    template <typename TField>
    bool operator()(const TField& lhs, const TField& rhs) const
    {return lhs != rhs;}

    template <typename TArg, Needs<isBoolProxy<TArg>()> = 0>
    bool operator()(const bool lhs, const TArg rhs) const
    {return lhs != rhs;}

    template <typename TField, typename TArg>
    bool operator()(const TField& lhs, const TArg& rhs) const
    {
        using BothAreNumbers = MetaBool<isNumber<TField>() && isNumber<TArg>()>;
        return compare(BothAreNumbers{}, lhs, rhs);
    }

    template <typename TElem, Needs<!isSameType<TElem, Variant>()> = 0>
    bool operator()(const ArrayType& lhs, const std::vector<TElem>& rhs) const
    {
        using VecConstRef = typename std::vector<TElem>::const_reference;
        // clang libc++ parameterizes equality comparison for default binary
        // predicate on iterator value type instead of const reference type,
        // which leads to an ambiguous function resolution.
        return (lhs.size() != rhs.size()) ? true :
                   std::mismatch(lhs.cbegin(), lhs.cend(), rhs.cbegin(),
                                 [](const Variant& lElem, VecConstRef rElem)
                                 {return lElem == rElem;}).first != lhs.cend();
    }

    template <typename TValue, Needs<!isSameType<TValue, Variant>()> = 0>
    bool operator()(const ObjectType& lhs,
                    const std::map<String, TValue>& rhs) const
    {
        using Map        = std::map<String, TValue>;
        using MapPair    = typename Map::value_type;
        using ObjectPair = typename ObjectType::value_type;

        auto comp = [](const ObjectPair& lPair, const MapPair& rPair)
        {
            return lPair.first == rPair.first &&
                   lPair.second == rPair.second;
        };

        return (lhs.size() != rhs.size()) ? true :
                   ( std::mismatch(lhs.cbegin(), lhs.cend(), rhs.cbegin(),
                                  std::move(comp)).first != lhs.end() );
    }

private:
    template <typename TField, typename TArg>
    static bool compare(FalseType, const TField&, const TArg&)
    {
        return true;
    }

    template <typename TField, typename TArg>
    static bool compare(TrueType, const TField& lhs, const TArg& rhs)
    {
        // Avoid directly comparing mixed signed/unsigned numbers
        using LhsIsSigned = typename std::is_signed<TField>;
        using RhsIsSigned = typename std::is_signed<TArg>;
        return compareNumbers(LhsIsSigned(), RhsIsSigned(), lhs, rhs);
    }

    template <typename TField, typename TArg>
    static bool compareNumbers(FalseType, FalseType,
                               const TField lhs, const TArg rhs)
    {
        return lhs != rhs;
    }

    template <typename TField, typename TArg>
    static bool compareNumbers(FalseType, TrueType,
                               const TField lhs, const TArg rhs)
    {
        if (rhs < 0)
            return true;
        using CT = typename std::common_type<TField, TArg>::type;
        return static_cast<CT>(lhs) != static_cast<CT>(rhs);
    }

    template <typename TField, typename TArg>
    static bool compareNumbers(TrueType, FalseType,
                               const TField lhs, const TArg rhs)
    {
        if (lhs < 0)
            return true;
        using CT = typename std::common_type<TField, TArg>::type;
        return static_cast<CT>(lhs) != static_cast<CT>(rhs);
    }

    template <typename TField, typename TArg>
    static bool compareNumbers(TrueType, TrueType,
                               const TField lhs, const TArg rhs)
    {
        return lhs != rhs;
    }
};


//------------------------------------------------------------------------------
template <typename TVariant>
class VariantIsConvertibleTo : public Visitor<bool>
{
public:
    using ArrayType = typename TVariant::Array;
    using ObjectType = typename TVariant::Array;

    template <typename T> struct Tag {};

    // Conversions to same type
    template <typename TField>
    bool operator()(const TField&, Tag<TField>) const {return true;}

    template <typename TField, typename TResult>
    bool operator()(const TField&, Tag<TResult>) const
    {
        return std::is_convertible<TField, TResult>::value;
    }

    // Vector conversions
    template <typename TElem, Needs<!isSameType<TElem, Variant>()> = 0>
    bool operator()(const ArrayType& from, Tag<std::vector<TElem>>) const
    {
        if (from.empty())
            return true;
        Tag<TElem> toElem;
        for (const auto& fromElem: from)
        {
            if (!applyWithOperand(*this, fromElem, toElem))
                return false;
        }
        return true;
    }

    // Map conversions
    template <typename TValue, Needs<!isSameType<TValue, Variant>()> = 0>
    bool operator()(const ObjectType& from, Tag<std::map<String, TValue>>) const
    {
        if (from.empty())
            return true;
        Tag<TValue> toValue;
        for (const auto& fromKv: from)
        {
            if (!applyWithOperand(*this, fromKv.second, toValue))
                return false;
        }
        return true;
    }
};

//------------------------------------------------------------------------------
template <typename TVariant>
class VariantConvertTo : public Visitor<>
{
public:
    using ArrayType = typename TVariant::Array;
    using ObjectType = typename TVariant::Object;

    // Conversions to same type
    template <typename TField>
    void operator()(const TField& from, TField& to) const {to = from;}

    template <typename TField, typename TResult>
    void operator()(const TField& from, TResult& to) const
    {
        convert(std::is_convertible<TField, TResult>{}, from, to);
    }

    // Vector conversions
    template <typename TElem, Needs<!isSameType<TElem, Variant>()> = 0>
    void operator()(const ArrayType& from, std::vector<TElem>& to) const
    {
        TElem toElem;
        for (size_t i=0; i<from.size(); ++i)
        {
            try
            {
                applyWithOperand(*this, from.at(i), toElem);
            }
            catch (const error::Conversion& e)
            {
                std::ostringstream oss;
                oss << e.what() << " (for Array element #" << i << ')';
                throw error::Conversion(oss.str());
            }
            to.push_back(std::move(toElem));
        }
    }

    // Map conversions
    template <typename TValue, Needs<!isSameType<TValue, Variant>()> = 0>
    void operator()(const ObjectType& from, std::map<String, TValue>& to) const
    {
        TValue toValue;
        for (const auto& fromKv: from)
        {
            try
            {
                applyWithOperand(*this, fromKv.second, toValue);
            }
            catch (const error::Conversion& e)
            {
                std::ostringstream oss;
                oss << e.what() << " (for Object member \""
                    << fromKv.first << "\")";
                throw error::Conversion(oss.str());
            }
            to.emplace(fromKv.first, std::move(toValue));
        }
    }

private:
    template <typename TField, typename TResult>
    static void convert(TrueType, const TField& from, TResult& to)
    {
        to = static_cast<TResult>(from);
    }

    template <typename TField, typename TResult>
    static void convert(FalseType, const TField& from, TResult& to)
    {
        throw error::Conversion(
            "wamp::error::Conversion: Invalid conversion "
            "from " + FieldTraits<TField>::typeName() +
            " to " + ArgTraits<TResult>::typeName());
    }
};

//------------------------------------------------------------------------------
class VariantTypeName : public Visitor<String>
{
public:
    template <typename TField> String operator()(const TField&) const
    {
        return FieldTraits<TField>::typeName();
    }
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_VARIANT_VISITORS_HPP
