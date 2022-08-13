/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TCPOPENER_HPP
#define CPPWAMP_INTERNAL_TCPOPENER_HPP

#include <cassert>
#include <memory>
#include <string>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include "../asiodefs.hpp"
#include "../tcphost.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class TcpOpener
{
public:
    using Info      = TcpHost;
    using Socket    = boost::asio::ip::tcp::socket;
    using SocketPtr = std::unique_ptr<Socket>;

    template <typename TExecutorOrStrand>
    TcpOpener(TExecutorOrStrand&& exec, Info info)
        : strand_(std::forward<TExecutorOrStrand>(exec)),
          info_(std::move(info))
    {}

    IoStrand strand() {return strand_;}

    template <typename F>
    void establish(F&& callback)
    {
        struct Resolved
        {
            TcpOpener* self;
            typename std::decay<F>::type callback;

            void operator()(AsioErrorCode ec, tcp::resolver::iterator iterator)
            {
                if (ec)
                {
                    self->resolver_.reset();
                    callback(ec, nullptr);
                }
                else
                    self->connect(iterator, std::move(callback));
            }
        };

        assert(!resolver_ && "Connect already in progress");

        resolver_.reset(new tcp::resolver(strand_));

        tcp::resolver::query query(info_.hostName(), info_.serviceName());

        // AsioConnector will keep this object alive until completion.
        resolver_->async_resolve(query,
                                 Resolved{this, std::forward<F>(callback)});
    }

    void cancel()
    {
        if (resolver_)
            resolver_->cancel();
        if (socket_)
            socket_->close();
    }

private:
    using tcp = boost::asio::ip::tcp;

    template <typename F>
    void connect(tcp::resolver::iterator iterator, F&& callback)
    {
        struct Connected
        {
            TcpOpener* self;
            typename std::decay<F>::type callback;

            void operator()(AsioErrorCode ec, tcp::resolver::iterator)
            {
                self->resolver_.reset();
                if (ec)
                    self->socket_.reset();
                callback(ec, std::move(self->socket_));
                self->socket_.reset();
            }
        };

        assert(!socket_);
        socket_.reset(new Socket(strand_));
        socket_->open(boost::asio::ip::tcp::v4());
        info_.options().applyTo(*socket_);

        // AsioConnector will keep this object alive until completion.
        boost::asio::async_connect(*socket_, iterator,
                                   Connected{this, std::forward<F>(callback)});
    }

    IoStrand strand_;
    Info info_;
    std::unique_ptr<tcp::resolver> resolver_;
    SocketPtr socket_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TCPOPENER_HPP
