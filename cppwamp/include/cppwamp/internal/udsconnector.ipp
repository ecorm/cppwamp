/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <utility>
#include "asioconnector.hpp"
#include "clientimpl.hpp"
#include "config.hpp"
#include "udsopener.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE UdsConnector::Ptr UdsConnector::create(AsioService& iosvc,
        const std::string& path, CodecId codecId, RawsockMaxLength maxRxLength)
{
    return Ptr( new UdsConnector({iosvc, path, codecId, maxRxLength}) );
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector::Ptr UdsConnector::clone() const
{
    return Connector::Ptr(new UdsConnector(info_));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void UdsConnector::establish(Handler handler)
{
    CPPWAMP_LOGIC_CHECK(!impl_, "Connection already in progress");
    const Info& i = info_;
    using internal::UdsOpener;
    impl_.reset(new Impl(UdsOpener(i.iosvc, i.path), i.codecId, i.maxRxLength));

    auto self = shared_from_this();

    // AsioConnector will keep this object alive until completion.
    impl_->establish([this, handler](std::error_code ec, CodecId codecId,
                                     Impl::Transport::Ptr trnsp)
    {
        internal::ClientImplBase::Ptr clientImpl;
        if (!ec)
            clientImpl = internal::createClientImpl(codecId, std::move(trnsp));
        info_.iosvc.post(std::bind(handler, ec, clientImpl));
        impl_.reset();
    });
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void UdsConnector::cancel()
{
    if (impl_)
        impl_->cancel();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE UdsConnector::UdsConnector(Info info)
    : info_(std::move(info))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE UdsConnector::Ptr UdsConnector::shared_from_this()
{
    return std::static_pointer_cast<UdsConnector>(
                Connector::shared_from_this() );
}

} // namespace wamp
