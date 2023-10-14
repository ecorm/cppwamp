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
#include "../routerlogger.hpp"
#include "../transports/httpprotocol.hpp"
#include "tcptraits.hpp"
#include "websockettransport.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
inline std::string httpStaticFilePath(boost::beast::string_view base,
                                      boost::beast::string_view path)
{
    if (base.empty())
        return std::string(path);

#ifdef _WIN32
    constexpr char separator = '\\';
#else
    constexpr char separator = '/';
#endif

    std::string result{base};
    if (result.back() == separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());

#ifdef _WIN32
    for (auto& c: result)
    {
        if (c == '/')
            c = separator;
    }
#endif
    return result;
}

//------------------------------------------------------------------------------
class HttpJob // TODO: Consolidate with HttpJobImpl
{
public:
    using Body = boost::beast::http::string_body;
    using Request = boost::beast::http::request<Body>;
    using Field = boost::beast::http::field;
    using StringView = boost::beast::string_view;
    using AnyMessage = boost::beast::http::message_generator;
    using FieldList = std::initializer_list<std::pair<Field, StringView>>;

    virtual ~HttpJob() = default;

    const Request& request() const {return request_;}

    const HttpEndpoint& settings() const {return *settings_;}

    void respond(AnyMessage response);

    void upgrade(Transporting::Ptr transport, int codecId);

    void balk(
        HttpStatus status, std::string what = {}, bool simple = false,
        FieldList fields = {})
    {
        doBalk(status, std::move(what), simple, fields);
    }

    void log(LogEntry entry) {doLog(std::move(entry));}

    void report(AccessActionInfo action) {doReport(std::move(action));}

protected:
    using SettingsPtr = std::shared_ptr<HttpEndpoint>;

    HttpJob(SettingsPtr s) : settings_(std::move(s)) {}

    virtual void doRespond(AnyMessage response) = 0;

    virtual void doUpgrade(Transporting::Ptr transport, int codecId) = 0;

    virtual void doBalk(
        HttpStatus status, std::string what, bool simple,
        std::initializer_list<std::pair<Field, StringView>> fields) = 0;

    virtual void doLog(LogEntry entry);

    virtual void doReport(AccessActionInfo action);

    void setRequest(Request&& req) {request_ = std::move(req);}

private:
    Request request_;
    SettingsPtr settings_;
};

//------------------------------------------------------------------------------
class HttpJobImpl : public HttpJob,
                    public std::enable_shared_from_this<HttpJobImpl>
{
public:
    using Ptr         = std::shared_ptr<HttpJobImpl>;
    using TcpSocket   = boost::asio::ip::tcp::socket;
    using Settings    = HttpEndpoint;
    using SettingsPtr = std::shared_ptr<Settings>;
    using Handler     = AnyCompletionHandler<void (AdmitResult)>;

    HttpJobImpl(TcpSocket&& t, SettingsPtr s, const CodecIdSet& c,
                ConnectionInfo i, RouterLogger::Ptr l)
        : Base(std::move(s)),
          tcpSocket_(std::move(t)),
          timer_(tcpSocket_.get_executor()),
          codecIds_(c),
          connectionInfo_(std::move(i)),
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
                if (check(netEc, "socket read"))
                    onRequest();
            });
    }

    void onTimeout(boost::system::error_code ec)
    {
        if (!ec)
            return finish(AdmitResult::rejected(TransportErrc::timeout));

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
            return doBalk(HttpStatus::serviceUnavailable,
                          "Connection limit exceeded", isUpgrade, {});
        }

        auto target = parser_->get().target();
        auto* action = settings().findAction(target);
        if (action == nullptr)
            return doBalk(HttpStatus::notFound, {}, isUpgrade, {});

        Base::setRequest(parser_->release());
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

    void doRespond(AnyMessage response) override
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

    void doUpgrade(Transporting::Ptr transport, int codecId) override
    {
        upgradedTransport_ = std::move(transport);
        finish(AdmitResult::wamp(codecId));
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

    void doReport(AccessActionInfo action) override
    {
        if (logger_)
            logger_->log(AccessLogEntry{connectionInfo_, {},
                                        std::move(action)});
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
                if (check(netEc, "socket write"))
                    finish(AdmitResult::responded());
            });
    }

    void finish(AdmitResult result)
    {
        if (result.status() != AdmitStatus::wamp)
            tcpSocket_.close();

        if (handler_)
            handler_(result);
        handler_ = nullptr;
    }

    TcpSocket tcpSocket_;
    boost::asio::steady_timer timer_;
    CodecIdSet codecIds_;
    TransportInfo transportInfo_;
    Transporting::Ptr upgradedTransport_;
    Handler handler_;
    boost::beast::flat_buffer buffer_;
    boost::optional<Parser> parser_;
    ConnectionInfo connectionInfo_;
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
                        RouterLogger::Ptr l)
        : Base(boost::asio::make_strand(t.get_executor()),
               makeConnectionInfo(t)),
          job_(std::make_shared<HttpJobImpl>(std::move(t), std::move(s), c,
                                             Base::connectionInfo(),
                                             std::move(l)))
    {}

private:
    using Base = Transporting;
    using WebsocketSocket = boost::beast::websocket::stream<TcpSocket>;

    static ConnectionInfo makeConnectionInfo(const TcpSocket& socket)
    {
        return TcpTraits::connectionInfo(socket.remote_endpoint(), "HTTP");
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

            void operator()(AdmitResult result)
            {
                self->onJobProcessed(result, handler);
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

    void onJobProcessed(AdmitResult result, AdmitHandler& handler)
    {
        if (result.status() == AdmitStatus::wamp)
            transport_ = job_->upgradedTransport();
        job_.reset();
        handler(result);
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
