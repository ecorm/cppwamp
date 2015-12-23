/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <utility>
#include "asioconnector.hpp"
#include "rawsockconnector.hpp"
#include "tcpopener.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
template <typename TCodec>
Connector::Ptr connector(AsioService& iosvc, TcpHost host)
{
    using Endpoint = internal::AsioConnector<internal::TcpOpener>;
    using ConcreteConnector = internal::RawsockConnector<TCodec, Endpoint>;
    return ConcreteConnector::create(iosvc, std::move(host));
}

} // namespace wamp
