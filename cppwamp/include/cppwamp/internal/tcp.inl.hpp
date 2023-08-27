/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/tcp.hpp"
#include "rawsockconnector.hpp"
#include "rawsocklistener.hpp"
#include "tcpacceptor.hpp"
#include "tcpopener.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// Making this a nested struct inside Connector<Tcp> leads to bogus Doxygen
// warnings.
//------------------------------------------------------------------------------
struct TcpConnectorImpl
{
    using RawsockConnector = internal::RawsockConnector<internal::TcpOpener>;

    TcpConnectorImpl(IoStrand i, TcpHost s, int codecId)
        : cnct(RawsockConnector::create(std::move(i), std::move(s), codecId))
    {}

    RawsockConnector::Ptr cnct;
};

//------------------------------------------------------------------------------
// Making this a nested struct inside Listener<Tcp> leads to bogus Doxygen
// warnings.
//------------------------------------------------------------------------------
struct TcpListenerImpl
{
    using RawsockListener = internal::RawsockListener<internal::TcpAcceptor>;

    TcpListenerImpl(AnyIoExecutor e, IoStrand i, TcpEndpoint s,
                    std::set<int> codecIds)
        : lstn(RawsockListener::create(std::move(e), std::move(i), std::move(s),
                                       std::move(codecIds)))
    {}

    RawsockListener::Ptr lstn;
};

} // namespace internal


//******************************************************************************
// Connector<Tcp>
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Tcp>::Connector(IoStrand i, Settings s, int codecId)
    : impl_(new internal::TcpConnectorImpl(std::move(i), std::move(s), codecId))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Tcp>::~Connector() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Tcp>::establish(Handler handler)
{
    impl_->cnct->establish(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Tcp>::cancel() {impl_->cnct->cancel();}


//******************************************************************************
// Listener<Tcp>
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Tcp>::Listener(AnyIoExecutor e, IoStrand i, Settings s,
                                       std::set<int> codecIds)
    : Listening(s.label()),
    impl_(new internal::TcpListenerImpl(std::move(e), std::move(i),
                                        std::move(s), std::move(codecIds)))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Tcp>::~Listener() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Tcp>::observe(Handler handler)
{
    impl_->lstn->observe(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Tcp>::establish() {impl_->lstn->establish();}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Tcp>::cancel() {impl_->lstn->cancel();}

} // namespace wamp
