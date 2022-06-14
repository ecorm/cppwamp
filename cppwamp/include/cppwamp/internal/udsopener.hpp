/*------------------------------------------------------------------------------
              Copyright Butterfly Energy Systems 2014-2015, 2022.
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
#include "../udspath.hpp"
#include "config.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class UdsOpener
{
public:
    using Info      = UdsPath;
    using Socket    = boost::asio::local::stream_protocol::socket;
    using SocketPtr = std::unique_ptr<Socket>;

    UdsOpener(AnyExecutor exec, Info info)
        : executor_(exec),
          info_(std::move(info))
    {}

    AnyExecutor executor() {return executor_;}

    template <typename TCallback>
    void establish(TCallback&& callback)
    {
        assert(!socket_ && "Connect already in progress");

        socket_.reset(new Socket(executor_));
        socket_->open();
        info_.options().applyTo(*socket_);

        // AsioConnector will keep this object alive until completion.
        socket_->async_connect(
            info_.pathName(),
            [this, CPPWAMP_MVCAP(callback)](AsioErrorCode ec)
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
    AnyExecutor executor_;
    Info info_;
    SocketPtr socket_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_UDSOPENER_HPP
