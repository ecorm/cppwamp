/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_REGISTRATIONIMPL_HPP
#define CPPWAMP_INTERNAL_REGISTRATIONIMPL_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include "../args.hpp"
#include "../invocation.hpp"
#include "../wampdefs.hpp"
#include "callee.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class RegistrationBase
{
public:
    using CalleePtr         = Callee::WeakPtr;
    using Ptr               = std::shared_ptr<RegistrationBase>;
    using WeakPtr           = std::weak_ptr<RegistrationBase>;
    using UnregisterHandler = Callee::UnregisterHandler;

    virtual ~RegistrationBase() {unregister();}

    const std::string& procedure() const {return procedure_;}

    RegistrationId id() const {return id_;}

    void setId(RegistrationId id) {id_ = id;}

    virtual void invoke(Invocation&& inv, Args&& args) = 0;

    void unregister()
    {
        if (!callee_.expired())
            callee_.lock()->unregister(id_);
    }

    void unregister(UnregisterHandler handler)
    {
        if (!callee_.expired())
            callee_.lock()->unregister(id_, std::move(handler));
    }

protected:
    RegistrationBase(CalleePtr callee, std::string&& procedure)
        : callee_(callee), procedure_(std::move(procedure)), id_(0)
    {}

private:
    CalleePtr callee_;
    std::string procedure_;
    RegistrationId id_;
};

//------------------------------------------------------------------------------
template <typename... Ts>
class RegistrationImpl : public RegistrationBase
{
public:
    using Slot = std::function<void (Invocation, Ts...)>;

    static Ptr create(CalleePtr callee, std::string procedure, Slot slot)
    {
        using std::move;
        return Ptr(new RegistrationImpl(callee, move(procedure), move(slot)));
    }

    virtual void invoke(Invocation&& inv, Args&& args) override
    {
        using std::move;
        wamp::Unmarshall<Ts...>::apply(slot_, args.list, move(inv));
    }

private:
    RegistrationImpl(CalleePtr callee, std::string&& procedure, Slot&& slot)
        : RegistrationBase(callee, std::move(procedure)), slot_(std::move(slot))
    {}

    Slot slot_;
};

//------------------------------------------------------------------------------
template <>
class RegistrationImpl<Args> : public RegistrationBase
{
public:
    using Slot = std::function<void (Invocation, Args)>;

    static Ptr create(CalleePtr callee, std::string procedure, Slot slot)
    {
        return Ptr(new RegistrationImpl(callee, std::move(procedure),
                                        std::move(slot)));
    }

    virtual void invoke(Invocation&& inv, Args&& args) override
    {
        slot_(std::move(inv), std::move(args));
    }

private:
    RegistrationImpl(CalleePtr callee, std::string&& procedure, Slot&& slot)
        : RegistrationBase(callee, std::move(procedure)), slot_(std::move(slot))
    {}

    Slot slot_;
};

//------------------------------------------------------------------------------
template <>
class RegistrationImpl<void> : public RegistrationBase
{
public:
    using Slot = std::function<void (Invocation)>;

    static Ptr create(CalleePtr callee, std::string procedure, Slot slot)
    {
        using std::move;
        return Ptr(new RegistrationImpl(callee, move(procedure), move(slot)));
    }

    virtual void invoke(Invocation&& inv, Args&&) override
    {
        slot_(std::move(inv));
    }

private:
    RegistrationImpl(CalleePtr callee, std::string&& procedure, Slot&& slot)
        : RegistrationBase(callee, std::move(procedure)), slot_(std::move(slot))
    {}

    Slot slot_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_REGISTRATIONIMPL_HPP
