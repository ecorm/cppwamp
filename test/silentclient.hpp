/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TEST_SILENTCLIENT_HPP
#define CPPWAMP_TEST_SILENTCLIENT_HPP

#include <cstdint>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace test
{

//------------------------------------------------------------------------------
// TCP client that connects but never writes anything
//------------------------------------------------------------------------------
class SilentClient
{
public:
    using ErrorCode = boost::system::error_code;

    SilentClient(boost::asio::io_context& io) : socket_(io), resolver_(io) {}

    void run(uint16_t port)
    {
        resolver_.async_resolve(
            "localhost",
            std::to_string(port),
            [this](ErrorCode ec, Tcp::resolver::results_type eps)
            {
                if (ec)
                    throw boost::system::system_error{ec};
                onResolved(eps);
            });
    }

    ErrorCode readError() const {return readError_;}

private:
    using Tcp = boost::asio::ip::tcp;

    void onResolved(boost::asio::ip::tcp::resolver::results_type eps)
    {
        boost::asio::async_connect(
            socket_,
            eps,
            [this](ErrorCode ec, Tcp::endpoint)
            {
                if (ec)
                    throw boost::system::system_error{ec};
                onConnected();
            });
    }

    void onConnected()
    {
        boost::asio::async_read(
            socket_,
            boost::asio::buffer(&bytes_, sizeof(bytes_)),
            [this](ErrorCode ec, std::size_t)
            {
                readError_ = ec;
            });
    }

    Tcp::socket socket_;
    Tcp::resolver resolver_;
    uint32_t bytes_;
    ErrorCode readError_;
};

} // namespace test

#endif // CPPWAMP_TEST_MOCKCLIENT_HPP
