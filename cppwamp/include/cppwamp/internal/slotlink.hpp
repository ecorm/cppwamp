/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_SLOTLINK_HPP
#define CPPWAMP_INTERNAL_SLOTLINK_HPP

#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include "clientcontext.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TSlotTag, typename TKey>
class SlotLink
{
public:
    using Ptr = std::shared_ptr<SlotLink>;
    using WeakPtr = std::weak_ptr<SlotLink>;
    using Key = TKey;

    static Ptr create(ClientContext context, TKey key = {})
    {
        return Ptr(new SlotLink(key, std::move(context)));
    }

    bool disarm()
    {
        return armed_.exchange(false);
    }

    void remove()
    {
        auto armed = armed_.exchange(false);
        if (armed)
            context_.removeSlot(TSlotTag{}, key_);
    }

    void setKey(Key key) {key_ = key;}

    bool armed() const {return armed_.load();}

    Key key() const {return key_;}

    ClientContext context() const {return context_;}

private:
    SlotLink(TKey key, ClientContext&& context)
        : key_(std::move(key)),
          context_(std::move(context)),
          armed_(true)
    {}

    Key key_;
    ClientContext context_;
    std::atomic<bool> armed_;
};

//------------------------------------------------------------------------------
using SubscriptionLink = SlotLink<SubscriptionTag, ClientLike::SubscriptionKey>;

//------------------------------------------------------------------------------
using RegistrationLink = SlotLink<RegistrationTag, ClientLike::RegistrationKey>;

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_SLOTLINK_HPP
