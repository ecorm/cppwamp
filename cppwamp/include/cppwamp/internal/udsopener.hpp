/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_UDSOPENER_HPP
#define CPPWAMP_INTERNAL_UDSOPENER_HPP

#include <cassert>
#include <memory>
#include <string>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/strand.hpp>
#include "../asiodefs.hpp"
#include "../udspath.hpp"

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

    template <typename TExecutorOrStrand>
    UdsOpener(TExecutorOrStrand&& exec, Info info)
        : strand_(std::forward<TExecutorOrStrand>(exec)),
          info_(std::move(info))
    {}

    IoStrand strand() {return strand_;}

    template <typename F>
    void establish(F&& callback)
    {
        struct Connected
        {
            UdsOpener* self;
            typename std::decay<F>::type callback;

            void operator()(AsioErrorCode ec)
            {
                if (ec)
                    self->socket_.reset();
                callback(ec, std::move(self->socket_));
                self->socket_.reset();
            }
        };

        assert(!socket_ && "Connect already in progress");

        socket_.reset(new Socket(strand_));
        socket_->open();
        info_.options().applyTo(*socket_);

        // AsioConnector will keep this object alive until completion.
        socket_->async_connect(info_.pathName(),
                               Connected{this, std::forward<F>(callback)});
    }

    void cancel()
    {
        if (socket_)
            socket_->close();
    }

private:
    IoStrand strand_;
    Info info_;
    SocketPtr socket_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_UDSOPENER_HPP
