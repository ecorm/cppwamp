/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_UDSACCEPTOR_HPP
#define CPPWAMP_INTERNAL_UDSACCEPTOR_HPP

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
class UdsAcceptor
{
public:
    using Settings  = UdsPath;
    using Socket    = boost::asio::local::stream_protocol::socket;
    using SocketPtr = std::unique_ptr<Socket>;

    template <typename TExecutorOrStrand>
    UdsAcceptor(TExecutorOrStrand&& exec, Settings s)
        : strand_(std::forward<TExecutorOrStrand>(exec)),
          settings_(std::move(s))
    {}

    template <typename F>
    void establish(F&& callback)
    {
        struct Accepted
        {
            UdsAcceptor* self;
            typename std::decay<F>::type callback;

            void operator()(boost::system::error_code asioEc)
            {
                SocketPtr socket{std::move(self->socket_)};
                self->socket_.reset();
                if (self->checkError(asioEc, callback))
                    callback(std::move(socket));
            }
        };

        assert(!socket_ && "Accept already in progress");

        // Acceptor must be constructed lazily to give a chance to delete
        // remnant file.
        if (!acceptor_)
        {
            if (settings_.deletePathEnabled())
                std::remove(settings_.pathName().c_str());
            acceptor_.reset(new Acceptor(strand_, settings_.pathName()));
        }
        socket_.reset(new Socket(strand_));

        // RawsockListener will keep this object alive until completion.
        acceptor_->async_accept(*socket_,
                                Accepted{this, std::forward<F>(callback)});
    }

    void cancel()
    {
        if (acceptor_)
            acceptor_->cancel();
    }

private:
    using Acceptor = boost::asio::local::stream_protocol::acceptor;

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
    std::unique_ptr<Acceptor> acceptor_;
    SocketPtr socket_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_UDSACCEPTOR_HPP
