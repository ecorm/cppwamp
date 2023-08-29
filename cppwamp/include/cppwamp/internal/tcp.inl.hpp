/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/tcp.hpp"
#include "tcpconnector.hpp"
#include "tcplistener.hpp"

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
    using ConnectorType = internal::TcpConnector;

    TcpConnectorImpl(IoStrand i, TcpHost s, int codecId)
        : cnct(ConnectorType::create(std::move(i), std::move(s), codecId))
    {}

    ConnectorType::Ptr cnct;
};

//------------------------------------------------------------------------------
// Making this a nested struct inside Listener<Tcp> leads to bogus Doxygen
// warnings.
//------------------------------------------------------------------------------
struct TcpListenerImpl
{
    using ListenerType = internal::TcpListener;

    TcpListenerImpl(AnyIoExecutor e, IoStrand i, TcpEndpoint s, CodecIdSet c)
        : lstn(ListenerType::create(std::move(e), std::move(i), std::move(s),
                                    std::move(c)))
    {}

    ListenerType::Ptr lstn;
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
                                       CodecIdSet c)
    : Listening(s.label()),
      impl_(new internal::TcpListenerImpl(std::move(e), std::move(i),
                                          std::move(s), std::move(c)))
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
