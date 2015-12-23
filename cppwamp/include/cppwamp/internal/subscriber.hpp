/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_SUBSCRIBER_HPP
#define CPPWAMP_INTERNAL_SUBSCRIBER_HPP

#include <functional>
#include <memory>
#include "../wampdefs.hpp"
#include "asynctask.hpp"

namespace wamp
{

class Subscription;

namespace internal
{

//------------------------------------------------------------------------------
class Subscriber
{
public:
    using WeakPtr = std::weak_ptr<Subscriber>;

    virtual ~Subscriber() {}

    virtual void unsubscribe(const Subscription& sub) = 0;

    virtual void unsubscribe(const Subscription& sub,
                             AsyncTask<bool>&& handler) = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_SUBSCRIBER_HPP
