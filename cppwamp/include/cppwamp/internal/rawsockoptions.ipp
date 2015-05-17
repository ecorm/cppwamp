/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <boost/asio/socket_base.hpp>
#include <boost/asio/ip/unicast.hpp>
#include <boost/asio/ip/v6_only.hpp>

namespace wamp
{

//------------------------------------------------------------------------------
namespace internal
{

template <typename TRawsockOptions, typename TSocket>
void applyRawsockOptions(const TRawsockOptions& options, TSocket& socket)
{
    options.socketOptions_.applyTo(socket);
}

} // namespace internal


//------------------------------------------------------------------------------
template <typename D, typename P>
RawsockOptions<D,P>::RawsockOptions()
    : maxRxLength_(defaultMaxRxLength)
{}

template <typename D, typename P>
D& RawsockOptions<D,P>::withMaxRxLength(RawsockMaxLength length)
{
    maxRxLength_ = length;
    return static_cast<D&>(*this);
}

template <typename D, typename P>
D& RawsockOptions<D,P>::withBroadcast(bool enabled)
{
    return addOption(boost::asio::socket_base::broadcast(enabled));
}

template <typename D, typename P>
D& RawsockOptions<D,P>::withDebug(bool enabled)
{
    return addOption(boost::asio::socket_base::debug(enabled));
}

template <typename D, typename P>
D& RawsockOptions<D,P>::withDoNotRoute(bool enabled)
{
    return addOption(boost::asio::socket_base::do_not_route(enabled));
}

template <typename D, typename P>
D& RawsockOptions<D,P>::withKeepAlive(bool enabled)
{
    return addOption(boost::asio::socket_base::keep_alive(enabled));
}

template <typename D, typename P>
D& RawsockOptions<D,P>::withLinger(bool enabled, int timeout)
{
    return addOption(boost::asio::socket_base::linger(enabled, timeout));
}

template <typename D, typename P>
D& RawsockOptions<D,P>::withReceiveBufferSize(int size)
{
    return addOption(boost::asio::socket_base::receive_buffer_size(size));
}

template <typename D, typename P>
D& RawsockOptions<D,P>::withReceiveLowWatermark(int size)
{
    return addOption(boost::asio::socket_base::receive_low_watermark(size));
}

template <typename D, typename P>
D& RawsockOptions<D,P>::withReuseAddress(bool enabled)
{
    return addOption(boost::asio::socket_base::reuse_address(enabled));
}

template <typename D, typename P>
D& RawsockOptions<D,P>::withSendBufferSize(int size)
{
    return addOption(boost::asio::socket_base::send_buffer_size(size));
}

template <typename D, typename P>
D& RawsockOptions<D,P>::withSendLowWatermark(int size)
{
    return addOption(boost::asio::socket_base::send_low_watermark(size));
}

template <typename D, typename P>
RawsockMaxLength RawsockOptions<D,P>::maxRxLength() const {return maxRxLength_;}


//------------------------------------------------------------------------------
template <typename D, typename P>
IpOptions<D,P>::IpOptions() {}

template <typename D, typename P>
D& IpOptions<D,P>::withUnicastHops(int hops)
{
    return this->addOption(boost::asio::ip::unicast::hops(hops));
}

template <typename D, typename P>
D& IpOptions<D,P>::withIpV6Only(bool enabled)
{
    return this->addOption(boost::asio::ip::v6_only(true));
}


} // namespace wamp

