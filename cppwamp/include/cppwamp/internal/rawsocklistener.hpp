/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_RAWSOCKLISTENER_HPP
#define CPPWAMP_INTERNAL_RAWSOCKLISTENER_HPP

#include <cassert>
#include <cerrno>
#include <memory>
#include <set>
#include <utility>

#if defined(_WIN32) || defined(__CYGWIN__)
#include <Winsock2.h>
#endif

#include <boost/asio/socket_base.hpp>
#include "../asiodefs.hpp"
#include "../errorcodes.hpp"
#include "../erroror.hpp"
#include "../listener.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
/** Provides functions that help in classifying socket operation errors. */
//------------------------------------------------------------------------------
struct SocketErrorHelper
{
    static bool isAcceptCancellationError(boost::system::error_code ec)
    {
        return ec == std::errc::operation_canceled ||
               ec == TransportErrc::aborted;
    }

    static bool isAcceptTransientError(boost::system::error_code ec)
    {
        // Asio already takes care of EAGAIN, EWOULDBLOCK, ECONNABORTED,
        // EPROTO, and EINTR.
        namespace sys = boost::system;
#if defined(__linux__)
        return ec == std::errc::host_unreachable
            || ec == std::errc::operation_not_supported
            || ec == std::errc::timed_out
            || ec == sys::error_code{EHOSTDOWN, sys::system_category()};
#elif defined(_WIN32) || defined(__CYGWIN__)
        return ec == std::errc::connection_refused
            || ec == std::errc::connection_reset
            || ec == sys::error_code{WSATRY_AGAIN, sys::system_category()});
#else
        return false;
#endif
    }

    static bool isAcceptCongestionError(boost::system::error_code ec)
    {
        namespace sys = boost::system;
        return ec == std::errc::no_buffer_space
            || ec == std::errc::not_enough_memory
            || ec == std::errc::too_many_files_open
            || ec == std::errc::too_many_files_open_in_system
#if defined(__linux__)
            || ec == sys::error_code{ENOSR, sys::system_category()}
#endif
            ;
    }

    static bool isAcceptOutageError(boost::system::error_code ec)
    {
        namespace sys = boost::system;
#if defined(__linux__)
        return ec == std::errc::network_down
            || ec == std::errc::network_unreachable
            || ec == std::errc::no_protocol_option // "Protocol not available"
            || ec == std::errc::operation_not_permitted // Denied by firewall
            || ec == sys::error_code{ENONET, sys::system_category()};
#elif defined(_WIN32) || defined(__CYGWIN__)
        return ec == std::errc::network_down;
#else
        return false;
#endif
    }

    static bool isAcceptFatalError(boost::system::error_code ec)
    {
        return ec == boost::asio::error::already_open
            || ec == std::errc::bad_file_descriptor
            || ec == std::errc::not_a_socket
            || ec == std::errc::invalid_argument
#if !defined(__linux__)
            || ec == std::errc::operation_not_supported
#endif
#if defined(BSD) || defined(__APPLE__)
            || ec == std::errc::bad_address // EFAULT
#elif defined(_WIN32) || defined(__CYGWIN__)
            || ec == std::errc::bad_address // EFAULT
            || ec == std::errc::permission_denied
            || ec == sys::error_code{WSANOTINITIALISED, sys::system_category()}
#endif
            ;
    }

    static bool isReceiveFatalError(boost::system::error_code ec)
    {
        return ec == std::errc::bad_address // EFAULT
            || ec == std::errc::bad_file_descriptor
            || ec == std::errc::invalid_argument
            || ec == std::errc::message_size
            || ec == std::errc::not_a_socket
            || ec == std::errc::not_connected
            || ec == std::errc::operation_not_supported
#if defined(_WIN32) || defined(__CYGWIN__)
            || ec == sys::error_code{WSANOTINITIALISED, sys::system_category()}
#endif
            ;
    }

    static bool isSendFatalError(boost::system::error_code ec)
    {
        return isReceiveFatalError(ec)
            || ec == std::errc::already_connected
            || ec == std::errc::connection_already_in_progress
            || ec == std::errc::permission_denied;
    }
};

