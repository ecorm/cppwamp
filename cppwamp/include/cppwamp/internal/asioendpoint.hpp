/*------------------------------------------------------------------------------
              Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ASIOENDPOINT_HPP
#define CPPWAMP_INTERNAL_ASIOENDPOINT_HPP

#include <cstdint>
#include <functional>
#include <utility>
#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include "../error.hpp"
#include "asiotransport.hpp"
#include "rawsockhandshake.hpp"

namespace wamp
{

// Forward declaration to mock objects used in testing
class FakeMsgTypeAsioConnector;
class FakeMsgTypeAsioListener;

namespace internal
{

//------------------------------------------------------------------------------
template <typename TEstablisher,
          template<typename> class TTransport = AsioTransport>
class AsioEndpoint
{
public:
    using Establisher  = TEstablisher;
    using Socket       = typename Establisher::Socket;
    using Transport    = TTransport<Socket>;
    using TransportPtr = std::shared_ptr<Transport>;
    using Handler      = std::function<void (std::error_code, int codecId,
                                             TransportPtr)>;

    explicit AsioEndpoint(Establisher&& est)
        : executor_(est.executor()),
          est_(std::move(est)),
          handshake_(0)
    {}

    void establish(Handler&& handler)
    {
        handler_ = std::move(handler);
        try
        {
            // The Connector that initiated 'establish' must keep this object
            // alive until completion.
            est_.establish( [this](AsioErrorCode ec, SocketPtr&& socket)
            {
                if (check(ec))
                {
                    socket_ = std::move(socket);
                    onEstablished();
                }
            });
        }
        catch (const boost::system::system_error& e)
        {
            check(e.code());
        }
    }

    void cancel()
    {
        if (socket_)
            socket_->close();
        else
            est_.cancel();
    }

protected:
    using Handshake = internal::RawsockHandshake;
    using SocketPtr = typename Establisher::SocketPtr;

    void sendHandshake(Handshake hs)
    {
        handshake_ = hs.toBigEndian();
        using boost::asio::buffer;
        using boost::asio::async_write;
        async_write(*socket_, buffer(&handshake_, sizeof(handshake_)),
            [this, hs](AsioErrorCode ec, size_t)
            {
                if (check(ec))
                    onHandshakeSent(hs);
            });
    }

    void receiveHandshake()
    {
        handshake_ = 0;
        using boost::asio::buffer;
        using boost::asio::async_read;
        async_read(*socket_, buffer(&handshake_, sizeof(handshake_)),
            [this](AsioErrorCode ec, size_t)
            {
                if (check(ec))
                    onHandshakeReceived(Handshake::fromBigEndian(handshake_));
            });
    }

    void complete(int codecId, size_t maxTxLength, size_t maxRxLength)
    {
        auto transport = Transport::create(std::move(socket_), maxTxLength,
                                    maxRxLength);
        std::error_code ec = make_error_code(TransportErrc::success);
        post(std::bind(handler_, ec, codecId, std::move(transport)));
        socket_.reset();
        handler_ = nullptr;
    }

    void fail(RawsockErrc errc)
    {
        socket_.reset();
        post(std::bind(handler_, make_error_code(errc), 0, nullptr));
        handler_ = nullptr;
    }

    bool check(AsioErrorCode asioEc)
    {
        if (asioEc)
        {
            auto ec = make_error_code(static_cast<std::errc>(asioEc.value()));
            socket_.reset();
            post(std::bind(handler_, ec, 0, nullptr));
            handler_ = nullptr;
        }
        return !asioEc;
    }

    virtual void onEstablished() = 0;

    virtual void onHandshakeReceived(Handshake hs) = 0;

    virtual void onHandshakeSent(Handshake hs) = 0;

private:
    template <typename TFunction>
    void post(TFunction&& fn)
    {
        boost::asio::post(executor_, std::forward<TFunction>(fn));
    }

    AnyExecutor executor_;
    SocketPtr socket_;
    Handler handler_;
    Establisher est_;
    uint32_t handshake_;

    // Grant friendship to mock objects used in testing
    friend class wamp::FakeMsgTypeAsioConnector;
    friend class wamp::FakeMsgTypeAsioListener;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ASIOENDPOINT_HPP
