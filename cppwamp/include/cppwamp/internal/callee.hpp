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
#include "../args.hpp"
#include "../asyncresult.hpp"
#include "../variant.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class Callee
{
public:
    using Ptr               = std::shared_ptr<Callee>;
    using WeakPtr           = std::weak_ptr<Callee>;
    using RegistrationId    = uint64_t;
    using RequestId         = uint64_t;
    using UnregisterHandler = AsyncHandler<bool>;

    virtual ~Callee() {}

    virtual void unregister(RegistrationId regId) = 0;

    virtual void unregister(RegistrationId regId,
                            UnregisterHandler handler) = 0;

    virtual void yield(RequestId reqId) = 0;

    virtual void yield(RequestId reqId, Args&& args) = 0;

    virtual void fail(RequestId reqId, std::string&& reason, Object&& details,
                      Args&& args) = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CALLEE_HPP
