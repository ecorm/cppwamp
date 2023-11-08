/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_HTTPTRANSPORT_HPP
#define CPPWAMP_INTERNAL_HTTPTRANSPORT_HPP

#include <fstream>
#include <initializer_list>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core/string_type.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/file_body.hpp>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/filesystem/path.hpp>
#include "../routerlogger.hpp"
#include "../transports/httpprotocol.hpp"
#include "tcptraits.hpp"
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
    using Handler     = AnyCompletionHandler<void (AdmitResult)>;
    using Body        = boost::beast::http::string_body;
    using Request     = boost::beast::http::request<Body>;
    using AnyMessage  = boost::beast::http::message_generator;
    using Field       = boost::beast::http::field;
    using StringView  = boost::beast::string_view;
    using FieldList   = std::initializer_list<std::pair<Field, StringView>>;
    using Path        = boost::filesystem::path;

    HttpJob(TcpSocket&& t, SettingsPtr s, const CodecIdSet& c,
            ConnectionInfo i, RouterLogger::Ptr l)
        : tcpSocket_(std::move(t)),
          timer_(tcpSocket_.get_executor()),
          codecIds_(c),
          connectionInfo_(std::move(i)),
          settings_(std::move(s)),
          logger_(std::move(l))
    {}

    const std::string& route() const {return route_;}

    const Path& target() const {return target_;}

    const Request& request() const
    {
        assert(parser_.has_value());
        return parser_->get();
    }

    const HttpEndpoint& settings() const {return *settings_;}

    void respond(AnyMessage response)
    {
        bool keepAlive = response.keep_alive();
        auto self = shared_from_this();
        boost::beast::async_write(
            tcpSocket_, std::move(response),
            [this, self, keepAlive](boost::beast::error_code netEc, std::size_t)
            {
                if (!check(netEc, "socket write"))
                    return;

                if (keepAlive)
                    start();
                else
                    finish(AdmitResult::responded());
            });
    }

    void websocketUpgrade()
    {
        auto self = shared_from_this();
        // TODO: Add permessageDeflate options from action
        auto wsEndpoint =
            std::make_shared<WebsocketEndpoint>(settings_->toWebsocket());
        auto t = std::make_shared<WebsocketServerTransport>(
            std::move(tcpSocket_), std::move(wsEndpoint), codecIds_);
        upgradedTransport_ = std::move(t);
        assert(parser_.has_value());
        upgradedTransport_->upgrade(
            parser_->get(),
            [this, self](AdmitResult result) {finish(result);});
    }

    void balk(
        HttpStatus status, std::string what = {}, bool simple = false,
        FieldList fields = {}, AdmitResult result = AdmitResult::responded())
    {
        // Don't send full HTML error page if request was a Websocket upgrade
        if (simple)
            return sendSimpleError(status, std::move(what), fields, result);

        namespace http = boost::beast::http;

        auto page = settings().findErrorPage(status);

        if (page == nullptr)
            return sendGeneratedError(status, std::move(what), fields, result);

        if (page->uri.empty())
            return sendGeneratedError(page->status, std::move(what), fields,
                                      result);

        if (page->isRedirect())
            return redirectError(page->status, page->uri, fields, result);

        return sendErrorFromFile(page->status, std::move(what), page->uri,
                                 fields, result);
    }

    void report(AccessActionInfo action)
    {
        if (logger_)
            logger_->log(AccessLogEntry{connectionInfo_, {},
                                        std::move(action)});
    }

