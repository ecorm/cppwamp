/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_HTTPTRANSPORT_HPP
#define CPPWAMP_INTERNAL_HTTPTRANSPORT_HPP

#include <initializer_list>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core/string_type.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/file_body.hpp>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/url.hpp>
#include "../routerlogger.hpp"
#include "../transports/httpprotocol.hpp"
#include "httpurlvalidator.hpp"
#include "servertimeoutmonitor.hpp"
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
    using Field       = boost::beast::http::field;
    using StringView  = boost::beast::string_view;
    using FieldList   = std::initializer_list<std::pair<Field, StringView>>;
    using Url         = boost::urls::url;

    HttpJob(TcpSocket&& t, SettingsPtr s, const CodecIdSet& c,
            ConnectionInfo i, RouterLogger::Ptr l)
        : tcpSocket_(std::move(t)),
          codecIds_(c),
          connectionInfo_(std::move(i)),
          settings_(std::move(s)),
          logger_(std::move(l)),
          monitor_(std::make_shared<Monitor>(settings_))
    {}

    const Url& target() const {return target_;}

    const Request& request() const
    {
        assert(parser_.has_value());
        return parser_->get();
    }

    std::string fieldOr(Field field, std::string fallback)
    {
        const auto& req = request();
        auto iter = req.find(field);
        return (iter == req.end()) ? fallback : std::string{iter->value()};
    }

    const HttpEndpoint& settings() const {return *settings_;}

    template <typename R>
    void respond(R&& response, HttpStatus status = HttpStatus::ok)
    {
        sendResponse(std::forward<R>(response), status,
                     AdmitResult::responded());
    }

    void websocketUpgrade(WebsocketOptions options,
                          const WebsocketServerLimits limits)
    {
        auto self = shared_from_this();
        auto settings = settings_->toWebsocket(std::move(options), limits);
        auto wsEndpoint =
            std::make_shared<WebsocketEndpoint>(std::move(settings));
        auto t = std::make_shared<WebsocketServerTransport>(
            std::move(tcpSocket_), std::move(wsEndpoint), codecIds_);
        upgradedTransport_ = std::move(t);
        assert(parser_.has_value());
        upgradedTransport_->upgrade(
            parser_->get(),
            [this, self](AdmitResult result) {finish(result);});
    }

    void balk(HttpStatus status, std::string what = {}, bool simple = false,
              FieldList fields = {},
              AdmitResult result = AdmitResult::responded())
    {
        // Don't send full HTML error page if request was a Websocket upgrade
        if (simple)
            return sendSimpleError(status, std::move(what), fields, result);

        namespace http = boost::beast::http;

        auto page = settings().findErrorPage(status);
        bool found = page != nullptr;
        HttpStatus responseStatus = status;

        if (found)
        {
            if (page->isRedirect())
                return redirectError(*page, fields, result);

            if (page->generator() != nullptr)
                return sendCustomGeneratedError(*page, what, fields, result);

            if (!page->uri().empty())
                return sendErrorFromFile(*page, what, fields, result);

            responseStatus = page->status();
            // Fall through
        }

        sendGeneratedError(responseStatus, what, fields, result);
    }

