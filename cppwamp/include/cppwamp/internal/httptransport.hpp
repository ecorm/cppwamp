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

    void onAccept(Timeout timeout, AdmitHandler handler) override
    {
    }

    void onShed(Timeout timeout, AdmitHandler handler) override
    {
    }

    void onCancelHandshake() override
    {
        assert(false && "Not a server transport");
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

    boost::asio::steady_timer timer_;
    RouterLogger::Ptr logger_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_HTTPTRANSPORT_HPP
