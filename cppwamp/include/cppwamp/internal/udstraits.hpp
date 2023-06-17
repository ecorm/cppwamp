/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_UDSTRAITS_HPP
#define CPPWAMP_INTERNAL_UDSTRAITS_HPP

#include <sstream>
#include "../connectioninfo.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
struct UdsTraits
{
    template <typename TEndpoint>
    static ConnectionInfo connectionInfo(const TEndpoint& ep)
    {
        std::ostringstream oss;
        oss << ep;

        Object details
        {
            {"endpoint", oss.str()},
            {"path", ep.path()},
            {"protocol", "UDS"},
        };

        return {std::move(details), oss.str()};
    }
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_UDSTRAITS_HPP
