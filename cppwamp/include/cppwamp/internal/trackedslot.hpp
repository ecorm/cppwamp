/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TRACKEDSLOT_HPP
#define CPPWAMP_INTERNAL_TRACKEDSLOT_HPP

#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include "../anyhandler.hpp"
#include "../pubsubinfo.hpp"
#include "../rpcinfo.hpp"
#include "clientcontext.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TSlotTag, typename TKey, typename TSignature>
class TrackedSlot
{
public:
    using Ptr = std::shared_ptr<TrackedSlot>;
    using WeakPtr = std::weak_ptr<TrackedSlot>;
    using Key = TKey;
    using Handler = AnyReusableHandler<TSignature>;

    static Ptr create(TKey key, Handler&& f, ClientContext context)
    {
        return Ptr(new TrackedSlot(key, std::move(f), std::move(context)));
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

    template <typename... Ts>
    auto invoke(Ts&&... args) const
        -> decltype(std::declval<const Handler&>()(std::forward<Ts>(args)...))
    {
        assert(handler_ != nullptr);
        return handler_(std::forward<Ts>(args)...);
    }

    bool armed() const {return armed_.load();}

    Key key() const {return key_;}

    AnyCompletionExecutor executor() const
    {
        return boost::asio::get_associated_executor(handler_);
    }

private:
    TrackedSlot(TKey key, Handler&& f, ClientContext&& context)
        : handler_(std::move(f)),
          key_(std::move(key)),
          context_(std::move(context)),
          armed_(true)
    {}

    Handler handler_;
    Key key_;
    ClientContext context_;
    std::atomic<bool> armed_;
};

//------------------------------------------------------------------------------
using TrackedEventSlot = TrackedSlot<EventSlotTag, ClientLike::EventSlotKey,
                                     void (Event)>;

//------------------------------------------------------------------------------
using TrackedCallSlot = TrackedSlot<CallSlotTag, ClientLike::CallSlotKey,
                                     Outcome (Invocation)>;

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TRACKEDSLOT_HPP
