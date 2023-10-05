/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_HTTPTRANSPORT_HPP
#define CPPWAMP_INTERNAL_HTTPTRANSPORT_HPP

#include "../routerlogger.hpp"
#include "../transports/httpprotocol.hpp"
#include "websockettransport.hpp"

namespace wamp
{

namespace internal
{

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
               makeConnectionInfo(t, server)),
          timer_(Base::strand()),
          logger_(std::move(l))
    {}

private:
    using Base = Transporting;
    using WebsocketSocket = boost::beast::websocket::stream<TcpSocket>;

    // This data is only used once for accepting connections.
    struct Data
    {
        Data(TcpSocket&& t, SettingsPtr s, const CodecIdSet& c)
            : tcpSocket(std::move(t)),
              codecIds(c),
              settings(std::move(s))
        {
            std::string agent = settings->agent();
            if (agent.empty())
                agent = Version::agentString();
            response.base().set(boost::beast::http::field::server,
                                std::move(agent));
        }

        TcpSocket tcpSocket;
        CodecIdSet codecIds;
        SettingsPtr settings;
        AdmitHandler handler;
        boost::beast::flat_buffer buffer;
        boost::beast::http::request<boost::beast::http::string_body> request;
        boost::beast::http::response<boost::beast::http::string_body> response;
        std::unique_ptr<WebsocketSocket> websocket; // TODO: Use optional<T>
        int codecId = 0;
        bool isRefusing = false;
    };

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
        assert((data_ != nullptr) && "Admit already performed");

        data_->handler = std::move(handler);
        auto self = this->shared_from_this();

        if (timeoutIsDefinite(timeout))
        {
            timeoutAfter(
                timeout,
                [this, self](boost::system::error_code ec) {onTimeout(ec);});
        }

        boost::beast::http::async_read(
            data_->tcpSocket, data_->buffer, data_->request,
            [this, self] (const boost::beast::error_code& netEc, std::size_t)
            {
                if (check(netEc))
                    receiveRequest();
            });
    }

    void onShed(Timeout timeout, AdmitHandler handler) override
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

    template <typename F>
    void timeoutAfter(Timeout timeout, F&& action)
    {
        timer_.expires_from_now(timeout);
        timer_.async_wait(std::forward<F>(action));
    }

    void onTimeout(boost::system::error_code ec)
    {
        if (!ec)
            return fail(TransportErrc::timeout);
        if (ec != boost::asio::error::operation_aborted)
            fail(static_cast<std::error_code>(ec));
    }

    void receiveRequest()
    {
        if (!data_)
            return;

        // Check if we received a websocket upgrade request
        if (!boost::beast::websocket::is_upgrade(data_->request))
            return onWebsocketUpgrade();

    }

    void onWebsocketUpgrade()
    {

    }

    bool check(boost::system::error_code netEc)
    {
        if (netEc)
            fail(netErrorCodeToStandard(netEc));
        return !netEc;
    }

    void fail(std::error_code ec)
    {
        if (!data_)
            return;
        Base::post(std::move(data_->handler), makeUnexpected(ec));
        shutdown();
    }

    template <typename TErrc>
    void fail(TErrc errc)
    {
        fail(static_cast<std::error_code>(make_error_code(errc)));
    }

    void shutdown()
    {
        data_->tcpSocket.close();
        data_.reset();
        Base::shutdown();
    }

    boost::asio::steady_timer timer_;
    RouterLogger::Ptr logger_;
    std::unique_ptr<Data> data_; // Only used once for accepting connection.
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_HTTPTRANSPORT_HPP
