/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_REGISTRATIONIMPL_HPP
#define CPPWAMP_INTERNAL_REGISTRATIONIMPL_HPP

#include "../registration.hpp"
#include "callee.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class DynamicRegistration : public wamp::Registration
{
public:
    using Slot = std::function<void (Invocation)>;

    static Ptr create(Callee::Ptr callee, Procedure&& procedure, Slot&& slot)
    {
        return Ptr(new DynamicRegistration(callee, std::move(procedure),
                                           std::move(slot)));
    }

protected:
    virtual void invoke(Invocation&& inv, internal::PassKey) override
    {
        slot_(std::move(inv));
    }

private:
    DynamicRegistration(CalleePtr callee, Procedure&& procedure, Slot&& slot)
        : Registration(callee, std::move(procedure)),
          slot_(std::move(slot))
    {}

    Slot slot_;
};

//------------------------------------------------------------------------------
template <typename... TParams>
class StaticRegistration : public wamp::Registration
{
public:
    using Slot = std::function<void (Invocation, TParams...)>;

    static Ptr create(Callee::Ptr callee, Procedure&& procedure, Slot&& slot)
    {
        return Ptr(new StaticRegistration(callee, std::move(procedure),
                                          std::move(slot)));
    }

    virtual void invoke(Invocation&& inv, internal::PassKey) override
    {
        // A copy of inv.args() must be made because of unspecified evaluation
        // order: http://stackoverflow.com/questions/15680489
        Array args = inv.args();
        wamp::Unmarshall<TParams...>::apply(slot_, args, std::move(inv));
    }

private:
    StaticRegistration(CalleePtr callee, Procedure&& procedure, Slot&& slot)
        : Registration(callee, std::move(procedure)),
          slot_(std::move(slot))
    {}

    Slot slot_;
};


} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_REGISTRATIONIMPL_HPP
