/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_WEBSOCKETLISTENER_HPP
#define CPPWAMP_INTERNAL_WEBSOCKETLISTENER_HPP

#include <memory>
#include <set>
#include "../asiodefs.hpp"
#include "../transports/websocketendpoint.hpp"
#include "websockettransport.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class WebsocketListener : public std::enable_shared_from_this<WebsocketListener>
{
public:
    using Ptr       = std::shared_ptr<WebsocketListener>;
    using Settings  = WebsocketEndpoint;
    using CodecIds  = std::set<int>;
    using Handler   = std::function<void (ErrorOr<Transporting::Ptr>)>;
    using Socket    = WebsocketTransport::Socket;
    using SocketPtr = std::unique_ptr<Socket>;
    using Transport = WebsocketTransport;

    static Ptr create(IoStrand i, Settings s, CodecIds codecIds)
    {
        return Ptr(new WebsocketListener(std::move(i), std::move(s),
                                         std::move(codecIds)));
    }

    void establish(Handler&& handler)
    {
        assert(!handler_ &&
               "WebsocketListener establishment already in progress");
        handler_ = std::move(handler);
        auto self = this->shared_from_this();
        // TODO
    }

    void cancel()
    {
        if (socket_)
            socket_->close(boost::beast::websocket::going_away);
        else
            ; // TODO: Cancel listening
    }

private:
    WebsocketListener(IoStrand i, Settings s, CodecIds codecIds)
        : codecIds_(std::move(codecIds))
    {}

    bool check(boost::system::error_code asioEc)
    {
        if (asioEc)
        {
            socket_.reset();
            auto ec = static_cast<std::error_code>(asioEc);
            if (asioEc == boost::asio::error::operation_aborted)
                ec = make_error_code(TransportErrc::aborted);
            dispatchHandler(UnexpectedError(ec));
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

    CodecIds codecIds_;
    Handler handler_;
    SocketPtr socket_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_WEBSOCKETLISTENER_HPP
