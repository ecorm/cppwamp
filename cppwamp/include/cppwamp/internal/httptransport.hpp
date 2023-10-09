/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_HTTPTRANSPORT_HPP
#define CPPWAMP_INTERNAL_HTTPTRANSPORT_HPP

#include <fstream>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/http/file_body.hpp>
#include "../routerlogger.hpp"
#include "../transports/httpprotocol.hpp"
#include "httpjob.hpp"
#include "websockettransport.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class HttpJobImpl : public HttpJob,
                    public std::enable_shared_from_this<HttpJobImpl>
{
public:
    using Ptr         = std::shared_ptr<HttpJobImpl>;
    using TcpSocket   = boost::asio::ip::tcp::socket;
    using Settings    = HttpEndpoint;
    using SettingsPtr = std::shared_ptr<Settings>;
    using Handler     = AnyCompletionHandler<void (ErrorOr<int> codecId)>;

    HttpJobImpl(TcpSocket&& t, SettingsPtr s, const CodecIdSet& c,
                const std::string& server, RouterLogger::Ptr l)
        : Base(std::move(s)),
          tcpSocket_(std::move(t)),
          timer_(tcpSocket_.get_executor()),
          codecIds_(c),
          server_(server),
          logger_(std::move(l))
    {}

    void process(bool isShedding, Timeout timeout, Handler handler)
    {
        isShedding_ = isShedding;
        timeout_ = timeout;
        handler_ = std::move(handler);
        start();
    }

    void cancel() {tcpSocket_.close();}

    const Transporting::Ptr& upgradedTransport() const
    {
        return upgradedTransport_;
    }

    // TODO: Remove if unused
    const TransportInfo& transportInfo() const {return transportInfo_;}

private:
    using Base = HttpJob;
    using Parser = boost::beast::http::request_parser<Body>;

    void start()
    {
        auto self = shared_from_this();

        if (timeoutIsDefinite(timeout_))
        {
            timer_.expires_after(timeout_);
            timer_.async_wait(
                [this, self](boost::system::error_code ec) {onTimeout(ec);});
        }

        parser_.emplace();
        parser_->header_limit(settings().headerLimit());
        parser_->body_limit(settings().bodyLimit());

        boost::beast::http::async_read(
            tcpSocket_, buffer_, *parser_,
            [this, self] (const boost::beast::error_code& netEc, std::size_t)
            {
                timer_.cancel();
                if (check(netEc))
                    onRequest();
            });
    }

    void onTimeout(boost::system::error_code ec)
    {
        if (!ec)
            return fail(TransportErrc::timeout);
        if (ec != boost::asio::error::operation_aborted)
            fail(static_cast<std::error_code>(ec));
    }

    void onRequest()
    {
        bool isUpgrade = parser_->upgrade();

        // Send an error response if the server connection limit
        // has been reached
        if (isShedding_)
        {
            return doBalk(HttpStatus::serviceUnavailable,
                          "Connection limit exceeded", isUpgrade, {});
        }

        auto target = parser_->get().target();
        auto* action = settings().findAction(target);
        if ((action == nullptr) || !action->is<HttpWebsocketUpgrade>())
            return doBalk(HttpStatus::notFound, {}, isUpgrade, {});

        Base::setRequest(parser_->release());
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

    void doRespond(AnyMessage response) override
    {
        bool keepAlive = response.keep_alive();
        auto self = shared_from_this();
        boost::beast::async_write(
            tcpSocket_, std::move(response),
            [this, self, keepAlive](boost::beast::error_code netEc, std::size_t)
            {
                if (!check(netEc))
                    return;

                if (keepAlive)
                    start();
                else
                    finish();
            });
    }

    void doUpgrade(Transporting::Ptr transport, int codecId) override
    {
        upgradedTransport_ = std::move(transport);
        finish(codecId);
    }

    void doBalk(HttpStatus status, std::string what, bool simple,
                FieldList fields) override
    {
        // Don't send full HTML error page if request was a Websocket upgrade
        if (simple)
            return sendSimpleError(status, std::move(what), fields);

        namespace http = boost::beast::http;

        auto page = settings().findErrorPage(status);

        if (page == nullptr)
            return sendGeneratedError(status, std::move(what), fields);

        if (page->uri.empty())
            return sendGeneratedError(page->status, std::move(what), fields);

        if (page->isRedirect())
            return redirectError(page->status, page->uri, fields);

        return sendErrorFromFile(page->status, std::move(what), page->uri,
                                 fields);
    }

    void sendSimpleError(HttpStatus status, std::string&& what,
                         FieldList fields)
    {
        namespace http = boost::beast::http;

        http::response<http::string_body> response{
            static_cast<http::status>(status), request().version(),
            std::move(what)};

        for (auto pair: fields)
            response.set(pair.first, pair.second);

        response.prepare_payload();
        sendAndFinish(std::move(response));
    }

    void sendGeneratedError(HttpStatus status, std::string&& what,
                            FieldList fields)
    {
        namespace http = boost::beast::http;

        http::response<http::string_body> response{
            static_cast<http::status>(status), request().version(),
            generateErrorPage(status, what)};

        response.set(Field::server, settings().agent());
        response.set(Field::content_type, "text/html");
        for (auto pair: fields)
            response.set(pair.first, pair.second);

        response.prepare_payload();
        sendAndFinish(std::move(response));
    }

    std::string generateErrorPage(wamp::HttpStatus status,
                                  const std::string& what) const
    {
        auto errorMessage = make_error_code(status).message();
        std::string body{
            "<!DOCTYPE html><html>\n"
            "<head><title>" + errorMessage + "</title></head>\n"
            "<body>\n"
            "<h1>" + errorMessage + "</h1>\n"};

        if (!what.empty())
            body += "<p>" + what + "</p>";

        body += "<hr>\n" +
                settings().agent() +
                "</body>"
                "</html>";

        return body;
    }

    void redirectError(HttpStatus status, const std::string& where,
                       FieldList fields)
    {
        namespace http = boost::beast::http;

        http::response<http::empty_body> response{
            static_cast<http::status>(status), request().version()};

        response.set(Field::server, settings().agent());
        response.set(Field::location, where);
        for (auto pair: fields)
            response.set(pair.first, pair.second);

        response.prepare_payload();
        sendAndFinish(std::move(response));
    }

    void sendErrorFromFile(HttpStatus status, std::string&& what,
                           std::string path, FieldList fields)
    {
        namespace beast = boost::beast;
        namespace http = beast::http;

        beast::error_code ec;
        http::file_body::value_type body;
        path = httpStaticFilePath(settings().documentRoot(), path);
        body.open(path.c_str(), beast::file_mode::scan, ec);

        if (ec)
        {
            // TODO: Log problem
            return sendGeneratedError(status, std::move(what), fields);
        }

        http::response<http::file_body> response{
            http::status::ok, request().version(), std::move(body)};
        response.set(Field::server, settings().agent());
        response.set(Field::content_type, "text/html");
        for (auto pair: fields)
            response.set(pair.first, pair.second);
        response.prepare_payload();
        sendAndFinish(std::move(response));
    }

    void sendAndFinish(boost::beast::http::message_generator&& response)
    {
        auto self = shared_from_this();
        boost::beast::async_write(
            tcpSocket_, std::move(response),
            [this, self](boost::beast::error_code netEc, std::size_t)
            {
                if (check(netEc))
                    finish();
            });
    }

    void finish(int codecId = 0)
    {
        if (handler_)
            handler_(codecId);
        handler_ = nullptr;
        tcpSocket_.close();
    }

    TcpSocket tcpSocket_;
    boost::asio::steady_timer timer_;
    CodecIdSet codecIds_;
    TransportInfo transportInfo_;
    Transporting::Ptr upgradedTransport_;
    Handler handler_;
    boost::beast::flat_buffer buffer_;
    boost::optional<Parser> parser_;
    std::string server_;
    RouterLogger::Ptr logger_;
    Timeout timeout_;
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
               makeConnectionInfo(t, server)),
          job_(std::make_shared<HttpJobImpl>(std::move(t), std::move(s), c,
                                             server, std::move(l)))
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
        assert((job_ != nullptr) && "HTTP job already performed");

        struct Processed
        {
            AdmitHandler handler;
            Ptr self;

            void operator()(ErrorOr<int> codecId)
            {
                self->onJobProcessed(codecId, handler);
            }
        };

        auto self = std::dynamic_pointer_cast<HttpServerTransport>(
            this->shared_from_this());

        bool isShedding = Base::state() == TransportState::shedding;
        job_->process(isShedding,
                      timeout,
                      Processed{std::move(handler), std::move(self)});
    }

    void onCancelAdmission() override
    {
        if (job_)
            job_->cancel();
    }

    void onJobProcessed(ErrorOr<int>& codecId, AdmitHandler& handler)
    {
        if (!codecId.has_value())
        {
            job_.reset();
            handler(makeUnexpected(codecId.error()));
            return;
        }

        if (*codecId != 0)
            transport_ = job_->upgradedTransport();
        job_.reset();
        handler(*codecId);
    }

    void onStart(RxHandler rxHandler, TxErrorHandler txErrorHandler) override
    {
        assert(transport_ != nullptr);
        transport_->onStart(std::move(rxHandler),
                            std::move(txErrorHandler));
    }

    void onSend(MessageBuffer message) override
    {
        assert(transport_ != nullptr);
        transport_->onSend(std::move(message));
    }

    void onSetAbortTimeout(Timeout timeout) override
    {
        assert(transport_ != nullptr);
        transport_->onSetAbortTimeout(timeout);
    }

    void onSendAbort(MessageBuffer message) override
    {
        assert(transport_ != nullptr);
        transport_->onSendAbort(std::move(message));
    }

    void onStop() override
    {
        assert(transport_ != nullptr);
        transport_->onStop();
    }

    void onClose(CloseHandler handler) override
    {
        assert(transport_ != nullptr);
        transport_->onClose(std::move(handler));
    }

    HttpJobImpl::Ptr job_;
    Transporting::Ptr transport_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_HTTPTRANSPORT_HPP
