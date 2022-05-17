/*------------------------------------------------------------------------------
                   Copyright Butterfly Energy Systems 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CHALLENGEE_HPP
#define CPPWAMP_INTERNAL_CHALLENGEE_HPP

#include <memory>

namespace wamp
{

class Authentication;

namespace internal
{

//------------------------------------------------------------------------------
class Challengee
{
public:
    using WeakPtr = std::weak_ptr<Challengee>;

    virtual ~Challengee() {}

    virtual void authenticate(Authentication&& authentication) = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CHALLENGEE_HPP
