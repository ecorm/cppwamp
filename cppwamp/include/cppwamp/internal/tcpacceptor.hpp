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
#include "../tcpendpoint.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class TcpAcceptor
{
public:
    using Settings  = TcpEndpoint;
    using Socket    = boost::asio::ip::tcp::socket;
    using SocketPtr = std::unique_ptr<Socket>;

    template <typename TExecutorOrStrand>
    TcpAcceptor(TExecutorOrStrand&& exec, const Settings& s)
        : strand_(std::forward<TExecutorOrStrand>(exec)),
          acceptor_(strand_, makeEndpoint(s))
    {}

    template <typename F>
    void establish(F&& callback)
    {
        struct Accepted
        {
            TcpAcceptor* self;
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

        socket_.reset(new Socket(strand_));

        // RawsockListener will keep this object alive until completion.
        acceptor_.async_accept(*socket_,
                               Accepted{this, std::forward<F>(callback)});
    }

    void cancel()
    {
        acceptor_.cancel();
    }

private:
    static boost::asio::ip::tcp::endpoint makeEndpoint(const Settings& s)
    {
        if (!s.address().empty())
            return {boost::asio::ip::make_address(s.address()), s.port()};
        else
            return {boost::asio::ip::tcp::v4(), s.port()};
    }

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
    boost::asio::ip::tcp::acceptor acceptor_;
    SocketPtr socket_;
};

} // namespace internal

} // namespace wamp


#endif // CPPWAMP_INTERNAL_TCPACCEPTOR_HPP
