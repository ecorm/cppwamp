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
private:
    using Self = TokenTrieValueLocalStorage;

public:
    using Value = T;

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

    Value& get() {return storage_.asValue;}

    const Value& get() const {return storage_.asValue;}

    template <typename... Us>
    void emplace(Us&&... args)
    {
        reset();
        new (&storage_.asValue) Value(std::forward<Us>(args)...);
        hasValue_ = true;
    }

    template <typename E, typename... Us>
    void emplace(std::initializer_list<E> list,  Us&&... args)
    {
        reset();
        new (&storage_.asValue) Value(list, std::forward<Us>(args)...);
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
            get().~Value();
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
        new (&storage_.asValue) Value(std::forward<Us>(args)...);
    }

    template <typename E, typename... Us>
    void construct(std::initializer_list<E> list, Us&&... args)
    {
        new (&storage_.asValue) Value(list, std::forward<Us>(args)...);
    }

    union Storage
    {
        Storage() : asNone(false) {}
        ~Storage() {}
        bool asNone;
        Value asValue;
    } storage_;

    bool hasValue_ = false;
};

//------------------------------------------------------------------------------
template <typename T>
class CPPWAMP_HIDDEN TokenTrieValueHeapStorage
{
private:
    using Self = TokenTrieValueHeapStorage;

public:
    using Value = T;

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
            ptr_.reset(new Value(rhs.get()));
        return *this;
    }

    TokenTrieValueHeapStorage& operator=(TokenTrieValueHeapStorage&& rhs)
    {
        if (!rhs.has_value())
            reset();
        else if (has_value())
            get() = std::move(rhs.get());
        else
            ptr_.reset(new Value(std::move(rhs.get())));
        // Moved-from side must still contain value
        return *this;
    }

    bool has_value() const noexcept {return ptr_ != nullptr;}

    Value& get() {return *ptr_;}

    const Value& get() const {return *ptr_;}

    template <typename... Us>
    void emplace(Us&&... args)
    {
        reset();
        ptr_.reset(new Value(std::forward<Us>(args)...));
    }

    template <typename E, typename... Us>
    void emplace(std::initializer_list<E> list,  Us&&... args)
    {
        reset();
        ptr_.reset(new Value(list, std::forward<Us>(args)...));
    }

    template <typename U>
    void assign(U&& value)
    {
        if (has_value())
            get() = std::forward<U>(value);
        else
            ptr_.reset(new Value(std::forward<U>(value)));
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
        ptr_.reset(new Value(std::forward<Us>(args)...));
    }

    template <typename E, typename... Us>
    void construct(std::initializer_list<E> list, Us&&... args)
    {
        ptr_.reset(new Value(list, std::forward<Us>(args)...));
    }

    std::unique_ptr<Value> ptr_;
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
