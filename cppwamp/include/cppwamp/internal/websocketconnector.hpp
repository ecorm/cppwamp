/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_WEBSOCKETCONNECTOR_HPP
#define CPPWAMP_INTERNAL_WEBSOCKETCONNECTOR_HPP

#include <memory>
#include "../asiodefs.hpp"
#include "../transports/websockethost.hpp"
#include "websockettransport.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class WebsocketConnector
    : public std::enable_shared_from_this<WebsocketConnector>
{
public:
    using Ptr       = std::shared_ptr<WebsocketConnector>;
    using Settings  = WebsocketHost;
    using Socket    = WebsocketTransport::Socket;
    using Handler   = std::function<void (ErrorOr<Transporting::Ptr>)>;
    using SocketPtr = std::unique_ptr<Socket>;
    using Transport = WebsocketTransport;

    static Ptr create(IoStrand i, Settings s, int codecId)
    {
        return Ptr(new WebsocketConnector(std::move(i), std::move(s), codecId));
    }

    void establish(Handler&& handler)
    {
        assert(!handler_ &&
               "WebsocketConnector establishment already in progress");
        handler_ = std::move(handler);
        auto self = shared_from_this();
        // TODO
    }

    void cancel()
    {
        if (socket_)
            socket_->close(boost::beast::websocket::going_away);
        else
            ;// TODO: Cancel resolver
    }

private:
    WebsocketConnector(IoStrand i, Settings s, int codecId)
        : codecId_(codecId)
    {}

    bool check(boost::system::error_code asioEc)
    {
        if (asioEc)
        {
            socket_.reset();
            auto ec = static_cast<std::error_code>(asioEc);
            if (asioEc == boost::asio::error::operation_aborted)
                ec = make_error_code(TransportErrc::aborted);
            dispatchHandler(makeUnexpected(ec));
        }
        return !asioEc;
    }

    void fail(TransportErrc errc)
    {
        socket_.reset();
        dispatchHandler(makeUnexpectedError(errc));
    }

    template <typename TArg>
    void dispatchHandler(TArg&& arg)
    {
        const Handler handler(std::move(handler_));
        handler_ = nullptr;
        handler(std::forward<TArg>(arg));
    }

    SocketPtr socket_;
    Handler handler_;
    int codecId_ = 0;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WEBSOCKETCONNECTOR_HPP
