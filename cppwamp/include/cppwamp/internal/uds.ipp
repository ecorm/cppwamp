/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../uds.hpp"
#include "rawsockconnector.hpp"
#include "rawsocklistener.hpp"
#include "udsacceptor.hpp"
#include "udsopener.hpp"

namespace wamp
{

//******************************************************************************
// Connector<Uds>
//******************************************************************************

//------------------------------------------------------------------------------
struct Connector<Uds>::Impl
{
    using RawsockOpener = internal::RawsockConnector<internal::UdsOpener>;

    Impl(IoStrand i, Settings s, int codecId)
        : cnct(RawsockOpener::create(std::move(i), std::move(s), codecId))
    {}

    RawsockOpener::Ptr cnct;
};

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector<Uds>::Connector(IoStrand i, Settings s, int codecId)
    : impl_(new Impl(std::move(i), std::move(s), codecId))
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


//******************************************************************************
// Listener<Uds>
//******************************************************************************

//------------------------------------------------------------------------------
struct Listener<Uds>::Impl
{
    using RawsockListener = internal::RawsockListener<internal::UdsAcceptor>;

    Impl(IoStrand i, Settings s, CodecIds codecIds)
        : lstn(RawsockListener::create(std::move(i), std::move(s),
                                       std::move(codecIds)))
    {}

    RawsockListener::Ptr lstn;
};

CPPWAMP_INLINE Listener<Uds>::Listener(IoStrand i, Settings s,
                                       std::set<int> codecIds)
    : Listening(s.label()),
      impl_(new Impl(std::move(i), std::move(s), std::move(codecIds)))
{}

//------------------------------------------------------------------------------
// Needed to avoid incomplete type errors.
//------------------------------------------------------------------------------
CPPWAMP_INLINE Listener<Uds>::~Listener() {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Uds>::establish(Handler&& handler)
{
    impl_->lstn->establish(std::move(handler));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void Listener<Uds>::cancel() {impl_->lstn->cancel();}

} // namespace wamp
