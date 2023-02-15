/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../variant.hpp"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <type_traits>
#include <utility>

#include "varianttraits.hpp"
#include "../api.hpp"
#include "jsonencoding.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
class Variant::Construct : public Visitor<>
{
public:
    explicit Construct(Variant& dest) : dest_(dest) {}

    template <typename TField> void operator()(const TField& field) const
    {
        dest_.template constructAs<TField>(field);
    }

private:
    Variant& dest_;
};

//------------------------------------------------------------------------------
class Variant::MoveConstruct : public Visitor<>
{
public:
    explicit MoveConstruct(Variant& dest) : dest_(dest) {}

    template <typename TField> void operator()(TField& field) const
    {
        dest_.template constructAs<TField>(std::move(field));
    }

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
    {
        leftField = std::move(rightField);
    }

    template <typename TOld, typename TNew>
    void operator()(TOld&, TNew& rhs) const
    {
        dest_.template destructAs<TOld>();
        dest_.template constructAs<TNew>(std::move(rhs));
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
    Swap(Variant& leftVariant, Variant& rightVariant)
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
        leftField = TLeft{};
        leftVariant_.template destructAs<TLeft>();
        leftVariant_.template constructAs<TRight>(std::move(rightField));
        rightField = TRight{};
        rightVariant_.template destructAs<TRight>();
        rightVariant_.template constructAs<TLeft>(std::move(leftTemp));
        std::swap(leftVariant_.typeId_, rightVariant_.typeId_);
    }

private:
    Variant& leftVariant_;
    Variant& rightVariant_;
};

//------------------------------------------------------------------------------
class Variant::ElementCount : public Visitor<typename Variant::SizeType>
{
public:
    using SizeType = typename Variant::SizeType;

    template <typename T>
    SizeType operator()(const T&) {return 1u;}

    SizeType operator()(const Null&) {return 0u;}

    SizeType operator()(const Array& a) {return a.size();}

    SizeType operator()(const Object& o) {return o.size();}
};

//------------------------------------------------------------------------------
class Variant::LessThan : public Visitor<bool>
{
public:
    template <typename TField>
    bool operator()(const TField& leftField, const TField& rightField) const
    {
        return leftField < rightField;
    }

    template <typename TLeft, typename TRight>
    bool operator()(TLeft lhs, TRight rhs) const
    {
        using BothAreNumbers = MetaBool<isNumber<TLeft>() &&
                                        isNumber<TRight>()>;
        return compare(BothAreNumbers{}, lhs, rhs);
    }

    // TODO: Lexicographic compare with std::vector<T>
    // TODO: Lexicographic compare with std::map<K,T>

private:
    template <typename TLeft, typename TRight>
    static bool compare(FalseType, const TLeft&, const TRight&)
    {
        return FieldTraits<TLeft>::typeId < FieldTraits<TRight>::typeId;
    }

    template <typename TLeft, typename TRight>
    static bool compare(TrueType, const TLeft& lhs, const TRight& rhs)
    {
        // Avoid directly comparing mixed signed/unsigned numbers
        using LhsIsSigned = typename std::is_signed<TLeft>;
        using RhsIsSigned = typename std::is_signed<TRight>;
        return compareNumbers(LhsIsSigned(), RhsIsSigned(), lhs, rhs);
    }

    template <typename TLeft, typename TRight>
    static bool compareNumbers(FalseType, FalseType,
                               const TLeft lhs, const TRight rhs)
    {
        return lhs < rhs;
    }

    template <typename TLeft, typename TRight>
    static bool compareNumbers(FalseType, TrueType,
                               const TLeft lhs, const TRight rhs)
    {
        if (rhs < 0)
            return false;
        using CT = typename std::common_type<TLeft, TRight>::type;
        return static_cast<CT>(lhs) < static_cast<CT>(rhs);
    }