private:
    using Base = HttpJob;
    using Parser = boost::beast::http::request_parser<Body>;

    void process(bool isShedding, Handler handler)
    {
        isShedding_ = isShedding;
        handler_ = std::move(handler);
        start();
    }

    template <typename F>
    void shutdown(std::error_code /*reason*/, F&& callback)
    {
        boost::system::error_code netEc;
        tcpSocket_.shutdown(TcpSocket::shutdown_send, netEc);
        auto ec = static_cast<std::error_code>(netEc);
        postAny(tcpSocket_.get_executor(), std::forward<F>(callback), ec);
    }

    void close() {tcpSocket_.close();}

    const WebsocketServerTransport::Ptr& upgradedTransport() const
    {
        return upgradedTransport_;
    }

    void start()
    {
        auto self = shared_from_this();

        // TODO: Split header/body reading
        auto timeout = settings_->limits().bodyTimeout().min();
        if (timeoutIsDefinite(timeout))
        {
            timer_.expires_after(timeout);
            timer_.async_wait(
                [this, self](boost::system::error_code ec) {onTimeout(ec);});
        }

        parser_.emplace();
        const auto headerLimit = settings().limits().headerSize();
        const auto bodyLimit = settings().limits().bodySize();
        if (headerLimit != 0)
            parser_->header_limit(headerLimit);
        if (bodyLimit != 0)
            parser_->body_limit(bodyLimit);

        boost::beast::http::async_read(
            tcpSocket_, buffer_, *parser_,
            [this, self] (const boost::beast::error_code& netEc, std::size_t)
            {
                if (netEc == boost::beast::http::error::end_of_stream)
                    finish(AdmitResult::responded());
                timer_.cancel();
                if (check(netEc, "socket read"))
                    onRequest();
            });
    }

    void onTimeout(boost::system::error_code ec)
    {
        if (!ec)
        {
            return balk(HttpStatus::requestTimeout, {}, false, {},
                        AdmitResult::rejected(HttpStatus::requestTimeout));
        }

        if (ec != boost::asio::error::operation_aborted)
        {
            finish(AdmitResult::failed(static_cast<std::error_code>(ec),
                                       "timer wait"));
        }
    }

    void onRequest()
    {
        bool isUpgrade = parser_->upgrade();

        // Send an error response if the server connection limit
        // has been reached
        if (isShedding_)
        {
            return balk(HttpStatus::serviceUnavailable,
                        "Connection limit exceeded", isUpgrade, {});
        }

        if (!normalizeAndCheckTargetPath())
            return balk(HttpStatus::badRequest, "Invalid request-target.");

        // Lookup and execute the action associated with the normalized
        // target path.
        auto* action = settings_->findAction(target_.generic_string());
        if (action == nullptr)
            return balk(HttpStatus::notFound, {}, isUpgrade, {});
        action->execute(*this);
    }

    bool normalizeAndCheckTargetPath()
    {
        // Normalize the request target path
        auto target = parser_->get().target();
        target_ = Path{target.begin(), target.end()};
        target_ = target_.lexically_normal();

        // Normalize as per v4 if v3 is in effect
        if (target_.filename_is_dot())
        {
            target_.remove_filename();
            target_.concat("/");
        }

        // Normalized request target path must not contain a dot-dot that
        // refers to a parent path.
        for (const auto& elem: target_)
        {
            if (elem == "..")
                return false;
        }

        return true;
    }

    bool check(boost::system::error_code netEc, const char* operation)
    {
        if (netEc)
        {
            finish(AdmitResult::failed(websocketErrorCodeToStandard(netEc),
                                       operation));
        }
        return !netEc;
    }

    void sendSimpleError(HttpStatus status, std::string&& what,
                         FieldList fields, AdmitResult result)
    {
        namespace http = boost::beast::http;

        http::response<http::string_body> response{
            static_cast<http::status>(status), request().version(),
            std::move(what)};

        for (auto pair: fields)
            response.set(pair.first, pair.second);

        response.prepare_payload();
        sendAndFinish(std::move(response), result);
    }

    void sendGeneratedError(HttpStatus status, std::string&& what,
                            FieldList fields, AdmitResult result)
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
        sendAndFinish(std::move(response), result);
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
                       FieldList fields, AdmitResult result)
    {
        namespace http = boost::beast::http;

        http::response<http::empty_body> response{
            static_cast<http::status>(status), request().version()};

        response.set(Field::server, settings().agent());
        response.set(Field::location, where);
        for (auto pair: fields)
            response.set(pair.first, pair.second);

        response.prepare_payload();
        sendAndFinish(std::move(response), result);
    }

    void sendErrorFromFile(HttpStatus status, std::string&& what,
                           std::string path, FieldList fields,
                           AdmitResult result)
    {
        namespace beast = boost::beast;
        namespace http = beast::http;

        beast::error_code netEc;
        http::file_body::value_type body;
        boost::filesystem::path absolutePath{settings().documentRoot()};
        absolutePath /= path;
        body.open(absolutePath.c_str(), beast::file_mode::scan, netEc);

        if (netEc)
        {
            auto ec = static_cast<std::error_code>(netEc);
            return sendGeneratedError(
                status, std::move(what), fields,
                AdmitResult::failed(ec, "error file read"));
        }

        http::response<http::file_body> response{
            http::status::ok, request().version(), std::move(body)};
        response.set(Field::server, settings().agent());
        response.set(Field::content_type, "text/html");
        for (auto pair: fields)
            response.set(pair.first, pair.second);
        response.prepare_payload();
        sendAndFinish(std::move(response), result);
    }

    void sendAndFinish(boost::beast::http::message_generator&& response,
                       AdmitResult result)
    {
        auto self = shared_from_this();
        boost::beast::async_write(
            tcpSocket_, std::move(response),
            [this, self, result](boost::beast::error_code netEc, std::size_t)
            {
                if (check(netEc, "socket write"))
                    finish(result);
            });
    }

    void finish(AdmitResult result)
    {
        if (result.status() != AdmitStatus::wamp)
        {
            boost::system::error_code ec;
            tcpSocket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send,
                                ec);
            tcpSocket_.close();
        }

        if (handler_)
            handler_(result);
        handler_ = nullptr;
    }

    TcpSocket tcpSocket_;
    boost::asio::steady_timer timer_;
    CodecIdSet codecIds_;
    TransportInfo transportInfo_;
    boost::beast::flat_buffer buffer_;
    boost::optional<Parser> parser_;
    boost::filesystem::path target_;
    std::string route_;
    Handler handler_;
    ConnectionInfo connectionInfo_;
    WebsocketServerTransport::Ptr upgradedTransport_;
    SettingsPtr settings_;
    RouterLogger::Ptr logger_;
    bool isShedding_ = false;

    friend class HttpServerTransport;
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
                        RouterLogger::Ptr l)
        : Base(boost::asio::make_strand(t.get_executor()),
               makeConnectionInfo(t)),
          job_(std::make_shared<HttpJob>(std::move(t), std::move(s), c,
                                         Base::connectionInfo(), std::move(l)))
    {}