//------------------------------------------------------------------------------
template <typename TConfig>
class RawsockListener
    : public std::enable_shared_from_this<RawsockListener<TConfig>>
{
public:
    using Ptr      = std::shared_ptr<RawsockListener>;
    using Settings = typename TConfig::Settings;
    using Handler  = Listening::Handler;

    static Ptr create(AnyIoExecutor e, IoStrand i, Settings s, CodecIdSet c)
    {
        return Ptr(new RawsockListener(std::move(e), std::move(i), std::move(s),
                                       std::move(c)));
    }

    ~RawsockListener()
    {
        TConfig::onDestruction(settings_);
    }

    void observe(Handler handler) {handler_ = handler;}

    void establish()
    {
        assert(!establishing_ && "RawsockListener already establishing");
        establishing_ = true;
        if (!acceptor_.is_open() && !listen())
        {
            establishing_ = false;
            return;
        }

        auto self = this->shared_from_this();
        acceptor_.async_accept(
            boost::asio::make_strand(executor_),
            [self](boost::system::error_code netEc, Socket socket)
            {
                self->onAccept(netEc, std::move(socket));
            });
    }

    void cancel() {acceptor_.cancel();}

private:
    using NetProtocol = typename TConfig::NetProtocol;
    using Socket      = typename NetProtocol::socket;
    using Transport   = typename TConfig::Transport;

    static std::error_code convertNetError(boost::system::error_code netEc)
    {
        auto ec = static_cast<std::error_code>(netEc);
        if (netEc == std::errc::operation_canceled)
        {
            ec = make_error_code(TransportErrc::aborted);
        }
        else if (netEc == std::errc::connection_reset ||
                 netEc == boost::asio::error::eof)
        {
            ec = make_error_code(TransportErrc::disconnected);
        }
        return ec;
    }

    RawsockListener(AnyIoExecutor e, IoStrand i, Settings s, CodecIdSet c)
        : executor_(std::move(e)),
          codecIds_(std::move(c)),
          settings_(std::move(s)),
          acceptor_(std::move(i))
    {}

    bool listen()
    {
        TConfig::onFirstEstablish(settings_);

        auto endpoint = TConfig::makeEndpoint(settings_);
        boost::system::error_code ec;
        acceptor_.open(endpoint.protocol(), ec);
        if (ec)
            return fail(ec, "socket open");

        TConfig::setAcceptorOptions(acceptor_);
        acceptor_.bind(endpoint, ec);
        if (ec)
            return fail(ec, "socket bind");

        acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
        if (ec)
            return fail(ec, "socket listen");

        return true;
    }

    bool fail(boost::system::error_code ec, const char* op)
    {
        struct Posted
        {
            Ptr self;
            ListenResult result;

            void operator()()
            {
                if (self->handler_)
                    self->handler_(std::move(result));
            }
        };

        if (ec == std::errc::operation_canceled)
            ec = make_error_code(TransportErrc::aborted);

        auto self = this->shared_from_this();
        boost::asio::post(
            strand_,
            Posted{std::move(self),
                   ListenResult{ec, ListeningErrorCategory::fatal, op}});
        return false;
    }

    void onAccept(boost::system::error_code netEc, Socket socket)
    {
        establishing_ = false;

        if (!handler_)
            return;

        auto cat = TConfig::classifyAcceptError(netEc, false);
        if (cat != ListeningErrorCategory::success)
        {
            auto ec = static_cast<std::error_code>(netEc);
            handler_(ListenResult{ec, cat, "socket accept"});
            return;
        }

        settings_.options().applyTo(socket);
        handler_(ListenResult{Transport::create(std::move(socket), settings_,
                                                codecIds_)});
    }

    AnyIoExecutor executor_;
    IoStrand strand_;
    CodecIdSet codecIds_;
    Settings settings_;
    typename NetProtocol::acceptor acceptor_;
    Handler handler_;
    bool establishing_ = false;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_RAWSOCKLISTENER_HPP
