/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_HTTPTRANSPORT_HPP
#define CPPWAMP_INTERNAL_HTTPTRANSPORT_HPP

#include <boost/asio/steady_timer.hpp>
#include "../routerlogger.hpp"
#include "../transports/httpprotocol.hpp"
#include "websockettransport.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class HttpJob : public std::enable_shared_from_this<HttpJob>
{
public:
    using Ptr         = std::shared_ptr<HttpJob>;
    using TcpSocket   = boost::asio::ip::tcp::socket;
    using Settings    = HttpEndpoint;
    using SettingsPtr = std::shared_ptr<Settings>;
    using Handler     = AnyCompletionHandler<void (ErrorOr<AnyHttpAction>)>;

    HttpJob(TcpSocket&& t, SettingsPtr s, const CodecIdSet& c)
        : tcpSocket_(std::move(t)),
          timer_(tcpSocket_.get_executor()),
          codecIds_(c),
          settings_(std::move(s))
    {
        std::string agent = settings_->agent();
        if (agent.empty())
            agent = Version::agentString();
        response_.base().set(boost::beast::http::field::server,
                             std::move(agent));
    }

    void process(bool isShedding, Timeout timeout, Handler handler)
    {
        isShedding_ = isShedding;
        handler_ = std::move(handler);
        auto self = shared_from_this();

        if (timeoutIsDefinite(timeout))
        {
            timer_.expires_after(timeout);
            timer_.async_wait(
                [this, self](boost::system::error_code ec) {onTimeout(ec);});
        }

        requestParser_.emplace();
        // TODO: Set header/body limits

        boost::beast::http::async_read(
            tcpSocket_, buffer_, *requestParser_,
            [this, self] (const boost::beast::error_code& netEc, std::size_t)
            {
                if (check(netEc))
                    onRequest();
            });
    }

    void cancel() {tcpSocket_.close();}

    const TransportInfo& transportInfo() const {return transportInfo_;}

private:
    using HttpStatus = boost::beast::http::status;
    using Parser =
        boost::beast::http::request_parser<boost::beast::http::string_body>;

    void onTimeout(boost::system::error_code ec)
    {
        if (!ec)
            return fail(TransportErrc::timeout);
        if (ec != boost::asio::error::operation_aborted)
            fail(static_cast<std::error_code>(ec));
    }

    void onRequest()
    {
        // Check if we received a websocket upgrade request
        if (requestParser_->upgrade())
            return onWebsocketUpgrade();

        // Send an error response if the server connection limit
        // has been reached
        if (isShedding_)
        {
            return respondThenFail("Connection limit reached",
                                   HttpStatus::service_unavailable,
                                   TransportErrc::shedded);
        }

    }

    void onWebsocketUpgrade()
    {

    }

    void respondThenFail(std::string msg, HttpStatus status, TransportErrc errc)
    {
        namespace http = boost::beast::http;
        response_.result(status);
        response_.body() = std::move(msg);
        auto self = shared_from_this();
        http::async_write(
            tcpSocket_, response_,
            [this, self, errc](boost::beast::error_code netEc, std::size_t)
            {
                if (check(netEc))
                    fail(errc);
            });
    }

    bool check(boost::system::error_code netEc)
    {
        if (netEc)
            fail(websocketErrorCodeToStandard(netEc));
        return !netEc;
    }

    void fail(std::error_code ec)
    {
        if (!handler_)
            return;
        handler_(makeUnexpected(ec));
        handler_ = nullptr;
        tcpSocket_.close();
    }

    template <typename TErrc>
    void fail(TErrc errc)
    {
        fail(static_cast<std::error_code>(make_error_code(errc)));
    }

    TcpSocket tcpSocket_;
    boost::asio::steady_timer timer_;
    CodecIdSet codecIds_;
    TransportInfo transportInfo_;
    SettingsPtr settings_;
    Handler handler_;
    boost::beast::flat_buffer buffer_;
    boost::optional<Parser> requestParser_;
    boost::beast::http::response<boost::beast::http::string_body> response_;
    bool isShedding_ = false;
};

//------------------------------------------------------------------------------
class HttpServerTransport : public Transporting
{
public:
    using Ptr = std::shared_ptr<HttpServerTransport>;
    using TcpSocket = boost::asio::ip::tcp::socket;
    using Settings = HttpEndpoint;
    using SettingsPtr = std::shared_ptr<HttpEndpoint>;

    HttpServerTransport(TcpSocket&& t, SettingsPtr s, const CodecIdSet& c,
                        const std::string& server, RouterLogger::Ptr l)
        : Base(boost::asio::make_strand(t.get_executor()),
               makeConnectionInfo(t, server))
    {}

private:
    using Base = Transporting;
    using WebsocketSocket = boost::beast::websocket::stream<TcpSocket>;

    // TODO: Consolidate with WebsocketTransport and RawsockTransport
    static ConnectionInfo makeConnectionInfo(const TcpSocket& socket,
                                             const std::string& server)
    {
        static constexpr unsigned ipv4VersionNo = 4;
        static constexpr unsigned ipv6VersionNo = 6;

        const auto& ep = socket.remote_endpoint();
        std::ostringstream oss;
        oss << ep;
        const auto addr = ep.address();
        const bool isIpv6 = addr.is_v6();

        Object details
        {
             {"address", addr.to_string()},
             {"ip_version", isIpv6 ? ipv6VersionNo : ipv4VersionNo},
             {"endpoint", oss.str()},
             {"port", ep.port()},
             {"protocol", "HTTP"},
         };

        if (!isIpv6)
        {
            details.emplace("numeric_address", addr.to_v4().to_uint());
        }

        return {std::move(details), oss.str(), server};
    }

    static std::error_code
    netErrorCodeToStandard(boost::system::error_code netEc)
    {
        if (!netEc)
            return {};

        namespace AE = boost::asio::error;
        bool disconnected = netEc == AE::broken_pipe ||
                            netEc == AE::connection_reset ||
                            netEc == AE::eof;
        auto ec = disconnected
                      ? make_error_code(TransportErrc::disconnected)
                      : static_cast<std::error_code>(netEc);

        if (netEc == AE::operation_aborted)
            ec = make_error_code(TransportErrc::aborted);

        return ec;
    }

    void onAdmit(Timeout timeout, AdmitHandler handler) override
    {
    }

    void onCancelAdmission() override
    {
    }

    void onStart(RxHandler rxHandler, TxErrorHandler txErrorHandler) override
    {
    }

    void onSend(MessageBuffer message) override
    {
    }

    void onSetAbortTimeout(Timeout timeout) override
    {
    }

    void onSendAbort(MessageBuffer message) override
    {
    }

    void onStop() override
    {
    }

    void onClose(CloseHandler handler) override
    {
    }
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_HTTPTRANSPORT_HPP
