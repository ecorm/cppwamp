/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../tcp.hpp"
#include "asioconnector.hpp"
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
    using Endpoint = internal::AsioConnector<internal::TcpOpener>;
    using ConcreteConnector = internal::RawsockConnector<Endpoint>;

    Impl(IoStrand s, TcpHost h, BufferCodecBuilder b)
        : cnct(ConcreteConnector::create(std::move(s), std::move(h),
                                         std::move(b)))
    {}

    Impl(ConcreteConnector::Ptr clone) : cnct(std::move(clone)) {}

    ConcreteConnector::Ptr cnct;
};

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Tcp>::Ptr
Connector<Tcp>::create(const AnyIoExecutor& e, TcpHost h, BufferCodecBuilder b)
{
    using std::move;
    return Ptr(new Connector(boost::asio::make_strand(e), move(h), move(b)));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE IoStrand Connector<Tcp>::strand() const
{
    return impl_->cnct->strand();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connecting::Ptr Connector<Tcp>::clone() const
{
    auto& c = *(impl_->cnct);
    return Ptr(new Connector(c.strand(), c.info(), c.codecBuilder()));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Tcp>::establish(Handler&& handler)
{
    impl_->cnct->establish(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Tcp>::cancel() {impl_->cnct->cancel();}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Tcp>::Connector(IoStrand s, TcpHost h,
                                         BufferCodecBuilder b)
    : impl_(new Impl(std::move(s), std::move(h), std::move(b)))
{}

} // namespace wamp
