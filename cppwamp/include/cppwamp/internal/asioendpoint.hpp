/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ASIOENDPOINT_HPP
#define CPPWAMP_INTERNAL_ASIOENDPOINT_HPP

#include <cstdint>
#include <functional>
#include <utility>
#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/write.hpp>
#include "../erroror.hpp"
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
    using Establisher     = TEstablisher;
    using Socket          = typename Establisher::Socket;
    using Transport       = TTransport<Socket>;
    using TransportingPtr = typename Transporting::Ptr;
    using Handler         = std::function<void (ErrorOr<TransportingPtr>)>;

    explicit AsioEndpoint(Establisher&& est)
        : strand_(est.strand()),
          est_(std::move(est)),
          handshake_(0)
    {}

    virtual ~AsioEndpoint() {}

    void establish(Handler&& handler)
    {
        handler_ = std::move(handler);
        try
        {
            // The Connector that initiated 'establish' must keep this object
            // alive until completion.
            est_.establish( [this](ErrorOr<SocketPtr> socket)
            {
                if (socket.has_value())
                {
                    socket_ = std::move(*socket);
                    onEstablished();
                }
                else
                {
                    auto ec = socket.error();
                    if (ec == std::errc::operation_canceled)
                        ec = make_error_code(TransportErrc::aborted);
                    postHandler(makeUnexpected(ec));
                    handler_ = nullptr;
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

    void complete(TransportInfo info)
    {
        Transporting::Ptr transport{Transport::create(std::move(socket_),
                                                      info)};
        postHandler(std::move(transport));
        socket_.reset();
        handler_ = nullptr;
    }

    void fail(RawsockErrc errc)
    {
        socket_.reset();
        postHandler(makeUnexpectedError(errc));
        handler_ = nullptr;
    }

    bool check(AsioErrorCode asioEc)
    {
        if (asioEc)
        {
            socket_.reset();
            auto ec = make_error_code(static_cast<std::errc>(asioEc.value()));
            if (ec == std::errc::operation_canceled)
                ec = make_error_code(TransportErrc::aborted);
            postHandler(makeUnexpected(ec));
            handler_ = nullptr;
        }
        return !asioEc;
    }

    virtual void onEstablished() = 0;

    virtual void onHandshakeReceived(Handshake hs) = 0;

    virtual void onHandshakeSent(Handshake hs) = 0;

private:
    template <typename... TArgs>
    void postHandler(TArgs&&... args)
    {
        boost::asio::post(strand_, std::bind(std::move(handler_),
                                             std::forward<TArgs>(args)...));
    }

    IoStrand strand_;
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
