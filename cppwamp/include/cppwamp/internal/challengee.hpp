/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CHALLENGEE_HPP
#define CPPWAMP_INTERNAL_CHALLENGEE_HPP

#include <future>
#include <memory>
#include "../erroror.hpp"

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

    virtual ErrorOrDone authenticate(Authentication&&) = 0;

    virtual std::future<ErrorOrDone> safeAuthenticate(Authentication&&) = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_CHALLENGEE_HPP
