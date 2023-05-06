/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CALLEE_HPP
#define CPPWAMP_INTERNAL_CALLEE_HPP

#include <cstdint>
#include <future>
#include <memory>
#include "../erroror.hpp"
#include "../wampdefs.hpp"

namespace wamp
{

class CalleeOutputChunk;
class Error;
class Registration;
class Result;

namespace internal
{

//------------------------------------------------------------------------------
class Callee
{
public:
    using WeakPtr = std::weak_ptr<Callee>;

    virtual ~Callee() {}

    virtual void safeUnregister(const Registration&) = 0;

    virtual ErrorOrDone yield(Result&&, RequestId, RegistrationId) = 0;

    virtual std::future<ErrorOrDone> safeYield(Result&&, RequestId,
                                               RegistrationId) = 0;

    virtual ErrorOrDone yield(Error&&, RequestId, RegistrationId) = 0;

    virtual std::future<ErrorOrDone> safeYield(Error&&, RequestId,
                                               RegistrationId) = 0;

    virtual ErrorOrDone yield(CalleeOutputChunk&&, RequestId,
                              RegistrationId) = 0;

    virtual std::future<ErrorOrDone> safeYield(CalleeOutputChunk&&,
                                               RequestId, RegistrationId) = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CALLEE_HPP
