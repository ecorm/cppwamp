/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_RAWSOCKACCEPTOR_HPP
#define CPPWAMP_INTERNAL_RAWSOCKACCEPTOR_HPP

#include <cassert>
#include <memory>
#include <type_traits>
#include <system_error>
#include <utility>
#include <boost/asio/socket_base.hpp>
#include "../asiodefs.hpp"
#include "../listener.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TConfig>
class RawsockAcceptor
{
public:
    using Settings  = typename TConfig::Settings;
    using Socket    = typename TConfig::NetProtocol::socket;
    using SocketPtr = std::unique_ptr<Socket>;
    using Traits    = typename TConfig::Traits;

    struct Result
    {
        Result() = default;

        Result(SocketPtr s) : socket(std::move(s)) {}

        /** Constructor taking information on a failed listen attempt. */
        Result(std::error_code e, ListeningErrorCategory c, const char* op)
            : error(e),
              operation(op),
              category(c)
        {}

        SocketPtr socket;
        std::error_code error;
        const char* operation = nullptr;
        ListeningErrorCategory category = ListeningErrorCategory::fatal;
    };

    RawsockAcceptor(AnyIoExecutor exec, IoStrand strand, Settings s)
        : executor_(std::move(exec)),
          strand_(std::move(strand)),
          acceptor_(strand_),
          settings_(std::move(s))
    {}

    ~RawsockAcceptor()
    {
        TConfig::onDestruction(settings_);
    }

    // Callback is expected to have this signature: void (Result)
    template <typename F>
    void establish(F&& callback)
    {
        // TODO: reuse_address option?
        // TODO: max_listen_connections option?
        // treatUnexpectedErrorsAsFatal option?

        struct Accepted
        {
            RawsockAcceptor* self;
            typename std::decay<F>::type callback;

            void operator()(boost::system::error_code netEc)
            {
                self->onAccept(netEc, callback);
            }
        };

        assert(!socket_ && "Accept already in progress");
        if (!acceptor_.is_open() && !listen(callback))
            return;

        // RawsockListener will keep this RocksockAcceptor object alive until
        // completion.
        socket_ = SocketPtr{new Socket(boost::asio::make_strand(executor_))};
        acceptor_.async_accept(*socket_,
                               Accepted{this, std::forward<F>(callback)});
    }

    void cancel() {acceptor_.cancel();}

    const Settings& settings() const {return settings_;}

private:
    template <typename F>
    bool listen(F&& callback)
    {
        TConfig::onFirstEstablish(settings_);

        auto endpoint = TConfig::makeEndpoint(settings_);
        boost::system::error_code ec;
        acceptor_.open(endpoint.protocol(), ec);
        if (ec)
            return fail(callback, ec, "socket open");

        TConfig::setAcceptorOptions(acceptor_);
        acceptor_.bind(endpoint, ec);
        if (ec)
            return fail(callback, ec, "socket bind");

        acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
        if (ec)
            return fail(callback, ec, "socket listen");

        return true;
    }

    template <typename F>
    bool fail(F& callback, boost::system::error_code ec, const char* op)
    {
        struct Posted
        {
            typename std::decay<F>::type callback;
            Result result;
            void operator()() {callback(std::move(result));}
        };

        if (ec == std::errc::operation_canceled)
            ec = make_error_code(TransportErrc::aborted);

        boost::asio::post(
            strand_,
            Posted{std::move(callback),
                   Result{ec, ListeningErrorCategory::fatal, op}});
        return false;
    }

    template <typename F>
    void onAccept(boost::system::error_code netEc, F& callback)
    {
        SocketPtr socket{std::move(socket_)};
        socket_.reset();

        auto cat = TConfig::classifyAcceptError(netEc, false);
        if (cat != ListeningErrorCategory::success)
        {
            auto ec = static_cast<std::error_code>(netEc);
            callback(Result{ec, cat, "socket accept"});
            return;
        }

        settings_.options().applyTo(*socket);
        callback(Result{std::move(socket)});
    }

    AnyIoExecutor executor_;
    IoStrand strand_;
    typename TConfig::NetProtocol::acceptor acceptor_;
    Settings settings_;
    SocketPtr socket_; // TODO: Use optional and move to Transporting object
};

} // namespace internal

} // namespace wamp


#endif // CPPWAMP_INTERNAL_RAWSOCKACCEPTOR_HPP
