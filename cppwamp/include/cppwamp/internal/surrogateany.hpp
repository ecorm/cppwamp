/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_SURROGATEANY_HPP
#define CPPWAMP_INTERNAL_SURROGATEANY_HPP

#include <array>
#include <cstddef>
#include <initializer_list>
#include <typeinfo>
#include <utility>
#include "../tagtypes.hpp"
#include "../traits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class BadAnyCast : public std::bad_cast
{
    const char* what() const noexcept override
    {
        return "wamp::BadAnyCast";
    }
};

class SurrogateAny;

//------------------------------------------------------------------------------
struct AnyReqs
{
    template <typename T>
    struct IsInPlaceType : FalseType {};

    template <typename T>
    struct IsInPlaceType<in_place_type_t<T>> : TrueType {};

    template <typename T>
    static constexpr bool isInPlaceType() {return IsInPlaceType<T>::value;}

    template <typename T>
    static constexpr bool constructible()
    {
        return isConstructible<T>(std::is_same<Decay<T>, SurrogateAny>{});
    }

    template <typename T, typename... As>
    static constexpr bool emplaceable()
    {
        return std::is_constructible<Decay<T>, As...>::value &&
               std::is_copy_constructible<Decay<T>>::value;
    }

    template <typename T, typename U, typename... As>
    static constexpr bool listEmplaceable()
    {
        using L = std::initializer_list<U>;
        return std::is_constructible<Decay<T>, L, As...>::value &&
               std::is_copy_constructible<Decay<T>>::value;
    }

    template <typename T>
    static constexpr bool assignable()
    {
        return isAssignable<T>(std::is_same<Decay<T>, SurrogateAny>{});
    }

private:
    template <typename T>
    static constexpr bool isConstructible(TrueType)
    {
        return false;
    }

    template <typename T>
    static constexpr bool isConstructible(FalseType)
    {
        return !isInPlaceType<Decay<T>>() &&
               std::is_copy_constructible<Decay<T>>::value;
    }

    template <typename T>
    static constexpr bool isAssignable(TrueType)
    {
        return false;
    }

    template <typename T>
    static constexpr bool isAssignable(FalseType)
    {
        return std::is_copy_constructible<Decay<T>>::value;
    }
};

//------------------------------------------------------------------------------
class SurrogateAny
{
private:
    using Reqs = internal::AnyReqs;

public:
    SurrogateAny() noexcept {}

    SurrogateAny(const SurrogateAny& other)
        : box_(other.copy(buffer_))
    {}

    SurrogateAny(SurrogateAny&& other) noexcept
        : box_(other.move(buffer_))
    {}

    template <typename T, Needs<Reqs::constructible<T>()> = 0>
    SurrogateAny(T&& value)
        : box_(construct<Decay<T>>(std::forward<T>(value)))
    {}

    template <typename T, typename... As,
              Needs<Reqs::emplaceable<T, As...>()> = 0>
    explicit SurrogateAny(in_place_type_t<T>, As&&... args)
        : box_(construct<Decay<T>>(std::forward<As>(args)...))
    {}

    template<typename T, typename U, typename... As,
             Needs<Reqs::listEmplaceable<T, U, As...>()> = 0>
    explicit SurrogateAny(in_place_type_t<T>, std::initializer_list<U> list,
                 As&&... args )
        : box_(construct<Decay<T>>(list, std::forward<As>(args)...))
    {}

    SurrogateAny& operator=(const SurrogateAny& rhs)
    {
        if (!rhs.has_value())
            reset();
        else
            *this = SurrogateAny(rhs);
        return *this;
    }

    SurrogateAny& operator=(SurrogateAny&& rhs) noexcept
    {
        reset();
        box_ = rhs.move(buffer_);
        return *this;
    }

    template <typename T, Needs<Reqs::assignable<T>()> = 0>
    SurrogateAny& operator=(T&& rhs)
    {
        SurrogateAny temp(std::forward<T>(rhs));
        return *this = std::move(temp);
    }

    ~SurrogateAny()
    {
        reset();
    }

    template <typename T, typename... As,
              Needs<Reqs::emplaceable<T, As...>()> = 0>
    typename std::decay<T>::type& emplace(As&&... args)
    {
        reset();
        auto b = construct<Decay<T>>(std::forward<As>(args)...);
        box_ = b;
        return b->value;
    }

    template <typename T, class U, class... As,
              Needs<Reqs::listEmplaceable<T, U, As...>()> = 0>
    Decay<T>& emplace(std::initializer_list<U> list, As&&... args)
    {
        reset();
        auto b = construct<Decay<T>>(list, std::forward<As>(args)...);
        box_ = b;
        return b->value;
    }

    void reset() noexcept
    {
        if (!has_value())
            return;

        if (isLocal())
            box_->~Boxing();
        else
            delete box_;
        box_ = nullptr;
    }

    void swap(SurrogateAny& other) noexcept
    {
        SurrogateAny temp(std::move(other));
        other = std::move(*this);
        *this = std::move(temp);
    }

