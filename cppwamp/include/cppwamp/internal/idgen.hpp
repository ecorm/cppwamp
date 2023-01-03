/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_IDGEN_HPP
#define CPPWAMP_INTERNAL_IDGEN_HPP

#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include "../wampdefs.hpp"
#include "../variantdefs.hpp"
#include "../bundled/amosnier_sha256.hpp"
#include "../bundled/sevmeyer_prng.hpp"
#include "base64.hpp"

namespace wamp
{


namespace internal
{

//------------------------------------------------------------------------------
class RandomIdGenerator
{
public:
    RandomIdGenerator() {}

    RandomIdGenerator(EphemeralId seed) : prng_(seed) {}

    EphemeralId operator()()
    {
        EphemeralId n = prng_();

        // Apply bit mask to constrain the distribution to consecutive integers
        // that can be represented by a double.
        static constexpr auto digits = std::numeric_limits<Real>::digits;
        static constexpr EphemeralId mask = (1ull << digits) - 1u;
        n &= mask;

        // Zero is reserved according to the WAMP spec.
        if (n == 0)
            n = 1; // Neglibibly biases the 1 value by 1/2^53

        return n;
    }

private:
    wamp::bundled::prng::Generator prng_;
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
    ReservedId(std::shared_ptr<RandomIdPool> pool, EphemeralId id)
        : pool_(std::move(pool)),
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
class RandomIdPool : std::enable_shared_from_this<RandomIdPool>
{
public:
    using Ptr = std::shared_ptr<RandomIdPool>;

    static Ptr create() {return Ptr(new RandomIdPool);}

    static Ptr create(EphemeralId seed) {return Ptr(new RandomIdPool(seed));}

    RandomIdPool(const RandomIdPool&) = delete;

    RandomIdPool& operator=(const RandomIdPool&) = delete;

    ReservedId reserve()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto end = ids_.cend();
        IdSet::const_iterator found;
        EphemeralId id;

        do
        {
            id = gen_();
            found = ids_.find(id);
        }
        while (found != end);

        ids_.emplace(id);
        return ReservedId{shared_from_this(), id};
    }

private:
    using IdSet = std::set<EphemeralId>;

    RandomIdPool() {}

    explicit RandomIdPool(EphemeralId seed) : gen_(seed) {}

    void free(EphemeralId id)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ids_.erase(id);
    }

    RandomIdGenerator gen_;
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

//------------------------------------------------------------------------------
class IdAnonymizer
{
public:
    static std::string anonymize(EphemeralId id)
    {
        /*  Compute SHA256 hash of id, then stringify the result with Base64Url.
            Truncate the hash to 128 bits to keep the anonymized ID reasonably
            short in the logs. Truncating only affects the (exceedingly small)
            probability that two ephemeral ids have the same anonymized ID in
            the logs. See https://security.stackexchange.com/a/34797/169835. */
        uint8_t hash[32];
        wamp::bundled::sha256::calc_sha_256(hash, &id, sizeof(id));
        std::string s;
        Base64Url::encode(hash, 16, s);
        return s;
    }
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_IDGEN_HPP