private:
    using Base = Transporting;
    using WebsocketSocket = boost::beast::websocket::stream<TcpSocket>;

    static ConnectionInfo makeConnectionInfo(const TcpSocket& socket)
    {
        return TcpTraits::connectionInfo(socket, "HTTP");
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

    void onAdmit(AdmitHandler handler) override
    {
        assert((job_ != nullptr) && "HTTP job already performed");

        struct Processed
        {
            AdmitHandler handler;
            Ptr self;

            void operator()(AdmitResult result)
            {
                self->onJobProcessed(result, handler);
            }
        };

        auto self = std::dynamic_pointer_cast<HttpServerTransport>(
            this->shared_from_this());

        bool isShedding = Base::state() == TransportState::shedding;
        job_->process(isShedding,
                      Processed{std::move(handler), std::move(self)});
    }

    void onStart(RxHandler r, TxErrorHandler t) override
    {
        assert(transport_ != nullptr);
        transport_->httpStart({}, std::move(r), std::move(t));
    }

    void onSend(MessageBuffer m) override
    {
        assert(transport_ != nullptr);
        transport_->httpSend({}, std::move(m));
    }

    void onAbort(MessageBuffer m, ShutdownHandler f) override
    {
        assert(transport_ != nullptr);
        transport_->httpAbort({}, std::move(m), std::move(f));
    }

    void onShutdown(std::error_code reason, ShutdownHandler f) override
    {
        if (job_ != nullptr)
            job_->shutdown(reason, std::move(f));
        else if (transport_ != nullptr)
            transport_->httpShutdown({}, reason, std::move(f));
    }

    void onClose() override
    {
        if (job_ != nullptr)
            job_->close();
        if (transport_ != nullptr)
            transport_->httpClose({});
    }

    void onJobProcessed(AdmitResult result, AdmitHandler& handler)
    {
        if (result.status() == AdmitStatus::wamp)
            transport_ = job_->upgradedTransport();
        job_.reset();
        handler(result);
    }

    HttpJob::Ptr job_;
    WebsocketServerTransport::Ptr transport_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_HTTPTRANSPORT_HPP