private:
    using Base = HttpJob;
    using Parser = boost::beast::http::request_parser<Body>;
    using Verb = boost::beast::http::verb;
    using Monitor = internal::HttpServerTimeoutMonitor<Settings>;

    static std::error_code httpErrorCodeToStandard(
        boost::system::error_code netEc)
    {
        if (!netEc)
            return {};

        namespace AE = boost::asio::error;
        bool disconnected = netEc == AE::broken_pipe ||
                            netEc == AE::connection_reset ||
                            netEc == AE::eof;
        if (disconnected)
            return make_error_code(TransportErrc::disconnected);
        if (netEc == AE::operation_aborted)
            return make_error_code(TransportErrc::aborted);

        using HE = boost::beast::http::error;
        if (netEc == HE::end_of_stream || netEc == HE::partial_message)
            return make_error_code(TransportErrc::ended);
        if (netEc == HE::buffer_overflow || netEc == HE::body_limit)
            return make_error_code(TransportErrc::inboundTooLong);

        return static_cast<std::error_code>(netEc);
    }

    void process(bool isShedding, Handler handler)
    {
        reportConnection({AccessAction::clientConnect});
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
                    return finish(AdmitResult::responded());
                if (checkRead(netEc))
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

        if (!parseAndNormalizeTarget())
            return balk(HttpStatus::badRequest, "Invalid request-target");

        // Lookup and execute the action associated with the normalized
        // target path.
        auto* action = settings_->findAction(target_.path());
        if (action == nullptr)
            return balk(HttpStatus::notFound, {}, isUpgrade, {});
        action->execute({}, *this);
    }

    bool parseAndNormalizeTarget()
    {
        auto normalized =
            HttpUrlValidator::interpretAndNormalize(parser_->get().target(),
                                                    parser_->get().method());
        if (!normalized)
            return false;
        target_ = std::move(*normalized);
        return true;
    }

    bool checkRead(boost::system::error_code netEc)
    {
        if (!netEc)
            return true;

        if (isHttpParseErrorDueToClient(netEc))
        {
            finish(AdmitResult::rejected(httpErrorCodeToStandard(netEc)));
        }
        else
        {
            finish(AdmitResult::failed(httpErrorCodeToStandard(netEc),
                                       "socket read"));
        }
        return false;
    }

    bool checkWrite(boost::system::error_code netEc)
    {
        if (netEc)
        {
            finish(AdmitResult::failed(httpErrorCodeToStandard(netEc),
                                       "socket write"));
        }
        return !netEc;
    }

    void report(HttpStatus status)
    {
        if (!logger_)
            return;

        const auto& req = request();
        auto action = actionFromRequestVerb(req.base().method());
        auto statusStr = std::to_string(static_cast<unsigned>(status));
        AccessActionInfo info{action, req.target(), {}, std::move(statusStr)};

        if (action == AccessAction::clientHttpOther)
        {
            info.options.emplace("method",
                                 std::string{req.base().method_string()});
        }

        HttpAccessInfo httpInfo{fieldOr(Field::host, {}),
                                fieldOr(Field::user_agent, {})};
        logger_->log(AccessLogEntry{connectionInfo_, std::move(httpInfo),
                                    std::move(info)});
    }

    void reportConnection(AccessActionInfo info)
    {
        if (!logger_)
            return;

        logger_->log(AccessLogEntry{connectionInfo_, std::move(info)});
    }

    static AccessAction actionFromRequestVerb(Verb verb)
    {
        switch (verb)
        {
        case Verb::delete_: return AccessAction::clientHttpDelete;
        case Verb::get:     return AccessAction::clientHttpGet;
        case Verb::head:    return AccessAction::clientHttpHead;
        case Verb::post:    return AccessAction::clientHttpPost;
        case Verb::put:     return AccessAction::clientHttpPut;
        case Verb::connect: return AccessAction::clientHttpConnect;
        case Verb::options: return AccessAction::clientHttpOptions;
        case Verb::trace:   return AccessAction::clientHttpTrace;
        default: break;
        }

        return AccessAction::clientHttpOther;
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
        sendResponse(response, status, result);
    }

    void sendGeneratedError(HttpStatus status, const std::string& what,
                            FieldList fields, AdmitResult result)
    {
        namespace http = boost::beast::http;

        http::response<http::string_body> response{
            static_cast<http::status>(status), request().version(),
            generateErrorPage(status, what)};

        response.set(Field::content_type, "text/html; charset=utf-8");
        for (auto pair: fields)
            response.set(pair.first, pair.second);

        response.prepare_payload();
        sendResponse(std::move(response), status, result);
    }

    void sendCustomGeneratedError(const HttpErrorPage& page,
                                  const std::string& what,
                                  FieldList fields, AdmitResult result)
    {
        namespace http = boost::beast::http;

        http::response<http::string_body> response{
            static_cast<http::status>(page.status()), request().version(),
            page.generator()(page.status(), what)};

        std::string mime{"text/html; charset="};
        mime += page.charset().empty() ? std::string{"utf-8"} : page.charset();
        response.set(Field::content_type, std::move(mime));
        for (auto pair: fields)
            response.set(pair.first, pair.second);
        sendResponse(std::move(response), page.status(), result);
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

    void redirectError(const HttpErrorPage& page, FieldList fields,
                       AdmitResult result)
    {
        namespace http = boost::beast::http;

        http::response<http::empty_body> response{
            static_cast<http::status>(page.status()), request().version()};

        response.set(Field::location, page.uri());
        for (auto pair: fields)
            response.set(pair.first, pair.second);
        sendResponse(std::move(response), page.status(), result);
    }

    void sendErrorFromFile(const HttpErrorPage& page, const std::string& what,
                           FieldList fields, AdmitResult result)
    {
        namespace beast = boost::beast;
        namespace http = beast::http;

        beast::error_code netEc;
        http::file_body::value_type body;
        const auto& docRoot = settings().fileServingOptions().documentRoot();
        boost::filesystem::path absolutePath{docRoot};
        absolutePath /= page.uri();
        body.open(absolutePath.c_str(), beast::file_mode::scan, netEc);

        if (netEc)
        {
            auto ec = static_cast<std::error_code>(netEc);
            return sendGeneratedError(
                page.status(), what, fields,
                AdmitResult::failed(ec, "error file read"));
        }

        http::response<http::file_body> response{
            http::status::ok, request().version(), std::move(body)};

        std::string mime{"text/html; charset="};
        mime += page.charset().empty() ? std::string{"utf-8"} : page.charset();
        response.set(Field::content_type, std::move(mime));
        for (auto pair: fields)
            response.set(pair.first, pair.second);
        sendResponse(std::move(response), page.status(), result);
    }

    template <typename R>
    void sendResponse(R&& response, HttpStatus status, AdmitResult result)
    {
        bool keepAlive = request().keep_alive() &&
                         result.status() == AdmitStatus::responded;
        // Beast will adjust the Connection field automatically depending on
        // the HTTP version.
        // https://datatracker.ietf.org/doc/html/rfc7230#section-6.3
        response.keep_alive(keepAlive);

        response.set(boost::beast::http::field::server, settings_->agent());
        response.prepare_payload();

        // TODO: Response timeout
        // TODO: Keepalive timeout

        // This is needed to transfer ownership of the response to the
        // async_write operation.
        boost::beast::http::message_generator message{std::move(response)};

        auto self = shared_from_this();
        boost::beast::async_write(
            tcpSocket_, std::move(message),
            [this, self, keepAlive, status, result](
                                      boost::beast::error_code netEc,
                                      std::size_t)
            {
                if (!checkWrite(netEc))
                    return;

                report(status);

                if (keepAlive)
                    start();
                else
                    finish(result);
            });
    }

    void finish(AdmitResult result)
    {
        if (result.status() != AdmitStatus::wamp)
        {
            if (logger_)
            {
                using AA = AccessAction;
                if (result.status() == AdmitStatus::responded)
                    reportConnection({AA::clientDisconnect});
                else
                    reportConnection({AA::serverReject, result.error()});
            }

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
    CodecIdSet codecIds_;
    TransportInfo transportInfo_;
    boost::beast::flat_buffer buffer_;
    boost::optional<Parser> parser_;
    boost::urls::url target_;
    Handler handler_;
    ConnectionInfo connectionInfo_;
    WebsocketServerTransport::Ptr upgradedTransport_;
    SettingsPtr settings_;
    RouterLogger::Ptr logger_;
    std::shared_ptr<Monitor> monitor_;
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
