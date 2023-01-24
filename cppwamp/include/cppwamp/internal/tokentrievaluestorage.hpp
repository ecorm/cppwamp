/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TOKENTRIEVALUESTORAGE_HPP
#define CPPWAMP_INTERNAL_TOKENTRIEVALUESTORAGE_HPP

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

//------------------------------------------------------------------------------
template <typename T>
class CPPWAMP_HIDDEN TokenTrieValueLocalStorage
{
public:
    using value_type = T;

    TokenTrieValueLocalStorage() noexcept = default;

    template <typename... Us>
    explicit TokenTrieValueLocalStorage(in_place_t, Us&&... args)
        : hasValue_(true)
    {
        construct(std::forward<Us>(args)...);
    }

    template <typename E, typename... Us>
    explicit TokenTrieValueLocalStorage(in_place_t,
                                        std::initializer_list<E> list,
                                        Us&&... args)
        : hasValue_(true)
    {
        construct(list, std::forward<Us>(args)...);
    }

    TokenTrieValueLocalStorage(const TokenTrieValueLocalStorage& rhs)
        : hasValue_(rhs.has_value())
    {
        if (rhs.has_value())
            construct(rhs.get());
    }

    template <typename U = T>
    TokenTrieValueLocalStorage(TokenTrieValueLocalStorage&& rhs)
        : hasValue_(rhs.has_value())
    {
        if (rhs.has_value())
            construct(std::move(rhs.get()));
        // Moved-from side must still contain value
    }

    ~TokenTrieValueLocalStorage() {reset();}

    TokenTrieValueLocalStorage& operator=(const TokenTrieValueLocalStorage& rhs)
    {
        if (!rhs.has_value())
        {
            reset();
        }
        else if (has_value())
        {
            get() = rhs.get();
        }
        else
        {
            construct(rhs.get());
            hasValue_ = true;
        }
    }

    TokenTrieValueLocalStorage& operator=(TokenTrieValueLocalStorage&& rhs)
    {
        if (!rhs.has_value())
        {
            reset();
        }
        else if (has_value())
        {
            get() = std::move(rhs.get());
        }
        else
        {
            construct(std::move(rhs.get()));
            hasValue_ = true;
        }
        // Moved-from side must still contain value
        return *this;
    }

    bool has_value() const noexcept {return hasValue_;}

    value_type& get() {return storage_.asValue;}

    const value_type& get() const {return storage_.asValue;}

    template <typename... Us>
    void emplace(Us&&... args)
    {
        reset();
        new (&storage_.asValue) value_type(std::forward<Us>(args)...);
        hasValue_ = true;
    }

    template <typename E, typename... Us>
    void emplace(std::initializer_list<E> list,  Us&&... args)
    {
        reset();
        new (&storage_.asValue) value_type(list, std::forward<Us>(args)...);
        hasValue_ = true;
    }

    template <typename U>
    void assign(U&& value)
    {
        if (has_value())
        {
            get() = std::forward<U>(value);
        }
        else
        {
            construct(std::forward<U>(value));
            hasValue_ = true;
        }
    }

    void reset()
    {
        if (has_value())
            get().~value_type();
        hasValue_ = false;
    }

    bool operator==(const TokenTrieValueLocalStorage& rhs) const noexcept
    {
        if (!has_value())
            return !rhs.has_value();
        return rhs.has_value() && (get() == rhs.get());
    }

    bool operator!=(const TokenTrieValueLocalStorage& rhs) const noexcept
    {
        if (!has_value())
            return rhs.has_value();
        return !rhs.has_value() || (get() != rhs.get());
    }

private:
    template <typename... Us>
    void construct(Us&&... args)
    {
        new (&storage_.asValue) value_type(std::forward<Us>(args)...);
    }

    template <typename E, typename... Us>
    void construct(std::initializer_list<E> list, Us&&... args)
    {
        new (&storage_.asValue) value_type(list, std::forward<Us>(args)...);
    }

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
class CPPWAMP_HIDDEN TokenTrieValueHeapStorage
{
public:
    using value_type = T;

    TokenTrieValueHeapStorage() noexcept = default;

    template <typename... Us>
    explicit TokenTrieValueHeapStorage(in_place_t, Us&&... args)
    {
        construct(std::forward<Us>(args)...);
    }

    template <typename E, typename... Us>
    explicit TokenTrieValueHeapStorage(in_place_t,
                                       std::initializer_list<E> list,
                                       Us&&... args)
    {
        construct(list, std::forward<Us>(args)...);
    }

    TokenTrieValueHeapStorage(const TokenTrieValueHeapStorage& rhs)
    {
        if (rhs.has_value())
            construct(rhs.get());
    }

    TokenTrieValueHeapStorage(TokenTrieValueHeapStorage&& rhs)
    {
        if (rhs.has_value())
            construct(std::move(rhs.get()));
        // Moved-from side must still contain value
    }

    TokenTrieValueHeapStorage& operator=(const TokenTrieValueHeapStorage& rhs)
    {
        if (!rhs.has_value())
            reset();
        else if (has_value())
            get() = rhs.get();
        else
            ptr_.reset(new value_type(rhs.get()));
        return *this;
    }

    TokenTrieValueHeapStorage& operator=(TokenTrieValueHeapStorage&& rhs)
    {
        if (!rhs.has_value())
            reset();
        else if (has_value())
            get() = std::move(rhs.get());
        else
            ptr_.reset(new value_type(std::move(rhs.get())));
        // Moved-from side must still contain value
        return *this;
    }

    bool has_value() const noexcept {return ptr_ != nullptr;}

    value_type& get() {return *ptr_;}

    const value_type& get() const {return *ptr_;}

    template <typename... Us>
    void emplace(Us&&... args)
    {
        reset();
        ptr_.reset(new value_type(std::forward<Us>(args)...));
    }

    template <typename E, typename... Us>
    void emplace(std::initializer_list<E> list,  Us&&... args)
    {
        reset();
        ptr_.reset(new value_type(list, std::forward<Us>(args)...));
    }

    template <typename U>
    void assign(U&& value)
    {
        if (has_value())
            get() = std::forward<U>(value);
        else
            ptr_.reset(new value_type(std::forward<U>(value)));
    }

    void reset() {ptr_.reset();}

    bool operator==(const TokenTrieValueHeapStorage& rhs) const noexcept
    {
        if (!has_value())
            return !rhs.has_value();
        return rhs.has_value() && (get() == rhs.get());
    }

    bool operator!=(const TokenTrieValueHeapStorage& rhs) const noexcept
    {
        if (!has_value())
            return rhs.has_value();
        return !rhs.has_value() || (get() != rhs.get());
    }

private:
    template <typename... Us>
    void construct(Us&&... args)
    {
        ptr_.reset(new value_type(std::forward<Us>(args)...));
    }

    template <typename E, typename... Us>
    void construct(std::initializer_list<E> list, Us&&... args)
    {
        ptr_.reset(new value_type(list, std::forward<Us>(args)...));
    }

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
