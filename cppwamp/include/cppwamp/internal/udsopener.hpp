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
#include "udstraits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class UdsOpener
{
public:
    using Settings  = UdsPath;
    using Socket    = boost::asio::local::stream_protocol::socket;
    using SocketPtr = std::unique_ptr<Socket>;
    using Traits    = UdsTraits;

    template <typename TExecutorOrStrand>
    UdsOpener(TExecutorOrStrand&& exec, Settings s)
        : strand_(std::forward<TExecutorOrStrand>(exec)),
          settings_(std::move(s))
    {}

    template <typename F>
    void establish(F&& callback)
    {
        struct Connected
        {
            UdsOpener* self;
            typename std::decay<F>::type callback;

            void operator()(boost::system::error_code asioEc)
            {
                SocketPtr socket{std::move(self->socket_)};
                self->socket_.reset();
                if (self->checkError(asioEc, callback))
                    callback(std::move(socket));
            }
        };

        assert(!socket_ && "Connect already in progress");

        socket_.reset(new Socket(strand_));
        socket_->open();
        settings_.options().applyTo(*socket_);

        // RawsockConnector will keep this object alive until completion.
        socket_->async_connect(settings_.pathName(),
                               Connected{this, std::forward<F>(callback)});
    }

    void cancel()
    {
        if (socket_)
            socket_->close();
    }

private:
    template <typename F>
    bool checkError(boost::system::error_code asioEc, F& callback)
    {
        if (asioEc)
        {
            auto ec = static_cast<std::error_code>(asioEc);
            callback(UnexpectedError(ec));
        }
        return !asioEc;
    }

    IoStrand strand_;
    Settings settings_;
    SocketPtr socket_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_UDSOPENER_HPP
