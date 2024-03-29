/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CALLEE_HPP
#define CPPWAMP_INTERNAL_CALLEE_HPP

#include <cstdint>
#include <memory>
#include "../anyhandler.hpp"
#include "../erroror.hpp"
#include "../peerdata.hpp"
#include "../wampdefs.hpp"

namespace wamp
{

class Registration;

namespace internal
{

//------------------------------------------------------------------------------
class Callee
{
public:
    using WeakPtr = std::weak_ptr<Callee>;

    virtual ~Callee() {}

    virtual void safeUnregister(const Registration&) = 0;

    virtual void safeUnregister(
        const Registration&,
        AnyCompletionHandler<void(ErrorOr<bool>)>&& handler) = 0;

    virtual ErrorOrDone yield(RequestId, wamp::Result&&) = 0;

    virtual std::future<ErrorOrDone> safeYield(RequestId, wamp::Result&&) = 0;

    virtual ErrorOrDone yield(RequestId, wamp::Error&&) = 0;

    virtual std::future<ErrorOrDone> safeYield(RequestId, wamp::Error&&) = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CALLEE_HPP
