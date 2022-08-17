/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../uds.hpp"
#include "asioconnector.hpp"
#include "rawsockconnector.hpp"
#include "udsopener.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
struct Connector<Uds>::Impl
{
    using Endpoint = internal::AsioConnector<internal::UdsOpener>;
    using ConcreteConnector = internal::RawsockConnector<Endpoint>;

    Impl(IoStrand s, UdsPath p, int codecId)
        : cnct(ConcreteConnector::create(std::move(s), std::move(p), codecId))
    {}

    ConcreteConnector::Ptr cnct;
};

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Uds>::Connector(IoStrand s, UdsPath h, int codecId)
    : impl_(new Impl(std::move(s), std::move(h), codecId))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Uds>::~Connector() {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Uds>::establish(Handler&& handler)
{
    impl_->cnct->establish(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Uds>::cancel() {impl_->cnct->cancel();}

} // namespace wamp
