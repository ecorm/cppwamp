/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TOKENTRIEVALUESTORAGE_HPP
#define CPPWAMP_INTERNAL_TOKENTRIEVALUESTORAGE_HPP

#include <cassert>
#include <initializer_list>
#include <memory>
#include <type_traits>
#include <utility>

#include "../api.hpp"
#include "../tagtypes.hpp"

namespace wamp
{

namespace internal
{

struct TokenTrieNullAllocator {};

//------------------------------------------------------------------------------
template <typename T, typename A>
class CPPWAMP_HIDDEN TokenTrieValueStorage
{
public:
    using value_type = T;
    using allocator_type = A;

    TokenTrieValueStorage() noexcept = default;

    explicit TokenTrieValueStorage(const allocator_type& a) noexcept
        : alloc_(a)
    {}

    ~TokenTrieValueStorage() {reset();}

    bool has_value() const noexcept {return ptr_ != nullptr;}

    value_type& get() {return *ptr_;}

    const value_type& get() const {return *ptr_;}

    void reset()
    {
        if (ptr_ != nullptr)
        {
            AllocTraits::destroy(alloc_, ptr_);
            AllocTraits::deallocate(alloc_, ptr_);
            ptr_ = nullptr;
        }
    }

    template <typename... Us>
    void construct(Us&&... args)
    {
        assert(ptr_ == nullptr);
        ptr_ = AllocTraits::allocate(alloc_, sizeof(value_type));
        AllocTraits::construct(alloc_, ptr_, std::forward<Us>(args)...);
    }

    template <typename E, typename... Us>
    void construct(std::initializer_list<E> list, Us&&... args)
    {
        assert(ptr_ == nullptr);
        ptr_ = AllocTraits::allocate(alloc_, sizeof(value_type));
        AllocTraits::construct(alloc_, ptr_, list, std::forward<Us>(args)...);
    }

private:
    using AllocTraits = std::allocator_traits<allocator_type>;

    allocator_type alloc_;
    value_type* ptr_;
};

//------------------------------------------------------------------------------
template <typename T>
class CPPWAMP_HIDDEN TokenTrieValueStorage<T, TokenTrieNullAllocator>
{
public:
    using value_type = T;

    TokenTrieValueStorage() noexcept = default;

    template <typename TAllocator>
    explicit TokenTrieValueStorage(const TAllocator&) noexcept {}

    ~TokenTrieValueStorage() {reset();}

    bool has_value() const noexcept {return hasValue_;}

    value_type& get() {return storage_.asValue;}

    const value_type& get() const {return storage_.asValue;}

    template <typename... Us>
    void construct(Us&&... args)
    {
        assert(!hasValue_);
        new (&storage_.asValue) value_type(std::forward<Us>(args)...);
        hasValue_ = true;
    }

    template <typename E, typename... Us>
    void construct(std::initializer_list<E> list, Us&&... args)
    {
        assert(!hasValue_);
        new (&storage_.asValue) value_type(list, std::forward<Us>(args)...);
        hasValue_ = true;
    }

    void reset()
    {
        if (has_value())
            get().~value_type();
        hasValue_ = false;
    }

private:
    union Storage
    {
        Storage() : asNone(false) {}
        ~Storage() {}
        bool asNone;
        value_type asValue;
    } storage_;

    bool hasValue_ = false;
};

//------------------------------------------------------------------------------
template <typename T>
class CPPWAMP_HIDDEN TokenTrieValueStorage<T, std::allocator<T>>
{
public:
    using value_type = T;

    TokenTrieValueStorage() noexcept = default;

    explicit TokenTrieValueStorage(const std::allocator<T>&) noexcept {}

    bool has_value() const noexcept {return ptr_ != nullptr;}

    value_type& get() {return *ptr_;}

    const value_type& get() const {return *ptr_;}

    void reset() {ptr_.reset();}

    template <typename... Us>
    void construct(Us&&... args)
    {
        assert(ptr_ == nullptr);
        ptr_.reset(new value_type(std::forward<Us>(args)...));
    }

    template <typename E, typename... Us>
    void construct(std::initializer_list<E> list, Us&&... args)
    {
        assert(ptr_ == nullptr);
        ptr_.reset(new value_type(list, std::forward<Us>(args)...));
    }

private:
    std::unique_ptr<value_type> ptr_;
};

//------------------------------------------------------------------------------
template <typename T, typename Self>
struct CPPWAMP_HIDDEN TokenTrieValueTraits
{
    template <typename U>
    static constexpr bool isConvertible()
    {
        using V = typename std::remove_cv<
            typename std::remove_reference<U>::type>::type;
        return !std::is_same<V, Self>::value &&
               !std::is_same<V, in_place_t>::value &&
               std::is_convertible<U, T>::value;
    }

    template <typename U>
    static constexpr bool isConstructible()
    {
        using V = typename std::remove_cv<
            typename std::remove_reference<U>::type>::type;
        return !std::is_same<V, Self>::value &&
               !std::is_same<V, in_place_t>::value &&
               !std::is_convertible<U, T>::value &&
               std::is_constructible<T, V>::value;
    }

    template <typename U>
    static constexpr bool isAssignable()
    {
        using V = typename std::remove_cv<
            typename std::remove_reference<U>::type>::type;
        return !std::is_same<V, Self>::value &&
               !std::is_same<V, in_place_t>::value &&
               std::is_constructible<T, V>::value &&
               std::is_assignable<T, V>::value;
    }
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TOKENTRIEVALUESTORAGE_HPP
