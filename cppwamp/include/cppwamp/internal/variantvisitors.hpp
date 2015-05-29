/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_VARIANT_VISITORS_HPP
#define CPPWAMP_INTERNAL_VARIANT_VISITORS_HPP

#include <algorithm>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include "../traits.hpp"
#include "../visitor.hpp"
#include "varianttraits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// The !isSameType is required because std::vector<bool>::const_reference may
// or may not just be bool.
template <typename T>
using EnableIfBoolRef = EnableIf<isBool<T>() && !isSameType<T, bool>()>;

template <typename T, typename U>
constexpr bool bothAreNumbers() {return isNumber<T>() && isNumber<U>();}

template <typename T, typename U>
using EnableIfBothAreNumbers = EnableIf<bothAreNumbers<T, U>()>;

template <typename T, typename U>
using DisableIfBothAreNumbers = DisableIf<bothAreNumbers<T, U>()>;

template <typename T>
using DisableIfVariant = DisableIf<isSameType<T, Variant>()>;

template <typename TFrom, typename TTo>
using EnableIfConvertible = EnableIf<std::is_convertible<TFrom,TTo>::value>;

template <typename TFrom, typename TTo>
using DisableIfConvertible = DisableIf<std::is_convertible<TFrom,TTo>::value>;


//------------------------------------------------------------------------------
class TypeName : public Visitor<String>
{
public:
    template <typename TField> String operator()(const TField&) const
        {return FieldTraits<TField>::typeName();}
};

//------------------------------------------------------------------------------
class EquivalentTo : public Visitor<bool>
{
public:
    template <typename TField>
    bool operator()(const TField& lhs, const TField& rhs) const
        {return lhs == rhs;}

    template <typename TArg, EnableIfBoolRef<TArg> = 0>
    bool operator()(const bool lhs, const TArg rhs) const
        {return lhs == bool(rhs);}

    template <typename TField, typename TArg,
              DisableIfBothAreNumbers<TField,TArg> = 0>
    bool operator()(const TField&, const TArg&) const {return false;}

    template <typename TField, typename TArg,
              EnableIfBothAreNumbers<TField,TArg> = 0>
    bool operator()(const TField lhs, const TArg rhs) const
        {return lhs == rhs;}

    template <typename TElem, DisableIfVariant<TElem> = 0>
    bool operator()(const Array& lhs, const std::vector<TElem>& rhs) const
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

    template <typename TValue, DisableIfVariant<TValue> = 0>
    bool operator()(const Object& lhs,
                    const std::map<String, TValue>& rhs) const
    {
        using Map        = std::map<String, TValue>;
        using MapPair    = typename Map::value_type;
        using ObjectPair = typename Object::value_type;
        return (lhs.size() != rhs.size()) ? false :
            std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(),
                       [](const ObjectPair& lPair, const MapPair& rPair)
                       {
                            return lPair.first == rPair.first &&
                                   lPair.second == rPair.second;
                       });
    }
};

//------------------------------------------------------------------------------
class NotEquivalentTo : public Visitor<bool>
{
public:
    template <typename TField>
    bool operator()(const TField& lhs, const TField& rhs) const
        {return lhs != rhs;}

    template <typename TArg, EnableIfBoolRef<TArg> = 0>
    bool operator()(const bool lhs, const TArg rhs) const
        {return lhs != rhs;}

    template <typename TField, typename TArg,
              DisableIfBothAreNumbers<TField,TArg> = 0>
    bool operator()(const TField&, const TArg&) const {return true;}

    template <typename TField, typename TArg,
              EnableIfBothAreNumbers<TField,TArg> = 0>
    bool operator()(TField lhs, TArg rhs) const {return lhs != rhs;}

    template <typename TElem, DisableIfVariant<TElem> = 0>
    bool operator()(const Array& lhs, const std::vector<TElem>& rhs) const
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

    template <typename TValue, DisableIfVariant<TValue> = 0>
    bool operator()(const Object& lhs,
                    const std::map<String, TValue>& rhs) const
    {
        using Map        = std::map<String, TValue>;
        using MapPair    = typename Map::value_type;
        using ObjectPair = typename Object::value_type;

        auto comp = [](const ObjectPair& lPair, const MapPair& rPair)
        {
             return lPair.first == rPair.first &&
                    lPair.second == rPair.second;
        };

        return (lhs.size() != rhs.size()) ? true :
            ( std::mismatch(lhs.cbegin(), lhs.cend(), rhs.cbegin(),
                            std::move(comp)).first != lhs.end() );
    }
};

//------------------------------------------------------------------------------
class Output : public Visitor<>
{
public:
    template <typename TField>
    void operator()(const TField& f, std::ostream& out) const {out << f;}

    void operator()(const Bool& b, std::ostream& out) const
    {
        out << (b ? "true" : "false");
    }

    void operator()(const Array& a, std::ostream& out) const
    {
        out << '[';
        for (const auto& v: a)
        {
            if (&v != &a.front())
                out << ",";
            if (v.template is<TypeId::string>())
                out << '"' << v.template as<TypeId::string>() << '"';
            else
                applyWithOperand(*this, v, out);
        }
        out << ']';
    }

    void operator()(const Object& o, std::ostream& out) const
    {
        out << '{';
        for (auto kv = o.cbegin(); kv != o.cend(); ++kv)
        {
            if (kv != o.cbegin())
                out << ',';
            out << '"' << kv->first << "\":";
            const auto& v = kv->second;
            if (v.template is<TypeId::string>())
                out << '"' << v.template as<TypeId::string>() << '"';
            else
                applyWithOperand(*this, v, out);
        }
        out << '}';
    }
};

} // namespace internal

