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
using UdsClientTransport = RawsockClientTransport<UdsTraits>;

//------------------------------------------------------------------------------
class UdsResolver
{
public:
    using Traits    = UdsTraits;
    using Settings  = UdsHost;
    using Transport = UdsClientTransport;
    using Result    = std::array<std::string, 1>;

    UdsResolver(const IoStrand&) {}

    template <typename F>
    void resolve(const Settings& settings, F&& callback)
    {
        callback(boost::system::error_code{}, Result{settings.address()});
    }

    void cancel() {}
};

//------------------------------------------------------------------------------
using UdsClientTransport = RawsockClientTransport<UdsTraits>;

//------------------------------------------------------------------------------
class UdsConnector : public RawsockConnector<UdsResolver>
{
    using Base = RawsockConnector<UdsResolver>;

public:
    using Ptr = std::shared_ptr<UdsConnector>;
    using Base::Base;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_UDSCONNECTOR_HPP
