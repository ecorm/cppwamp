/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_RANDOM_HPP
#define CPPWAMP_INTERNAL_RANDOM_HPP

#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include "../api.hpp"
#include "../variantdefs.hpp"
#include "../wampdefs.hpp"
#include "../bundled/sevmeyer_prng.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class CPPWAMP_HIDDEN DefaultPRNG64
{
public:
    explicit DefaultPRNG64() : prng_(new Gen) {}

    explicit DefaultPRNG64(uint64_t seed) : prng_(new Gen(seed)) {}

    uint64_t operator()() {return (*prng_)();}

private:
    using Gen = bundled::prng::Generator;

    std::shared_ptr<Gen> prng_; // In heap to avoid losing state in copies.
};

//------------------------------------------------------------------------------
class RandomEphemeralIdGenerator
{
public:
    using Gen64 = std::function<uint64_t ()>;

    RandomEphemeralIdGenerator(Gen64 gen) : gen_(std::move(gen)) {}

    EphemeralId operator()()
    {
        EphemeralId n = gen_();

        // Apply bit mask to constrain the distribution to consecutive integers
        // that can be represented by a double.
        static constexpr auto digits = std::numeric_limits<Real>::digits;
        static constexpr EphemeralId mask = (1ull << digits) - 1u;
        n &= mask;

        // Zero is reserved according to the WAMP spec.
        if (n == 0)
            n = 1; // Negligibly biases the 1 value by 1/2^53

        return n;
    }

private:
    Gen64 gen_;
};

class RandomIdPool;

//------------------------------------------------------------------------------
class ReservedId
{
public:
    ReservedId() = default;

    ReservedId(const ReservedId& rhs) = delete;

    ReservedId(ReservedId&& rhs) noexcept {moveFrom(rhs);}

    ~ReservedId() {reset();}

    ReservedId& operator=(const ReservedId& rhs) = delete;

    ReservedId& operator=(ReservedId&& rhs) noexcept
    {
        moveFrom(rhs);
        return *this;
    }

    void reset();

    EphemeralId get() const {return value_;}

    operator EphemeralId() const {return value_;}

private:
    ReservedId(const std::shared_ptr<RandomIdPool>& pool, EphemeralId id)
        : pool_(pool),
          value_(id)
    {}

    void moveFrom(ReservedId& rhs) noexcept
    {
        reset();
        value_ = rhs.value_;
        rhs.value_ = nullId();
    }

    std::weak_ptr<RandomIdPool> pool_;
    EphemeralId value_ = nullId();

    friend class RandomIdPool;
};

//------------------------------------------------------------------------------
class RandomIdPool : public std::enable_shared_from_this<RandomIdPool>
{
public:
    using Gen64 = std::function<uint64_t ()>;

    using Ptr = std::shared_ptr<RandomIdPool>;

    static Ptr create(Gen64 prng)
    {
        return Ptr(new RandomIdPool(std::move(prng)));
    }

    ~RandomIdPool() = default;

    ReservedId reserve()
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        const auto end = ids_.cend();
        IdSet::const_iterator found;
        EphemeralId id = 0;
        while ((found = ids_.find(id)) != end)
            id = gen_();
        ids_.emplace(id);
        return ReservedId{shared_from_this(), id};
    }

    RandomIdPool(const RandomIdPool&) = delete;
    RandomIdPool(RandomIdPool&&) = delete;
    RandomIdPool& operator=(const RandomIdPool&) = delete;
    RandomIdPool& operator=(RandomIdPool&&) = delete;

private:
    using IdSet = std::set<EphemeralId>;

    explicit RandomIdPool(Gen64&& prng) : gen_(std::move(prng)) {}

    void free(EphemeralId id)
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        ids_.erase(id);
    }

    RandomEphemeralIdGenerator gen_;
    IdSet ids_;
    std::mutex mutex_;

    friend class ReservedId;
};

inline void ReservedId::reset()
{
    if (value_ != nullId())
    {
        auto n = value_;
        value_ = nullId();
        auto pool = pool_.lock();
        if (pool)
            pool->free(n);
    }
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_RANDOM_HPP
