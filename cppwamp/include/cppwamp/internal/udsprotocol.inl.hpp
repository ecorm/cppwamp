/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../udsprotocol.hpp"
#include <boost/asio/local/stream_protocol.hpp>
#include "../api.hpp"

namespace wamp
{

// NOLINTBEGIN(readability-inconsistent-declaration-parameter-name)
CPPWAMP_INLINE UdsOptions& UdsOptions::withBroadcast(bool b)          {return set<boost::asio::socket_base::broadcast>(b);}
CPPWAMP_INLINE UdsOptions& UdsOptions::withDebug(bool b)              {return set<boost::asio::socket_base::debug>(b);}
CPPWAMP_INLINE UdsOptions& UdsOptions::withDoNotRoute(bool b)         {return set<boost::asio::socket_base::do_not_route>(b);}
CPPWAMP_INLINE UdsOptions& UdsOptions::withKeepAlive(bool b)          {return set<boost::asio::socket_base::keep_alive>(b);}
CPPWAMP_INLINE UdsOptions& UdsOptions::withLinger(bool b, int n)      {return set<boost::asio::socket_base::linger>(b, n);}
CPPWAMP_INLINE UdsOptions& UdsOptions::withOutOfBandInline(bool b)    {return set<boost::asio::socket_base::out_of_band_inline>(b);}
CPPWAMP_INLINE UdsOptions& UdsOptions::withReceiveBufferSize(int n)   {return set<boost::asio::socket_base::receive_buffer_size>(n);}
CPPWAMP_INLINE UdsOptions& UdsOptions::withReceiveLowWatermark(int n) {return set<boost::asio::socket_base::receive_low_watermark>(n);}
CPPWAMP_INLINE UdsOptions& UdsOptions::withReuseAddress(bool b)       {return set<boost::asio::socket_base::reuse_address>(b);}
CPPWAMP_INLINE UdsOptions& UdsOptions::withSendBufferSize(int n)      {return set<boost::asio::socket_base::send_buffer_size>(n);}
CPPWAMP_INLINE UdsOptions& UdsOptions::withSendLowWatermark(int n)    {return set<boost::asio::socket_base::send_low_watermark>(n);}
// NOLINTEND(readability-inconsistent-declaration-parameter-name)

template <typename TOption, typename... TArgs>
UdsOptions& UdsOptions::set(TArgs... args)
{
    options_.add(TOption(args...));
    return *this;
}

template <typename TSocket>
void UdsOptions::applyTo(TSocket& socket) const {options_.applyTo(socket);}

// Explicit template instantiation
template void
UdsOptions::applyTo(boost::asio::local::stream_protocol::socket&) const;

} // namespace wamp
