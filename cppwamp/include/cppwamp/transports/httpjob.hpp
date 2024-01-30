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
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/file_body.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/url.hpp>
#include "httpprotocol.hpp"
#include "websocketprotocol.hpp"
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

    template <typename TBody>
    using Response = boost::beast::http::response<TBody>;

    using HeaderResponse = Response<boost::beast::http::empty_body>;
    using StringResponse = Response<boost::beast::http::string_body>;
    using FileResponse   = Response<boost::beast::http::file_body>;

    HttpJob(TcpSocket&& t, SettingsPtr s, const CodecIdSet& c,
            ConnectionInfo i, RouterLogger::Ptr l);

    const Url& target() const;

    const Header& header() const;

    const std::string& body() const &;

    std::string&& body() &&;

    std::string fieldOr(Field field, std::string fallback) const;

    const std::string& hostName() const;

    const HttpEndpoint& settings() const;

    void continueRequest();

    void respond(HeaderResponse&& response, HttpStatus status = HttpStatus::ok);

    void respond(StringResponse&& response, HttpStatus status = HttpStatus::ok);

    void respond(FileResponse&& response, HttpStatus status = HttpStatus::ok);

    void websocketUpgrade(WebsocketOptions options,
                          const WebsocketServerLimits& limits);

    // TODO: Replace 5 arguments with single object
    void balk(HttpStatus status, std::string what = {}, bool simple = true,
              AdmitResult result = AdmitResult::responded(),
              FieldList fields = {});

private:
    using Base            = HttpJob;
    using Body            = boost::beast::http::buffer_body;
    using Request         = boost::beast::http::request<Body>;
    using Parser          = boost::beast::http::request_parser<Body>;
    using Verb            = boost::beast::http::verb;
    using Monitor         = internal::HttpServerTimeoutMonitor;
    using TimePoint       = std::chrono::steady_clock::time_point;
    using ShutdownHandler = AnyCompletionHandler<void (std::error_code)>;
    using WebsocketServerTransportPtr =
        std::shared_ptr<internal::WebsocketServerTransport>;

    enum class RoutingStatus
    {
        ok,
        badHost,
        badTarget
    };

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

    void onRequestRead();

    RoutingStatus interpretRoutingInformation();

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
    void sendResponse(R&& response, HttpStatus status, AdmitResult result);

    void sendMoreResponse();

    void onShutdownResponseSent();

    void onResponseSent();

    void finish(AdmitResult result);

    template <typename F, typename... Ts>
    void post(F&& handler, Ts&&... args);

    const HttpServerOptions& blockOptions() const;

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
    internal::AnyHttpSerializer serializer_;
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

    friend class internal::HttpServerTransport;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/httpjob.inl.hpp
#endif

#endif // CPPWAMP_TRANSPORTS_HTTPJOB_HPP
