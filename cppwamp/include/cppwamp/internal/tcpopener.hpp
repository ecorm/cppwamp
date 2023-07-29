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
#include <type_traits>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include "../asiodefs.hpp"
#include "../transports/tcphost.hpp"
#include "tcptraits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class TcpOpener
{
public:
    using Settings  = TcpHost;
    using Socket    = boost::asio::ip::tcp::socket;
    using SocketPtr = std::unique_ptr<Socket>;
    using Traits    = TcpTraits;

    template <typename TExecutorOrStrand>
    TcpOpener(TExecutorOrStrand&& exec, Settings s)
        : strand_(std::forward<TExecutorOrStrand>(exec)),
          settings_(std::move(s)),
          resolver_(strand_)
    {}

    template <typename F>
    void establish(F&& callback)
    {
        struct Resolved
        {
            TcpOpener* self;
            typename std::decay<F>::type callback;

            void operator()(boost::system::error_code asioEc,
                            tcp::resolver::iterator iterator)
            {
                if (self->checkError(asioEc, callback))
                    self->connect(iterator, std::move(callback));
            }
        };

        // RawsockConnector will keep this object alive until completion.
        using Query = boost::asio::ip::tcp::resolver::query;
        const Query query{settings_.hostName(), settings_.serviceName()};
        resolver_.async_resolve(
            query, Resolved{this, std::forward<F>(callback)});
    }

    void cancel()
    {
        resolver_.cancel();
        if (socket_)
            socket_->close();
    }

    const Settings& settings() const {return settings_;}

private:
    using tcp = boost::asio::ip::tcp;

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

    template <typename F>
    void connect(tcp::resolver::iterator iterator, F&& callback)
    {
        struct Connected
        {
            TcpOpener* self;
            typename std::decay<F>::type callback;

            void operator()(boost::system::error_code asioEc,
                            const tcp::resolver::iterator&)
            {
                SocketPtr socket{std::move(self->socket_)};
                self->socket_.reset();
                if (self->checkError(asioEc, callback))
                    callback(std::move(socket));
            }
        };

        assert(!socket_);
        socket_ = SocketPtr{new Socket(strand_)};
        socket_->open(boost::asio::ip::tcp::v4());
        settings_.options().applyTo(*socket_);

        // RawsockConnector will keep this object alive until completion.
        boost::asio::async_connect(*socket_, iterator,
                                   Connected{this, std::forward<F>(callback)});
    }

    IoStrand strand_;
    Settings settings_;
    boost::asio::ip::tcp::resolver resolver_;
    SocketPtr socket_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TCPOPENER_HPP
