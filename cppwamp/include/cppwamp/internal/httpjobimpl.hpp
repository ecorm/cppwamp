/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023-2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_HTTPJOBIMPL_HPP
#define CPPWAMP_INTERNAL_HTTPJOBIMPL_HPP

#include <initializer_list>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/string_type.hpp>
#include <boost/beast/http/buffer_body.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/file_body.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/filesystem.hpp>
#include <boost/url.hpp>
#include "../erroror.hpp"
#include "../routerlogger.hpp"
#include "../transports/httpprotocol.hpp"
#include "../transports/httpresponse.hpp"
#include "../transports/httpserveroptions.hpp"
#include "../transports/websocketprotocol.hpp"
#include "httpserializer.hpp"
#include "servertimeoutmonitor.hpp"
#include "websockettransport.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class HttpJobImpl : public std::enable_shared_from_this<HttpJobImpl>
{
public:
    using Ptr             = std::shared_ptr<HttpJobImpl>;
    using TcpSocket       = boost::asio::ip::tcp::socket;
    using Settings        = HttpEndpoint;
    using SettingsPtr     = std::shared_ptr<Settings>;
    using AdmitHandler    = AnyCompletionHandler<void (AdmitResult)>;
    using ShutdownHandler = AnyCompletionHandler<void (std::error_code)>;
    using Header          = boost::beast::http::request_header<>;
    using Field           = boost::beast::http::field;
    using StringView      = boost::beast::string_view;
    using FieldList       = std::initializer_list<std::pair<Field, StringView>>;
    using Url             = boost::urls::url;
    using WebsocketServerTransportPtr =
        std::shared_ptr<internal::WebsocketServerTransport>;

    HttpJobImpl(TcpSocket&& t, SettingsPtr s, const CodecIdSet& c,
                ConnectionInfo i, RouterLogger::Ptr l)
        : tcpSocket_(std::move(t)),
          codecIds_(c),
          connectionInfo_(std::move(i)),
          settings_(std::move(s)),
          logger_(std::move(l))
    {}

    const Url& target() const {return target_;}

    std::string method() const
    {
        assert(parser_.has_value());
        return std::string{parser_->get().method_string()};
    }

    const std::string& body() const {return body_;}

    std::string& body() {return body_;}

    ErrorOr<std::string> field(const std::string& key) const
    {
        assert(parser_.has_value());
        const auto& hdr = parser_->get().base();
        auto iter = hdr.find(key);
        if (iter == hdr.end())
            return makeUnexpectedError(MiscErrc::absent);
        return std::string{iter->value()};
    }

    std::string fieldOr(const std::string& key, std::string fallback) const
    {
        assert(parser_.has_value());
        const auto& hdr = parser_->get().base();
        auto iter = hdr.find(key);
        return (iter == hdr.end()) ? fallback : std::string{iter->value()};
    }

    const std::string& hostName() const {return hostName_;}

    bool isUpgrade() const
    {
        assert(parser_.has_value());
        const auto& hdr = parser_->get().base();
        return hdr.find(boost::beast::http::field::upgrade) != hdr.end();
    }

    bool isWebsocketUpgrade() const
    {
        assert(parser_.has_value());
        return boost::beast::websocket::is_upgrade(parser_->get().base());
    }

    const HttpEndpoint& settings() const
    {
        return *settings_;
    }

    void continueRequest()
    {
        assert(parser_.has_value());

        HttpResponse response{HttpStatus::continueRequest};
        serializer_ = response.takeSerializer();
        serializer_->prepare(blockOptions().limits().responseIncrement(),
                             parser_->get().version(), blockOptions().agent(),
                             parser_->keep_alive());
        status_ = HttpStatus::continueRequest;
        monitor_.startResponse(steadyTime(),
                               blockOptions().timeouts().responseTimeout());
        sendMoreResponse();
    }

    void respond(HttpResponse&& response)
    {
        if (!responseSent_)
            sendResponse(response, AdmitResult::responded());
    }

    void websocketUpgrade(WebsocketOptions options,
                          const WebsocketServerLimits& limits)
    {
        if (responseSent_)
            return;

        auto self = shared_from_this();
        auto wsEndpoint = std::make_shared<WebsocketEndpoint>(settings_->address(),
                                                              settings_->port());
        wsEndpoint->withOptions(std::move(options)).withLimits(limits);
        auto t = std::make_shared<internal::WebsocketServerTransport>(
            std::move(tcpSocket_), std::move(wsEndpoint), codecIds_);
        upgradedTransport_ = std::move(t);
        assert(parser_.has_value());
        upgradedTransport_->upgrade(
            parser_->get(),
            [this, self](AdmitResult result) {finish(result);});
    }

    void deny(HttpDenial denial)
    {
        if (responseSent_)
            return;

        if (!denial.htmlEnabled())
            return sendSimpleError(denial);

        namespace http = boost::beast::http;

        auto page = blockOptions().findErrorPage(denial.status());
        const bool found = page != nullptr;
        auto responseStatus = denial.status();

        if (found)
        {
            if (page->isRedirect())
                return redirectError(denial, *page);

            if (page->generator() != nullptr)
                return sendCustomGeneratedError(denial, *page);

            if (!page->uri().empty())
                return sendErrorFromFile(denial, *page);

            responseStatus = page->status();
            // Fall through
        }

        denial.setStatus(responseStatus);
        sendGeneratedError(denial);
    }

    std::error_code monitor()
    {
        return monitor_.check(steadyTime());
    }

    void process(bool isShedding, AdmitHandler handler)
    {
        isShedding_ = isShedding;
        admitHandler_ = std::move(handler);
        start();
    }

    void shutdown(std::error_code reason, ShutdownHandler handler)
    {
        shutdownHandler_ = std::move(handler);

        if (responseSent_ || !reason)
        {
            if (admitHandler_)
            {
                post(std::move(admitHandler_), AdmitResult::cancelled(reason));
                admitHandler_ = nullptr;
            }

            doShutdown();
            return;
        }

        isPoisoned_ = true;
        auto what = errorCodeToUri(reason);
        what += ": ";
        what += reason.message();
        deny(HttpDenial{shutdownReasonToHttpStatus(reason)}
                 .withMessage(std::move(what))
                 .withResult(AdmitResult::cancelled(reason)));
    }

    void close()
    {
        monitor_.endLinger();
        tcpSocket_.close();
    }

    const WebsocketServerTransportPtr& upgradedTransport() const
    {
        return upgradedTransport_;
    }

private:
    using Body            = boost::beast::http::buffer_body;
    using Request         = boost::beast::http::request<Body>;
    using Parser          = boost::beast::http::request_parser<Body>;
    using Verb            = boost::beast::http::verb;
    using Monitor         = internal::HttpServerTimeoutMonitor;
    using TimePoint       = std::chrono::steady_clock::time_point;

    enum class RoutingStatus
    {
        ok,
        badHost,
        badTarget,
        badScheme,
        badPort,
        count
    };

    static TimePoint steadyTime()
    {
        return std::chrono::steady_clock::now();
    }

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
        if (netEc == HE::buffer_overflow ||
            netEc == HE::header_limit ||
            netEc == HE::body_limit)
        {
            return make_error_code(TransportErrc::inboundTooLong);
        }

        return static_cast<std::error_code>(netEc);
    }

    static HttpStatus shutdownReasonToHttpStatus(std::error_code ec)
    {
        if (ec == TransportErrc::timeout)
            return HttpStatus::requestTimeout;
        if (ec == WampErrc::systemShutdown || ec == WampErrc::sessionKilled)
            return HttpStatus::serviceUnavailable;
        return HttpStatus::internalServerError;
    }

    void doShutdown()
    {
        if (tcpSocket_.is_open())
        {
            boost::system::error_code netEc;
            tcpSocket_.shutdown(TcpSocket::shutdown_send, netEc);
            if (netEc)
            {
                auto ec = static_cast<std::error_code>(netEc);
                post(std::move(shutdownHandler_), ec);
                tcpSocket_.close();
            }
            else
            {
                monitor_.startLinger(steadyTime(),
                                     blockOptions().timeouts().lingerTimeout());
                if (!isReading_)
                    flush();
            }
        }
        else
        {
            post(std::move(shutdownHandler_), std::error_code{});
        }
        shutdownHandler_ = nullptr;
    }

    void flush()
    {
        bodyBuffer_.resize(+flushReadSize_);
        tcpSocket_.async_read_some(
            boost::asio::buffer(&bodyBuffer_.front(), +flushReadSize_),
            [this](boost::system::error_code netEc, size_t n)
            {
                if (!netEc)
                    return flush();
                onFlushComplete(netEc);
            });
    }

    void onFlushComplete(boost::system::error_code netEc)
    {
        monitor_.endLinger();
        tcpSocket_.close();

        bool isEof = netEc == boost::beast::http::error::end_of_stream ||
                     netEc == boost::beast::http::error::partial_message;
        if (isEof)
            netEc = {};

        if (shutdownHandler_ != nullptr)
        {
            shutdownHandler_(httpErrorCodeToStandard(netEc));
            shutdownHandler_ = nullptr;
        }
    }

    void start()
    {
        serverBlock_ = nullptr;
        status_ = HttpStatus::none;
        responseSent_ = false;
        expectationReceived_ = false;
        hostName_.clear();
        target_.clear();
        body_.clear();
        parser_.emplace();
        const auto& limits = settings().options().limits();
        parser_->header_limit(limits.requestHeaderSize());

        // Only set the body limit after we parse the hostname and determine
        // its associated server block.
        parser_->body_limit(boost::none);

        // After the first request, hold off arming the read timeout until data
        // is available to be read, as the keep-alive timeout is already in
        // effect.
        if (alreadyRequested_)
            waitForHeader();
        else
            readHeader();
    }

    void waitForHeader()
    {
        auto self = shared_from_this();
        tcpSocket_.async_wait(
            TcpSocket::wait_read,
            [this, self](boost::system::error_code netEc)
            {
                if (checkRead(netEc))
                    readHeader();
            });
    }

    void readHeader()
    {
        alreadyRequested_ = true;
        isReading_ = true;
        monitor_.startHeader(
            steadyTime(),
            settings_->options().timeouts().requestHeaderTimeout());

        auto self = shared_from_this();
        boost::beast::http::async_read_header(
            tcpSocket_, streamBuffer_, *parser_,
            [this, self] (boost::beast::error_code netEc, std::size_t)
            {
                isReading_ = false;
                monitor_.endHeader();
                if (checkRead(netEc))
                    onHeaderRead(netEc);
            });
    }

    void onHeaderRead(boost::beast::error_code netEc)
    {
        auto routingStatus = interpretRoutingInformation();

        // Find the server block associated with the interpreted hostname.
        if (routingStatus == RoutingStatus::ok)
            serverBlock_ = settings_->findBlock(hostName_);

        // If the request body exceeds the limit, mark the request as rejected
        // so that keep-alive is disabled and the connection is shut down
        // after sending the response. Otherwise, we would have to consume
        // the large request body until the parser inevitably overflows.
        const auto bodyLength = parser_->content_length().value_or(0);
        const auto bodyLimit = blockOptions().limits().requestBodySize();
        parser_->body_limit(bodyLimit);
        if (bodyLength > bodyLimit)
        {
            const auto s = HttpStatus::contentTooLarge;
            return deny(HttpDenial{s}.withResult(AdmitResult::rejected(s)));
        }

        // Send an error response and disconnect if the server connection limit
        // has been reached.
        if (isShedding_)
        {
            return deny(HttpDenial{HttpStatus::serviceUnavailable}
                            .withMessage("Connection limit exceeded")
                            .withResult(AdmitResult::shedded()));
        }

        // Send an error response if the routing information was invalid.
        if (routingStatus != RoutingStatus::ok)
            return sendRoutingError(routingStatus);

        // Send an error response if a matching server block was not found.
        if (serverBlock_ == nullptr)
        {
            return deny(HttpDenial{HttpStatus::badRequest}
                            .withMessage("Invalid hostname"));
        }

        // Check if a 100-continue expectation was received
        auto expectField = parser_->get().find(Field::expect);
        if (expectField != parser_->get().end())
            return onExpectationReceived(expectField->value());

        readBody();
    }

    void onExpectationReceived(boost::beast::string_view expectField)
    {
        if (!boost::beast::iequals(expectField, "100-continue"))
            return deny(HttpStatus::expectationFailed);

        // Ignore 100-continue expectations if it's an HTTP/1.0 request, or if
        // the request has no body.
        const auto bodyLength = parser_->content_length().value_or(0);
        if (parser_->get().version() < 11 || bodyLength == 0)
            return readBody();

        expectationReceived_ = true;

        // Lookup the action associated with the normalized target path,
        // and make it emit the expected status code.
        assert(serverBlock_ != nullptr);
        auto* action = serverBlock_->findAction(target_.path());
        if (action == nullptr)
            return deny(HttpStatus::notFound);
        HttpJob job{shared_from_this()};
        action->expect({}, job);
    }

    void readBody()
    {
        if (parser_->is_done())
        {
            onRequestRead();
        }
        else
        {
            monitor_.startBody(steadyTime(),
                               blockOptions().timeouts().requestBodyTimeout());
            readMoreBody();
        }
    }

    void readMoreBody()
    {
        bodyBuffer_.resize(blockOptions().limits().requestBodyIncrement());
        parser_->get().body().data = &bodyBuffer_.front();
        parser_->get().body().size = bodyBuffer_.size();
        isReading_ = true;

        auto self = shared_from_this();
        boost::beast::http::async_read(
            tcpSocket_, streamBuffer_, *parser_,
            [this, self] (boost::beast::error_code netEc, std::size_t)
            {
                isReading_ = false;

                if (netEc == boost::beast::http::error::need_buffer)
                    netEc = {};

                if (!checkRead(netEc))
                    return monitor_.endBody();

                assert(bodyBuffer_.size() >= parser_->get().body().size);
                auto bytesParsed = bodyBuffer_.size() -
                                   parser_->get().body().size;
                body_.append(&bodyBuffer_.front(), bytesParsed);

                if (parser_->is_done())
                    return onRequestRead();

                monitor_.updateBody(steadyTime(), bytesParsed);
                return readMoreBody();
            });
    }

    void onRequestRead()
    {
        monitor_.endBody();

        // If we already sent a response other than 100-continue, discard the
        // request.
        if (status_ != HttpStatus::none &&
            status_ != HttpStatus::continueRequest)
        {
            if (keepAlive_)
                start();
            else
                finish(result_);
            return;
        }

        // Lookup and execute the action associated with the normalized
        // target path.
        assert(serverBlock_ != nullptr);
        auto* action = serverBlock_->findAction(target_.path());
        if (action == nullptr)
            return deny(HttpStatus::notFound);
        HttpJob job{shared_from_this()};
        action->execute({}, job);
    }

    RoutingStatus interpretRoutingInformation()
    {
        auto hostField = parser_->get().find(Field::host);
        if (hostField == parser_->get().end())
            return RoutingStatus::badHost;

        auto result = boost::urls::parse_authority(hostField->value());
        if (!result.has_value() || result->has_userinfo())
        {
            // Save the invalid host name anyway so that it's logged.
            hostName_ = std::string{hostField->value()};
            return RoutingStatus::badHost;
        }

        hostName_ = std::string{result->host()};

        if (result->has_port() && result->port_number() != settings_->port())
            return RoutingStatus::badPort;

        auto normalized = internal::HttpUrlValidator::interpretAndNormalize(
            parser_->get().target(), parser_->get().method());
        if (!normalized)
            return RoutingStatus::badTarget;
        target_ = std::move(*normalized);

        /*  From RFC9112, subsection 3.2.2:
            "When an origin server receives a request with an absolute-form of
            request-target, the origin server MUST ignore the received Host
            header field (if any) and instead use the host information of the
            request-target. Note that if the request-target does not have an
            authority component, an empty Host header field will be sent in
            this case."

            From RFC9110, subsection 7.4:
            "Unless the connection is from a trusted gateway, an origin server
            MUST reject a request if any scheme-specific requirements for the
            target URI are not met. In particular, a request for an "https"
            resource MUST be rejected unless it has been received over a
            connection that has been secured via a certificate valid for that
            target URI's origin, as defined by Section 4.2.2." */
        if (target_.has_scheme())
        {
            if (target_.scheme_id() != boost::urls::scheme::http)
                return RoutingStatus::badScheme;

            if (target_.has_port() &&
                target_.port_number() != settings_->port())
            {
                return RoutingStatus::badPort;
            }

            if (target_.has_authority())
                hostName_ = std::string{target_.authority().host()};
        }

        return RoutingStatus::ok;
    }

    void sendRoutingError(RoutingStatus s)
    {
        using HD = HttpDenial;
        using HS = HttpStatus;

        switch (s)
        {
        case RoutingStatus::badHost:
            return deny(HD{HS::badRequest}.withMessage("Invalid hostname"));

        case RoutingStatus::badTarget:
            return deny(HD{HS::badRequest}.withMessage("Invalid request-target"));

        case RoutingStatus::badScheme:
            return deny(HD{HS::misdirectedRequest}.withMessage("Invalid scheme"));

        case RoutingStatus::badPort:
            return deny(HD{HS::misdirectedRequest}.withMessage("Invalid port"));

        default:
            assert(false && "Unexpected RoutingStatus enumerator");
            break;
        }
    }

    bool checkRead(boost::system::error_code netEc)
    {
        if (!netEc)
            return true;

        bool isEof = netEc == boost::beast::http::error::end_of_stream ||
                     netEc == boost::beast::http::error::partial_message;

        if (isEof)
        {
            if (shutdownHandler_ != nullptr)
            {
                close();
                shutdownHandler_(std::error_code{});
                shutdownHandler_ = nullptr;
            }
            finish(AdmitResult::disconnected());
            return false;
        }

        auto ec = httpErrorCodeToStandard(netEc);

        if (ec == TransportErrc::disconnected)
        {
            close();
            finish(AdmitResult::disconnected());
        }
        else if (internal::isHttpParseErrorDueToClient(netEc))
        {
            finish(AdmitResult::rejected(ec));
        }
        else
        {
            close();
            finish(AdmitResult::failed(ec, "socket read"));
        }

        return false;
    }

    bool checkWrite(boost::system::error_code netEc)
    {
        if (!netEc)
            return true;

        monitor_.endResponse(steadyTime(), false);
        close();

        auto ec = httpErrorCodeToStandard(netEc);

        if (isPoisoned_ && shutdownHandler_ != nullptr)
        {
            post(std::move(shutdownHandler_), ec);
            shutdownHandler_ = nullptr;
            return false;
        }

        if (ec == TransportErrc::disconnected)
            finish(AdmitResult::disconnected());
        else
            finish(AdmitResult::failed(ec, "socket write"));
        return false;
    }

    void report(HttpStatus status)
    {
        if (!logger_)
            return;

        assert(parser_.has_value());
        const auto& hdr = parser_->get().base();
        auto action = actionFromRequestVerb(hdr.method());
        auto statusStr = std::to_string(static_cast<unsigned>(status));
        AccessActionInfo info{action, hdr.target(), {}, std::move(statusStr)};

        if (action == AccessAction::clientHttpOther)
        {
            info.options.emplace("method",
                                 std::string{hdr.method_string()});
        }

        if (status_ == HttpStatus::continueRequest)
            info.options.emplace("Expect", "100-continue");

        auto upgradeField = hdr.find(Field::upgrade);
        if (upgradeField != hdr.end())
            info.options.emplace("upgrade", std::string{upgradeField->value()});

        HttpAccessInfo httpInfo{hostName_, fieldOr("User-Agent", {})};
        logger_->log(AccessLogEntry{connectionInfo_, std::move(httpInfo),
                                    std::move(info)});
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

    void sendSimpleError(HttpDenial& denial)
    {
        namespace http = boost::beast::http;

        std::string body{std::move(denial).message()};
        body += "\r\n";
        HttpStringResponse response{denial.status(), std::move(body),
                                    std::move(denial).fields()};
        sendResponse(response, denial.result());
    }

    void sendGeneratedError(HttpDenial& denial)
    {
        HttpFieldMap fields{std::move(denial).fields()};
        fields.emplace("Content-Type", "text/html; charset=utf-8");
        HttpStringResponse response{denial.status(), generateErrorPage(denial),
                                    std::move(fields)};
        sendResponse(response, denial.result());
    }

    void sendCustomGeneratedError(HttpDenial& denial, const HttpErrorPage& page)
    {
        std::string mime{"text/html; charset="};
        mime += page.charset().empty() ? std::string{"utf-8"} : page.charset();
        HttpFieldMap fields{std::move(denial).fields()};
        fields.emplace("Content-Type", std::move(mime));

        std::string body = page.generator()(page.status(), denial.message());
        HttpStringResponse response{page.status(), std::move(body),
                                    std::move(fields)};
        sendResponse(response, denial.result());
    }

    std::string generateErrorPage(HttpDenial& denial) const
    {
        auto errorMessage = make_error_code(denial.status()).message();
        std::string body{
            "<!DOCTYPE html><html>\r\n"
            "<head><title>" + errorMessage + "</title></head>\r\n"
            "<body>\r\n"
            "<h1>" + errorMessage + "</h1>\r\n"};

        if (!denial.message().empty())
            body += "<p>" + denial.message() + "</p>";

        body += "<hr>\r\n" +
                blockOptions().agent() +
                "</body>"
                "</html>";

        return body;
    }

    void redirectError(HttpDenial& denial, const HttpErrorPage& page)
    {
        HttpFieldMap fields{std::move(denial).fields()};
        fields.emplace("Location", page.uri());

        HttpResponse response{page.status(), std::move(fields)};
        sendResponse(response, denial.result());
    }

    void sendErrorFromFile(HttpDenial& denial, const HttpErrorPage& page)
    {
        namespace beast = boost::beast;
        namespace http = beast::http;

        const auto& docRoot = blockOptions().fileServingOptions().documentRoot();
        boost::filesystem::path absolutePath{docRoot};
        absolutePath /= page.uri();

        HttpFile file;
        auto ec = file.open(absolutePath.c_str());
        if (ec)
        {
            file.close();
            denial.setStatus(page.status());
            denial.withResult(AdmitResult::failed(ec, "error file read"));
            return sendGeneratedError(denial);
        }

        std::string mime{"text/html; charset="};
        mime += page.charset().empty() ? std::string{"utf-8"} : page.charset();
        HttpFieldMap fields{std::move(denial).fields()};
        fields.emplace("Content-type", std::move(mime));
        HttpFileResponse response{page.status(), std::move(file),
                                  std::move(fields)};
        sendResponse(response, denial.result());
    }

    void sendResponse(HttpResponse& response, AdmitResult result)
    {
        responseSent_ = true;
        assert(parser_.has_value());

        using AS = AdmitStatus;
        keepAlive_ =
            (result.status() == AS::wamp) ||
            (result.status() == AS::responded && parser_->keep_alive());

        auto increment = blockOptions().limits().responseIncrement();

        serializer_ = response.takeSerializer();
        serializer_->prepare(increment, parser_->get().version(),
                             blockOptions().agent(), keepAlive_);
        result_ = result;
        status_ = response.status();
        monitor_.startResponse(steadyTime(),
                               blockOptions().timeouts().responseTimeout());
        sendMoreResponse();
    }

    void sendMoreResponse()
    {
        auto self = shared_from_this();
        serializer_->asyncWriteSome(
            tcpSocket_,
            [this, self](boost::beast::error_code netEc, std::size_t n)
            {
                if (!checkWrite(netEc))
                    return;

                auto now = steadyTime();

                if (!serializer_->done())
                {
                    monitor_.updateResponse(now, n);
                    return sendMoreResponse();
                }

                monitor_.endResponse(
                    now, keepAlive_,
                    blockOptions().timeouts().keepaliveTimeout());

                if (isPoisoned_)
                    onShutdownResponseSent();
                else
                    onResponseSent();
            });
    }

    void onShutdownResponseSent()
    {
        finish(result_);
        doShutdown();
    }

    void onResponseSent()
    {
        report(status_);

        if (expectationReceived_)
        {
            expectationReceived_ = false;

            /*  If we intend to keep the connection open, then the body
            following a header containing 'Expect: 100-continue' must be
            read and discarded/processed even if an intermediary response
            other than 100 has been previously sent.

            Excerpt from https://curl.se/mail/lib-2004-08/0002.html :

                For this reason, the server has only two possible
                subsequent behaviours: read and discard the request body,
                or don't process any further input from that connection
                (i.e. close it, using TCP-safe lingering close). And the
                client has only two possible subsequent behaviours: send
                the request body to be discarded, or close the connection
                after receiving the error response.
        */
            if (result_.status() == AdmitStatus::rejected)
                finish(result_);
            else
                readBody();
        }
        else if (keepAlive_)
        {
            start();
        }
        else
        {
            finish(result_);
        }
    }

    void finish(AdmitResult result)
    {
        if (admitHandler_)
            admitHandler_(result);
        admitHandler_ = nullptr;
    }


    template <typename F, typename... Ts>
    void post(F&& handler, Ts&&... args)
    {
        postAny(tcpSocket_.get_executor(), std::forward<F>(handler),
                std::forward<Ts>(args)...);
    }

    const HttpServerOptions& blockOptions() const
    {
        return serverBlock_ ? serverBlock_->options() : settings_->options();
    }

    static constexpr std::size_t flushReadSize_ = 1536;

    TcpSocket tcpSocket_;
    CodecIdSet codecIds_;
    TransportInfo transportInfo_;
    boost::beast::flat_buffer streamBuffer_;
    std::string body_;
    std::string bodyBuffer_;
    boost::optional<Parser> parser_;
    boost::urls::url target_;
    Monitor monitor_;
    AdmitHandler admitHandler_;
    ShutdownHandler shutdownHandler_;
    ConnectionInfo connectionInfo_;
    std::string hostName_;
    AdmitResult result_;
    internal::HttpSerializerBase::Ptr serializer_;
    WebsocketServerTransportPtr upgradedTransport_;
    SettingsPtr settings_;
    RouterLogger::Ptr logger_;
    HttpServerBlock* serverBlock_ = nullptr;
    HttpStatus status_ = HttpStatus::none;
    bool isShedding_ = false;
    bool isPoisoned_ = false;
    bool isReading_ = false;
    bool responseSent_ = false;
    bool keepAlive_ = false;
    bool alreadyRequested_ = false;
    bool expectationReceived_ = false;

    friend class HttpServerTransport;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_HTTPJOBIMPL_HPP