    bool has_value() const noexcept {return box_ != nullptr;}

    const std::type_info& type() const noexcept
    {
        if (has_value())
            return box_->type();
        else
            return typeid(void);
    }

private:
    struct Boxing
    {
        virtual ~Boxing() {}
        virtual const std::type_info& type() const noexcept = 0;
        virtual Boxing* copyLocal(char* buffer) const = 0;
        virtual Boxing* copyHeaped() const = 0;
        virtual Boxing* moveLocal(char* buffer) noexcept = 0;
    };

    template <typename T>
    struct Boxed : Boxing
    {
        T value;

        template <typename... As>
        Boxed(As&&... args) : value(std::forward<As>(args)...) {}

        const std::type_info& type() const noexcept override
        {
            return typeid(T);
        }

        Boxing* copyLocal(char* buffer) const override
        {
            return new (buffer) Boxed<T>(value);
        }

        Boxing* copyHeaped() const override
        {
            return new Boxed<T>(value);
        }

        Boxing* moveLocal(char* buffer) noexcept override
        {
            return new (buffer) Boxed<T>(std::move(value));
        }
    };

    // Have enough capacity to locally store two pointers.
    using StorageLimitType = Boxed<std::array<Boxing*, 2>>;

    static constexpr auto capacity_ = sizeof(StorageLimitType);

    static constexpr auto alignment_ = alignof(StorageLimitType);

    using Buffer = std::array<char, capacity_>;

    template <typename T>
    constexpr bool fits()
    {
        return (sizeof(T) <= capacity_) == (alignof(T) <= alignment_);
    }

    template <typename T, typename... As>
    Boxed<T>* construct(As&&... args)
    {
        if (fits<T>())
            return new (buffer_.data()) Boxed<T>(std::forward<As>(args)...);
        else
            return new Boxed<T>(std::forward<As>(args)...);
    }

    template<typename T, class U, class... As>
    Boxed<T>* construct(std::initializer_list<U> il, As&&... args)
    {
        if (fits<T>())
            return new (buffer_.data()) Boxed<T>(il, std::forward<As>(args)...);
        else
            return new Boxed<T>(il, std::forward<As>(args)...);
    }

    Boxing* copy(Buffer& localBuffer) const
    {
        if (!has_value())
            return nullptr;

        return isLocal() ? box_->copyLocal(localBuffer.data())
                         : box_->copyHeaped();
    }

    Boxing* move(Buffer& localBuffer)
    {
        if (!has_value())
            return nullptr;

        if (isLocal())
        {
            auto ptr = box_->moveLocal(localBuffer.data());
            box_->~Boxing();
            box_ = nullptr;
            return ptr;
        }
        else
        {
            auto ptr = box_;
            box_ = nullptr;
            return ptr;
        }
    }

    bool isLocal() const
    {
        // https://stackoverflow.com/a/9657868
        return reinterpret_cast<const char*>(box_) == buffer_.data();
    }

    alignas(alignment_) Buffer buffer_ = {0};
    Boxing* box_ = nullptr;

    template <typename T>
    friend const T* anyCast(const SurrogateAny* a) noexcept;

    template<typename T>
    friend T* anyCast(SurrogateAny* a) noexcept;

    friend struct SurrogateAnyTestAccess;
};

inline void swap(SurrogateAny& lhs, SurrogateAny& rhs) noexcept {lhs.swap(rhs);}

template <typename T>
const T* anyCast(const SurrogateAny* a) noexcept
{
    if (a == nullptr || !a->has_value() || a->type() != typeid(T))
        return nullptr;
    auto b = static_cast<const SurrogateAny::Boxed<T>*>(a->box_);
    return &(b->value);
}

template<typename T>
T* anyCast(SurrogateAny* a) noexcept
{
    if (a == nullptr || !a->has_value() || a->type() != typeid(T))
        return nullptr;
    auto b = static_cast<SurrogateAny::Boxed<T>*>(a->box_);
    return &(b->value);
}

template <typename T>
T anyCast(const SurrogateAny& a)
{
    using V = ValueTypeOf<T>;
    static_assert(std::is_constructible<T, const V&>::value, "");

    auto ptr = anyCast<V>(&a);
    if (ptr == nullptr)
        throw BadAnyCast();
    return static_cast<T>(*ptr);
}

template <typename T>
T anyCast(SurrogateAny& a)
{
    using V = ValueTypeOf<T>;
    static_assert(std::is_constructible<T, V&>::value, "");

    auto ptr = anyCast<V>(&a);
    if (ptr == nullptr)
        throw BadAnyCast();
    return static_cast<T>(*ptr);
}

template <typename T>
T anyCast(SurrogateAny&& a)
{
    using V = ValueTypeOf<T>;
    static_assert(std::is_constructible<T, V>::value, "");

    auto ptr = anyCast<V>(&a);
    if (ptr == nullptr)
        throw BadAnyCast();
    return static_cast<T>(std::move(*ptr));
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_SURROGATEANY_HPP
