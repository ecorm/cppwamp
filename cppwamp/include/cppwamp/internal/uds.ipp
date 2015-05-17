/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <utility>
#include "asioconnector.hpp"
#include "legacyasioendpoint.hpp"
#include "rawsockconnector.hpp"
#include "udsopener.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
template <typename TCodec>
Connector::Ptr connector(AsioService& iosvc, UdsPath path)
{
    using Endpoint = internal::AsioConnector<internal::UdsOpener>;
    using ConcreteConnector = internal::RawsockConnector<TCodec, Endpoint>;
    return ConcreteConnector::create(iosvc, std::move(path));
}


namespace legacy
{

//------------------------------------------------------------------------------
template <typename TCodec>
Connector::Ptr connector(AsioService& iosvc, UdsPath path)
{
    using Endpoint = internal::LegacyAsioEndpoint<internal::UdsOpener>;
    using ConcreteConnector = internal::RawsockConnector<TCodec, Endpoint>;
    return ConcreteConnector::create(iosvc, std::move(path));
}

} // namespace legacy

} // namespace wamp
