/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023-2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_HTTPJOB_HPP
#define CPPWAMP_TRANSPORTS_HTTPJOB_HPP

#include <initializer_list>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/string_type.hpp>
#include <boost/beast/http/buffer_body.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/file_body.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/url.hpp>
#include "httpprotocol.hpp"
#include "../routerlogger.hpp"
#include "../internal/anyhttpserializer.hpp"
#include "../internal/servertimeoutmonitor.hpp"

namespace wamp
{

namespace internal
{
class HttpServerTransport;
class WebsocketServerTransport;
}

//------------------------------------------------------------------------------
class HttpJob : public std::enable_shared_from_this<HttpJob>
{
public:
    using Ptr          = std::shared_ptr<HttpJob>;
    using TcpSocket    = boost::asio::ip::tcp::socket;
    using Settings     = HttpEndpoint;
    using SettingsPtr  = std::shared_ptr<Settings>;
    using AdmitHandler = AnyCompletionHandler<void (AdmitResult)>;
    using Header       = boost::beast::http::request_header<>;
    using Field        = boost::beast::http::field;
    using StringView   = boost::beast::string_view;
    using FieldList    = std::initializer_list<std::pair<Field, StringView>>;
    using Url          = boost::urls::url;

    HttpJob(TcpSocket&& t, SettingsPtr s, const CodecIdSet& c,
            ConnectionInfo i, RouterLogger::Ptr l);

    const Url& target() const;

    const Header& header() const;

    const std::string& body() const &;

    std::string&& body() &&;

    std::string fieldOr(Field field, std::string fallback);

    const HttpEndpoint& settings() const;

    void continueRequest();

    template <typename R>
    void respond(R&& response, HttpStatus status = HttpStatus::ok)
    {
        if (responseSent_)
            return;
        sendResponse(std::forward<R>(response), status,
                     AdmitResult::responded());
    }

    void websocketUpgrade(WebsocketOptions options,
                          const WebsocketServerLimits limits);

    void balk(HttpStatus status, std::string what = {}, bool simple = false,
              AdmitResult result = AdmitResult::responded(),
              FieldList fields = {});

private:
    using Base            = HttpJob;
    using Body            = boost::beast::http::buffer_body;
    using Request         = boost::beast::http::request<Body>;
    using Parser          = boost::beast::http::request_parser<Body>;
    using Verb            = boost::beast::http::verb;
    using Monitor         = internal::HttpServerTimeoutMonitor<Settings>;
    using TimePoint       = std::chrono::steady_clock::time_point;
    using ShutdownHandler = AnyCompletionHandler<void (std::error_code)>;
    using WebsocketServerTransportPtr =
        std::shared_ptr<internal::WebsocketServerTransport>;

    static TimePoint steadyTime();

    static std::error_code httpErrorCodeToStandard(
        boost::system::error_code netEc);

    static HttpStatus shutdownReasonToHttpStatus(std::error_code ec);

    std::error_code monitor();

    void process(bool isShedding, AdmitHandler handler);

    void shutdown(std::error_code reason, ShutdownHandler handler);

    void doShutdown();

    void flush();

    void onFlushComplete(boost::system::error_code netEc);

    void close();

    const WebsocketServerTransportPtr& upgradedTransport() const;

    void start();

    void waitForHeader();

    void readHeader();

    void onHeaderRead(boost::beast::error_code netEc);

    void onExpectationReceived(boost::beast::string_view expectField);

    void readBody();

    void readMoreBody();

    void onMessageRead();

    bool parseAndNormalizeTarget();

    bool checkRead(boost::system::error_code netEc);

    bool checkWrite(boost::system::error_code netEc);

    void report(HttpStatus status);

    static AccessAction actionFromRequestVerb(Verb verb);

    void sendSimpleError(HttpStatus status, std::string&& what,
                         FieldList fields, AdmitResult result);

    void sendGeneratedError(HttpStatus status, const std::string& what,
                            FieldList fields, AdmitResult result);

    void sendCustomGeneratedError(const HttpErrorPage& page,
                                  const std::string& what,
                                  FieldList fields, AdmitResult result);

    std::string generateErrorPage(wamp::HttpStatus status,
                                  const std::string& what) const;

    void redirectError(const HttpErrorPage& page, FieldList fields,
                       AdmitResult result);

    void sendErrorFromFile(const HttpErrorPage& page, const std::string& what,
                           FieldList fields, AdmitResult result);

    template <typename R>
    void sendResponse(R&& response, HttpStatus status, AdmitResult result)
    {
        responseSent_ = true;
        assert(parser_.has_value());
        keepAlive_ = result.status() == AdmitStatus::responded &&
                     parser_->keep_alive();
        // Beast will adjust the Connection field automatically depending on
        // the HTTP version.
        // https://datatracker.ietf.org/doc/html/rfc7230#section-6.3
        response.keep_alive(keepAlive_);

        response.set(boost::beast::http::field::server, settings_->agent());

        // Set the Connection field to close if we intend to shut down the
        // connection after sending the response.
        // https://datatracker.ietf.org/doc/html/rfc9112#section-9.6
        if (!keepAlive_ && result.status() != AdmitStatus::wamp)
            response.set(boost::beast::http::field::connection, "close");

        response.prepare_payload();

        serializer_.reset(std::forward<R>(response),
                          settings_->limits().httpResponseIncrement());
        result_ = result;
        status_ = status;
        monitor_.startResponse(steadyTime());
        sendMoreResponse();
    }

    void sendMoreResponse();

    void onShutdownResponseSent();

    void onResponseSent();

    void finish(AdmitResult result);

    template <typename F, typename... Ts>
    void post(F&& handler, Ts&&... args);

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
    AdmitResult result_;
    internal::AnyHttpSerializer serializer_;
    WebsocketServerTransportPtr upgradedTransport_;
    SettingsPtr settings_;
    RouterLogger::Ptr logger_;
    HttpStatus status_ = HttpStatus::none;
    bool isShedding_ = false;
    bool isPoisoned_ = false;
    bool isReading_ = false;
    bool responseSent_ = false;
    bool keepAlive_ = false;
    bool alreadyRequested_ = false;
    bool expectationReceived_ = false;

    friend class internal::HttpServerTransport;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/httpjob.inl.hpp
#endif

#endif // CPPWAMP_TRANSPORTS_HTTPJOB_HPP
