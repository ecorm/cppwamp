/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_UDSOPENER_HPP
#define CPPWAMP_INTERNAL_UDSOPENER_HPP

#include <cassert>
#include <memory>
#include <string>
#include <boost/asio/local/stream_protocol.hpp>
#include "../asiodefs.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class UdsOpener
{
public:
    using Socket    = boost::asio::local::stream_protocol::socket;
    using SocketPtr = std::unique_ptr<Socket>;

    UdsOpener(AsioService& iosvc, const std::string& path)
        : iosvc_(iosvc), path_(path)
    {}

    AsioService& iosvc() {return iosvc_;}

    template <typename TCallback>
    void establish(TCallback&& callback)
    {
        assert(!socket_ && "Connect already in progress");

        socket_.reset(new Socket(iosvc_));

        // AsioConnector will keep this object alive until completion.
        socket_->async_connect(path_, [this, callback](AsioErrorCode ec)
        {
            if (ec)
                socket_.reset();
            callback(ec, std::move(socket_));
            socket_.reset();
        });
    }

    void cancel()
    {
        if (socket_)
            socket_->close();
    }

private:
    AsioService& iosvc_;
    std::string path_;
    SocketPtr socket_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_UDSOPENER_HPP
