/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CALLER_HPP
#define CPPWAMP_INTERNAL_CALLER_HPP

#include <future>
#include <memory>
#include "../erroror.hpp"
#include "../wampdefs.hpp"

namespace wamp
{

class CallerOutputChunk;

namespace internal
{

//------------------------------------------------------------------------------
class Caller
{
public:
    using WeakPtr = std::weak_ptr<Caller>;

    virtual ~Caller() {}

    virtual ErrorOrDone cancelCall(RequestId, CallCancelMode) = 0;

    virtual std::future<ErrorOrDone> safeCancelCall(RequestId,
                                                    CallCancelMode) = 0;

    virtual ErrorOrDone sendCallerChunk(CallerOutputChunk&&) = 0;

    virtual std::future<ErrorOrDone>
    safeSendCallerChunk(CallerOutputChunk&&) = 0;

    virtual std::future<ErrorOrDone> safeCancelStream(RequestId) = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CALLER_HPP
