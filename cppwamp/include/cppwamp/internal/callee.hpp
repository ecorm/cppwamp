/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CALLEE_HPP
#define CPPWAMP_INTERNAL_CALLEE_HPP

#include <cstdint>
#include <memory>
#include <string>
#include "../peerdata.hpp"
#include "../wampdefs.hpp"
#include "asynctask.hpp"

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

    virtual void unregister(const Registration& reg) = 0;

    virtual void unregister(const Registration& reg,
                            AsyncTask<bool>&& handler) = 0;

    virtual void yield(RequestId reqId, wamp::Result&& result) = 0;

    virtual void yield(RequestId reqId, wamp::Error&& failure) = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CALLEE_HPP
