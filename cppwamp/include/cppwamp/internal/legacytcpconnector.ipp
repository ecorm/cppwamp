/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <utility>
#include "client.hpp"
#include "config.hpp"
#include "legacyasioendpoint.hpp"
#include "tcpopener.hpp"

namespace wamp
{

namespace legacy
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE TcpConnector::Ptr TcpConnector::create(AsioService& iosvc,
        const std::string& hostName, const std::string& serviceName,
        CodecId codecId, RawsockMaxLength maxLength)
{
    return Ptr( new TcpConnector({iosvc, hostName, serviceName, codecId,
                                  maxLength}) );
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE TcpConnector::Ptr TcpConnector::create(AsioService& iosvc,
        const std::string& hostName, unsigned short port, CodecId codecId,
        RawsockMaxLength maxLength)
{
    return Ptr( new TcpConnector({iosvc, hostName, std::to_string(port),
                                  codecId, maxLength}) );
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connector::Ptr TcpConnector::clone() const
{
    return Connector::Ptr(new TcpConnector(info_));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void TcpConnector::establish(Handler handler)
{
    CPPWAMP_LOGIC_CHECK(!impl_, "Connection already in progress");
    const Info& i = info_;
    using internal::TcpOpener;
    impl_.reset(new Impl(TcpOpener(i.iosvc, i.hostName, i.serviceName),
                         i.codecId, i.maxRxLength));

    auto self = shared_from_this();
    impl_->establish([this, self, handler](std::error_code ec, CodecId codecId,
                                           Impl::Transport::Ptr trnsp)
    {
        internal::ClientInterface::Ptr client;
        if (!ec)
            client = internal::createClient(codecId, std::move(trnsp));
        info_.iosvc.post(std::bind(handler, ec, client));
        impl_.reset();
    });
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE void TcpConnector::cancel()
{
    if (impl_)
        impl_->cancel();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE TcpConnector::TcpConnector(Info info)
    : info_(std::move(info))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE TcpConnector::Ptr TcpConnector::shared_from_this()
{
    return std::static_pointer_cast<TcpConnector>(
                Connector::shared_from_this() );
}

} // namespace legacy

} // namespace wamp