//------------------------------------------------------------------------------
class Variant::Construct : public Visitor<>
{
public:
    explicit Construct(Variant& dest) : dest_(dest) {}

    template <typename TField> void operator()(const TField& field) const
        {dest_.constructAs<TField>(field);}

private:
    Variant& dest_;
};

//------------------------------------------------------------------------------
class Variant::MoveConstruct : public Visitor<>
{
public:
    explicit MoveConstruct(Variant& dest) : dest_(dest) {}

    template <typename TField> void operator()(TField& field) const
        {dest_.constructAs<TField>(std::move(field));}

private:
    Variant& dest_;
};

//------------------------------------------------------------------------------
class Variant::MoveAssign : public Visitor<>
{
public:
    explicit MoveAssign(Variant& dest) : dest_(dest) {}

    template <typename TField>
    void operator()(TField& leftField, TField& rightField) const
        {leftField = std::move(rightField);}

    template <typename TOld, typename TNew>
    void operator()(TOld&, TNew& rhs) const
    {
        dest_.destructAs<TOld>();
        dest_.constructAs<TNew>(std::move(rhs));
        dest_.typeId_ = FieldTraits<TNew>::typeId;
    }

private:
    Variant& dest_;
};

//------------------------------------------------------------------------------
class Variant::Destruct : public Visitor<>
{
public:
    Destruct(void* field) : field_(field) {}

    template <typename TField>
    void operator()(TField&) const {Access<TField>::destruct(field_);}

private:
    void* field_;
};

//------------------------------------------------------------------------------
class Variant::Swap : public Visitor<>
{
public:
    explicit Swap(Variant& leftVariant, Variant& rightVariant)
        : leftVariant_(leftVariant), rightVariant_(rightVariant) {}

    template <typename TField>
    void operator()(TField& leftField, TField& rightField) const
    {
        using std::swap;
        swap(leftField, rightField);
    }

    template <typename TLeft, typename TRight>
    void operator()(TLeft& leftField, TRight& rightField) const
    {
        TLeft leftTemp = std::move(leftField);
        leftVariant_.destructAs<TLeft>();
        leftVariant_.constructAs<TRight>(std::move(rightField));
        rightVariant_.destructAs<TRight>();
        rightVariant_.constructAs<TLeft>(std::move(leftTemp));
        std::swap(leftVariant_.typeId_, rightVariant_.typeId_);
    }

private:
    Variant& leftVariant_;
    Variant& rightVariant_;
};

//------------------------------------------------------------------------------
class Variant::LessThan : public Visitor<bool>
{
public:
    template <typename TField>
    bool operator()(const TField& leftField, const TField& rightField) const
        {return leftField < rightField;}

    template <typename TLeft, typename TRight,
              internal::DisableIfBothAreNumbers<TLeft,TRight> = 0>
    bool operator()(const TLeft&, const TRight&) const
        {return FieldTraits<TLeft>::typeId < FieldTraits<TRight>::typeId;}

    template <typename TLeft, typename TRight,
              internal::EnableIfBothAreNumbers<TLeft,TRight> = 0>
    bool operator()(TLeft lhs, TRight rhs) const {return lhs < rhs;}
};

//------------------------------------------------------------------------------
class Variant::ConvertibleTo : public Visitor<bool>
{
public:
    // Conversions to same type
    template <typename TField>
    bool operator()(const TField&, Tag<TField>) const {return true;}

    // Implicit conversions
    template <typename TField, typename TResult,
              internal::EnableIfConvertible<TField,TResult> = 0>
    bool operator()(const TField&, Tag<TResult>) const {return true;}

    // Invalid conversions
    template <typename TField, typename TResult,
              internal::DisableIfConvertible<TField,TResult> = 0>
    bool operator()(const TField&, Tag<TResult>) const {return false;}

    // Vector conversions
    template <typename TElem, internal::DisableIfVariant<TElem> = 0>
    bool operator()(const Array& from, Tag<std::vector<TElem>>) const
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
    template <typename TValue, internal::DisableIfVariant<TValue> = 0>
    bool operator()(const Object& from, Tag<std::map<String, TValue>>) const
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
class Variant::ConvertTo : public Visitor<>
{
public:
    // Conversions to same type
    template <typename TField>
    void operator()(const TField& from, TField& to) const {to = from;}

    // Implicit conversions
    template <typename TField, typename TResult,
              internal::EnableIfConvertible<TField,TResult> = 0>
    void operator()(const TField& from, TResult& to) const
        {to = static_cast<TResult>(from);}

    // Invalid conversions
    template <typename TField, typename TResult,
              internal::DisableIfConvertible<TField,TResult> = 0>
    void operator()(const TField&, TResult&) const
    {
        throw error::Conversion(
                "wamp::error::Conversion: Invalid conversion "
                "from " + FieldTraits<TField>::typeName() +
                " to " + ArgTraits<TResult>::typeName());
    }

    // Vector conversions
    template <typename TElem, internal::DisableIfVariant<TElem> = 0>
    void operator()(const Array& from, std::vector<TElem>& to) const
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
    template <typename TValue, internal::DisableIfVariant<TValue> = 0>
    void operator()(const Object& from, std::map<String, TValue>& to) const
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
};

//------------------------------------------------------------------------------
class Variant::ElementCount : public Visitor<Variant::SizeType>
{
public:
    using SizeType = Variant::SizeType;

    template <typename T>
    SizeType operator()(const T&) {return 1u;}

    SizeType operator()(const Null&) {return 0u;}

    SizeType operator()(const Array& a) {return a.size();}

    SizeType operator()(const Object& o) {return o.size();}
};

} // namespace wamp

#endif // CPPWAMP_INTERNAL_VARIANT_VISITORS_HPP
