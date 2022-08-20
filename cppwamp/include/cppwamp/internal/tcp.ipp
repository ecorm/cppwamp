/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../tcp.hpp"
#include "asiotransport.hpp"
#include "rawsockconnector.hpp"
#include "tcpopener.hpp"

namespace wamp
{
//******************************************************************************
// TcpConnector
//******************************************************************************

//------------------------------------------------------------------------------
struct Connector<Tcp>::Impl
{
    using RawsockOpener = internal::RawsockConnector<internal::TcpOpener>;

    Impl(IoStrand i, Settings s, int codecId)
        : cnct(RawsockOpener::create(std::move(i), std::move(s), codecId))
    {}

    RawsockOpener::Ptr cnct;
};

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Tcp>::Connector(IoStrand i, Settings s, int codecId)
    : impl_(new Impl(std::move(i), std::move(s), codecId))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Tcp>::~Connector() {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Tcp>::establish(Handler&& handler)
{
    impl_->cnct->establish(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Tcp>::cancel() {impl_->cnct->cancel();}

} // namespace wamp