    template <typename TLeft, typename TRight>
    static bool compareNumbers(TrueType, FalseType,
                               const TLeft lhs, const TRight rhs)
    {
        if (lhs < 0)
            return true;
        using CT = typename std::common_type<TLeft, TRight>::type;
        return static_cast<CT>(lhs) < static_cast<CT>(rhs);
    }

    template <typename TLeft, typename TRight>
    static bool compareNumbers(TrueType, TrueType,
                               const TLeft lhs, const TRight rhs)
    {
        return lhs < rhs;
    }
};

//------------------------------------------------------------------------------
CPPWAMP_INLINE Variant::Variant() noexcept : typeId_(TypeId::null) {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Variant::Variant(const Variant& other)
    : typeId_(other.typeId_)
{
    wamp::apply(Construct(*this), other);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Variant::Variant(Variant&& other) noexcept
    : typeId_(other.typeId_)
{
    wamp::apply(MoveConstruct(*this), other);
    other = null;
}

//------------------------------------------------------------------------------
/** @post `this->is<Array>() == true`
    @post `*this == array` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Variant::Variant(Array array)
    : typeId_(TypeId::array)
{
    constructAs<Array>(std::move(array));
}

//------------------------------------------------------------------------------
/** @post `this->is<Object>() == true`
    @post `*this == object` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Variant::Variant(Object object)
    : typeId_(TypeId::object)
{
    constructAs<Object>(std::move(object));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Variant::~Variant() {*this = null;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE TypeId Variant::typeId() const {return typeId_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Variant::operator bool() const {return typeId_ != TypeId::null;}


//------------------------------------------------------------------------------
/** @returns `0` if the variant is null
    @returns `1` if the variant is a boolean, number, or string
    @returns The number of elements if the variant is an array
    @returns The number of members if the variant is an object */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Variant::SizeType Variant::size() const
{
    return wamp::apply(ElementCount(), *this);
}

//------------------------------------------------------------------------------
/** @pre `this->is<Array>() == true`
    @pre `this->size() > index`
    @throws error::Access if `this->is<Array>() == false`
    @throws std::out_of_range if `index >= this->size()`. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Variant& Variant::operator[](SizeType index)
{
    return at(index);
}

//------------------------------------------------------------------------------
/** @pre `this->is<Array>() == true`
    @pre `this->size() > index`
    @throws error::Access if `this->is<Array>() == false`
    @throws std::out_of_range if `index >= this->size()`. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE const Variant& Variant::operator[](SizeType index) const
{
    return at(index);
}

//------------------------------------------------------------------------------
/** @pre `this->is<Array>() == true`
    @pre `this->size() > index`
    @throws error::Access if `this->is<Array>() == false`
    @throws std::out_of_range if `index >= this->size()`. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Variant& Variant::at(SizeType index)
{
    return as<Array>().at(index);
}

//------------------------------------------------------------------------------
/** @pre `this->is<Array>() == true`
    @pre `this->size() > index`
    @throws error::Access if `this->is<Array>() == false`
    @throws std::out_of_range if `index >= this->size()`. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE const Variant& Variant::at(SizeType index) const
{
    return as<Array>().at(index);
}

//------------------------------------------------------------------------------
/** @details
    If there is no element under the given key, a null variant will be
    automatically inserted under that key before the reference is returned.
    @pre `this->is<Object>() == true`
    @throws error::Access if `this->is<Object>() == false` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Variant& Variant::operator[](const String& key)
{
    return as<Object>()[key];
}

//------------------------------------------------------------------------------
/** @pre `this->is<Object>() == true`
    @pre `this->as<Object>().count(key) > 0`
    @throws error::Access if `this->is<Object>() == false`
    @throws std::out_of_range if the key does not exist. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Variant& Variant::at(const String& key)
{
    return as<Object>().at(key);
}

//------------------------------------------------------------------------------
/** @pre `this->is<Object>() == true`
    @pre `this->as<Object>().count(key) > 0`
    @throws error::Access if `this->is<Object>() == false`
    @throws std::out_of_range if the key does not exist. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE const Variant& Variant::at(const String& key) const
{
    return as<Object>().at(key);
}

//------------------------------------------------------------------------------
/** @details
    The comparison is analogous to the Javascript `===` operator, and is
    performed according to the following matrix:

| LHS, RHS-> | Null  | Bool  | Int   | UInt  | Real  | String | Array | Object |
|------------|-------|-------|-------|-------|-------|--------|-------|--------|
| Null       | true  | false | false | false | false | false  | false | false  |
| Bool       | false | L==R  | false | false | false | false  | false | false  |
| Int        | false | false | L==R  | L==R  | L==R  | false  | false | false  |
| UInt       | false | false | L==R  | L==R  | L==R  | false  | false | false  |
| Real       | false | false | L==R  | L==R  | L==R  | false  | false | false  |
| String     | false | false | false | false | false | L==R   | false | false  |
| Array      | false | false | false | false | false | false  | L==R  | false  |
| Object     | false | false | false | false | false | false  | false | L==R   | */
//------------------------------------------------------------------------------
CPPWAMP_INLINE bool Variant::operator==(const Variant& other) const
{
    return wamp::apply(internal::VariantEquivalentTo<Variant>(), *this, other);
}

//------------------------------------------------------------------------------
/** @details
    The result is equivalant to `!(lhs == rhs)`
    @see Variant::operator== */
//------------------------------------------------------------------------------
CPPWAMP_INLINE bool Variant::operator!=(const Variant& other) const
{
    return !(*this == other);
}

//------------------------------------------------------------------------------
/** @details
    This operator is provided to allow the use of variants in associative
    containers. The comparison is performed according to the following
    matrix:

| LHS, RHS-> | Null  | Bool  | Int   | UInt  | Real  | String | Array | Object |
|------------|-------|-------|-------|-------|-------|--------|-------|--------|
| Null       | false | false | false | false | false | false  | false | false  |
| Bool       | false | L<R   | false | false | false | false  | false | false  |
| Int        | false | false | L<R   | L<R   | L<R   | false  | false | false  |
| UInt       | false | false | L<R   | L<R   | L<R   | false  | false | false  |
| Real       | false | false | L<R   | L<R   | L<R   | false  | false | false  |
| String     | false | false | false | false | false | L<R    | false | false  |
| Array      | false | false | false | false | false | false  | L<R   | false  |
| Object     | false | false | false | false | false | false  | false | L<R    | */
//------------------------------------------------------------------------------
CPPWAMP_INLINE bool Variant::operator<(const Variant &other) const
{
    return wamp::apply(LessThan(), *this, other);
}

//------------------------------------------------------------------------------
/** @post `this->typeId() == other.typeId()`
    @post `*this == other` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Variant& Variant::operator=(const Variant& other)
{
    if (&other != this)
        Variant(other).swap(*this);
    return *this;
}

//------------------------------------------------------------------------------
/** @post `other == null` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Variant& Variant::operator=(Variant&& other) noexcept
{
    if (&other != this)
    {
        wamp::apply(MoveAssign(*this), *this, other);
        other = null;
    }
    return *this;
}

//------------------------------------------------------------------------------
/** @post `this->typeId() == TypeId::array`
    @post `*this == array` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Variant& Variant::operator=(Array array)
{
    return this->operator=<Array>(std::move(array));
}

//------------------------------------------------------------------------------
/** @post `this->typeId() == TypeId::object`
    @post `*this == object` */
//------------------------------------------------------------------------------
CPPWAMP_INLINE Variant& Variant::operator=(Object object)
{
    return this->operator=<Object>(std::move(object));
}

//------------------------------------------------------------------------------
/** @see void swap(Variant& v, Variant& w) noexcept */
//------------------------------------------------------------------------------
CPPWAMP_INLINE void Variant::swap(Variant &other) noexcept
{
    if (&other != this)
        wamp::apply(Swap(*this, other), *this, other);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Variant::destruct()
{
    wamp::apply(Destruct(&field_), *this);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Variant::Field::Field() : nullValue(null) {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Variant::Field::~Field() {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void swap(Variant& v, Variant& w) noexcept {v.swap(w);}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool isNumber(const Variant& v)
{
    using T = TypeId;
    switch(v.typeId())
    {
    case T::integer: case T::uint: case T::real:
        return true;
    default:
        return false;
    }
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool isScalar(const Variant& v)
{
    using T = TypeId;
    switch(v.typeId())
    {
    case T::boolean: case T::integer: case T::uint: case T::real:
        return true;
    default:
        return false;
    }
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Variant::String typeNameOf(const Variant& v)
{
    return wamp::apply(internal::VariantTypeName(), v);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::ostream& operator<<(std::ostream& out, const Array& a)
{
    using Sink = jsoncons::stream_sink<char>;
    internal::JsonEncoderImpl<Sink> encoder;
    out.put('[');
    auto begin = a.begin();
    auto end = a.end();
    for (auto iter = begin; iter != end; ++iter)
    {
        if (iter != begin)
            out.put(',');
        encoder.encode(*iter, out);
    }
    out.put(']');
    return out;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::ostream& operator<<(std::ostream& out, const Object& o)
{
    using Sink = jsoncons::stream_sink<char>;
    internal::JsonEncoderImpl<Sink> encoder;
    out.put('{');
    auto begin = o.begin();
    auto end = o.end();
    for (auto kv = begin; kv != end; ++kv)
    {
        if (kv != begin)
            out.put(',');
        encoder.encode(Variant(kv->first), out);
        out.put(':');
        encoder.encode(kv->second, out);
    }
    out.put('}');
    return out;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::ostream& operator<<(std::ostream& out, const Variant& v)
{
    using Sink = jsoncons::stream_sink<char>;
    internal::JsonEncoderImpl<Sink> encoder;
    encoder.encode(v, out);
    return out;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::string toString(const Array& a)
{
    using Sink = jsoncons::string_sink<std::string>;
    internal::JsonEncoderImpl<Sink> encoder;
    std::string str;
    str += '[';
    auto begin = a.begin();
    auto end = a.end();
    for (auto iter = begin; iter != end; ++iter)
    {
        if (iter != begin)
            str += ',';
        encoder.encode(*iter, str);
    }
    str += ']';
    return str;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::string toString(const Object& o)
{
    using Sink = jsoncons::string_sink<std::string>;
    internal::JsonEncoderImpl<Sink> encoder;
    std::string str;
    str += '{';
    auto begin = o.begin();
    auto end = o.end();
    for (auto kv = begin; kv != end; ++kv)
    {
        if (kv != begin)
            str += ',';
        encoder.encode(Variant(kv->first), str);
        str += ':';
        encoder.encode(kv->second, str);
    }
    str += '}';
    return str;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::string toString(const Variant& v)
{
    using Sink = jsoncons::string_sink<std::string>;
    internal::JsonEncoderImpl<Sink> encoder;
    std::string str;
    encoder.encode(v, str);
    return str;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool operator==(const Variant& variant,
                               const Variant::CharType* str)
{
    return variant.is<String>() && (variant.as<String>() == str);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool operator==(const Variant::CharType* str,
                               const Variant& variant)
{
    return variant == str;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool operator==(const Variant& variant, Variant::CharType* str)
{
    return variant == static_cast<const Variant::CharType*>(str);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool operator==(Variant::CharType* str, const Variant& variant)
{
    return variant == static_cast<const Variant::CharType*>(str);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool operator!=(const Variant& variant,
                               const Variant::CharType* str)
{
    return !variant.is<String>() || (variant.as<String>() != str);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool operator!=(const Variant::CharType* str,
                               const Variant& variant)
{
    return variant != str;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool operator!=(const Variant& variant, Variant::CharType* str)
{
    return variant != static_cast<const Variant::CharType*>(str);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool operator!=(Variant::CharType* str, const Variant& variant)
{
    return variant != static_cast<const Variant::CharType*>(str);
}

} // namespace wamp
