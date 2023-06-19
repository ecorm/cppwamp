/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../tcp.hpp"
#include "rawsockconnector.hpp"
#include "rawsocklistener.hpp"
#include "tcpacceptor.hpp"
#include "tcpopener.hpp"

namespace wamp
{

//******************************************************************************
// Connector<Tcp>
//******************************************************************************

//------------------------------------------------------------------------------
struct Connector<Tcp>::Impl
{
    using RawsockConnector = internal::RawsockConnector<internal::TcpOpener>;

    Impl(IoStrand i, Settings s, int codecId)
        : cnct(RawsockConnector::create(std::move(i), std::move(s), codecId))
    {}

    RawsockConnector::Ptr cnct;
};

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Tcp>::Connector(IoStrand i, Settings s, int codecId)
    : impl_(new Impl(std::move(i), std::move(s), codecId))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Tcp>::Connector(Connector&&) = default;

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Tcp>::~Connector() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Tcp>& Connector<Tcp>::operator=(Connector&&) = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Tcp>::establish(Handler&& handler)
{
    impl_->cnct->establish(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Connector<Tcp>::cancel() {impl_->cnct->cancel();}


//******************************************************************************
// Listener<Tcp>
//******************************************************************************

//------------------------------------------------------------------------------
struct Listener<Tcp>::Impl
{
    using RawsockListener = internal::RawsockListener<internal::TcpAcceptor>;

    Impl(IoStrand i, Settings s, CodecIds codecIds)
        : lstn(RawsockListener::create(std::move(i), std::move(s),
                                       std::move(codecIds)))
    {}

    RawsockListener::Ptr lstn;
};

//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Tcp>::Listener(IoStrand i, Settings s,
                                       std::set<int> codecIds)
    : Listening(s.label()),
      impl_(new Impl(std::move(i), std::move(s), std::move(codecIds)))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Tcp>::Listener(Listener&&) = default;

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors due to unique_ptr.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Tcp>::~Listener() = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Tcp>& Listener<Tcp>::operator=(Listener&&) = default;

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Tcp>::establish(Handler&& handler)
{
    impl_->lstn->establish(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Tcp>::cancel() {impl_->lstn->cancel();}

} // namespace wamp
