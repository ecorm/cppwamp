/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_UDSCONNECTOR_HPP
#define CPPWAMP_INTERNAL_UDSCONNECTOR_HPP

#include <array>
#include "rawsockconnector.hpp"
#include "rawsocktransport.hpp"
#include "udstraits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class UdsResolver
{
public:
    using Settings = UdsHost;
    using Result   = std::array<std::string, 1>;

    UdsResolver(const IoStrand&) {}

    template <typename F>
    void resolve(const Settings& settings, F&& callback)
    {
        callback(boost::system::error_code{}, Result{settings.address()});
    }

    void cancel() {}
};

//------------------------------------------------------------------------------
template <typename TTransport>
using BasicUdsConnectorConfig =
    BasicRawsockConnectorConfig<UdsTraits, UdsResolver, TTransport>;

//------------------------------------------------------------------------------
using UdsConnectorConfig =
    BasicUdsConnectorConfig<
        RawsockClientTransport<BasicRawsockTransportConfig<UdsTraits>>>;

//------------------------------------------------------------------------------
using UdsConnector = RawsockConnector<UdsConnectorConfig>;

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_UDSCONNECTOR_HPP
