/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TCPACCEPTOR_HPP
#define CPPWAMP_INTERNAL_TCPACCEPTOR_HPP

#include <cassert>
#include <memory>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include "../asiodefs.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class TcpAcceptor
{
public:
    using Socket    = boost::asio::ip::tcp::socket;
    using SocketPtr = std::unique_ptr<Socket>;

    template <typename TExecutorOrStrand>
    TcpAcceptor(TExecutorOrStrand&& exec, const std::string addr,
                unsigned short port)
        : strand_(std::forward<TExecutorOrStrand>(exec)),
          endpoint_(boost::asio::ip::address::from_string(addr), port)
    {}

    template <typename TExecutorOrStrand>
    TcpAcceptor(TExecutorOrStrand&& exec, unsigned short port)
        : strand_(std::forward<TExecutorOrStrand>(exec)),
          endpoint_(boost::asio::ip::tcp::v4(), port)
    {}

    IoStrand strand() {return strand_;}

    template <typename F>
    void establish(F&& callback)
    {
        struct Accepted
        {
            TcpAcceptor* self;
            typename std::decay<F>::type callback;

            void operator()(AsioErrorCode ec)
            {
                if (ec)
                {
                    self->acceptor_.reset();
                    self->socket_.reset();
                }
                callback(ec, std::move(self->socket_));
            }
        };

        assert(!socket_ && "Accept already in progress");

        if (!acceptor_)
            acceptor_.reset(new tcp::acceptor(strand_, endpoint_));
        socket_.reset(new Socket(strand_));

        // AsioListener will keep this object alive until completion.
        acceptor_->async_accept(*socket_,
                                Accepted{this, std::forward<F>(callback)});
    }

    void cancel()
    {
        if (acceptor_)
            acceptor_->close();
    }

private:
    using tcp = boost::asio::ip::tcp;

    IoStrand strand_;
    tcp::endpoint endpoint_;
    std::unique_ptr<tcp::acceptor> acceptor_;
    SocketPtr socket_;
};

} // namespace internal

} // namespace wamp


#endif // CPPWAMP_INTERNAL_TCPACCEPTOR_HPP
