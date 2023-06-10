/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_UDSTRAITS_HPP
#define CPPWAMP_INTERNAL_UDSTRAITS_HPP

#include "../variant.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct UdsTraits
{
    template <typename TEndpoint>
    static Object remoteEndpointDetails(const TEndpoint& ep)
    {
        return Object
        {
            {"path", ep.path()},
            {"protocol", "UDS"},
        };
    }
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_UDSTRAITS_HPP
