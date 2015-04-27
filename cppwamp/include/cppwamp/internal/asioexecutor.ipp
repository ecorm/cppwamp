/*------------------------------------------------------------------------------
                     Copyright Emile Cormier 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include "config.hpp"

namespace wamp
{

CPPWAMP_INLINE AsioExecutor::AsioExecutor(AsioService& iosvc)
    : iosvc_(iosvc)
{}

CPPWAMP_INLINE AsioExecutor::~AsioExecutor() {close();}

CPPWAMP_INLINE AsioService& AsioExecutor::iosvc() {return iosvc_;}

CPPWAMP_INLINE const AsioService& AsioExecutor::iosvc() const
{
    return iosvc_;
}

CPPWAMP_INLINE void AsioExecutor::close()
{
    if (!closed())
    {
        isClosed_ = true;
        iosvc_.stop();;
    }
}

CPPWAMP_INLINE bool AsioExecutor::closed() const {return isClosed_;}

CPPWAMP_INLINE bool AsioExecutor::try_executing_one()
{
    return iosvc_.poll_one() != 0;
}

CPPWAMP_INLINE void AsioExecutor::loop()
{
    while ( !closed() &&
            (iosvc().run_one() != 0) )
    {}
    if (!iosvc_.stopped())
        run_queued_closures();
}

CPPWAMP_INLINE void AsioExecutor::run_queued_closures()
{
    while (iosvc_.poll_one() != 0) {}
}

} // namespace wamp
