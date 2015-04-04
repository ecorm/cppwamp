/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TCPACCEPTOR_HPP
#define CPPWAMP_INTERNAL_TCPACCEPTOR_HPP

#include <cassert>
#include <memory>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include "../asiodefs.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class TcpAcceptor
{
public:
    using Socket        = boost::asio::ip::tcp::socket;
    using SocketPtr     = std::unique_ptr<Socket>;

    TcpAcceptor(AsioService& iosvc, const std::string addr, unsigned short port)
        : iosvc_(iosvc),
          endpoint_(boost::asio::ip::address::from_string(addr), port)
    {}

    TcpAcceptor(AsioService& iosvc, unsigned short port)
        : iosvc_(iosvc),
          endpoint_(boost::asio::ip::tcp::v4(), port)
    {}

    AsioService& iosvc() {return iosvc_;}

    template <typename TCallback>
    void establish(TCallback&& callback)
    {
        assert(!socket_ && "Accept already in progress");

        if (!acceptor_)
            acceptor_.reset(new tcp::acceptor(iosvc_, endpoint_));
        socket_.reset(new Socket(iosvc_));

        // AsioListener will keep this object alive until completion.
        acceptor_->async_accept(*socket_, [this, callback](AsioErrorCode ec)
        {
            if (ec)
            {
                acceptor_.reset();
                socket_.reset();
            }
            callback(ec, std::move(socket_));
        });
    }

    void cancel()
    {
        if (acceptor_)
            acceptor_->close();
    }

private:
    using tcp = boost::asio::ip::tcp;

    AsioService& iosvc_;
    tcp::endpoint endpoint_;
    std::unique_ptr<tcp::acceptor> acceptor_;
    SocketPtr socket_;
};

} // namespace internal

} // namespace wamp


#endif // CPPWAMP_INTERNAL_TCPACCEPTOR_HPP
