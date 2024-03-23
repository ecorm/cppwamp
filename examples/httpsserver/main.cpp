/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// HTTPS + Websocket Secure + WAMP server example
//******************************************************************************

#include <csignal>
#include <utility>
#include <boost/asio/signal_set.hpp>
#include <cppwamp/authenticators/anonymousauthenticator.hpp>
#include <cppwamp/directsession.hpp>
#include <cppwamp/router.hpp>
#include <cppwamp/unpacker.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/httpsserver.hpp>
#include <cppwamp/utils/consolelogger.hpp>

const std::string realmUri = "cppwamp.examples";

//------------------------------------------------------------------------------
namespace wamp
{
// Convert a std::tm to/from an object variant.
template <typename TConverter>
void convert(TConverter& conv, std::tm& t)
{
    conv("sec",   t.tm_sec)
        ("min",   t.tm_min)
        ("hour",  t.tm_hour)
        ("mday",  t.tm_mday)
        ("mon",   t.tm_mon)
        ("year",  t.tm_year)
        ("wday",  t.tm_wday)
        ("yday",  t.tm_yday)
        ("isdst", t.tm_isdst);
}
}

//------------------------------------------------------------------------------
class TimeService : public wamp::RealmObserver
{
public:
    static std::shared_ptr<TimeService>
    create(wamp::AnyIoExecutor exec, wamp::Realm realm)
    {
        return std::shared_ptr<TimeService>(
            new TimeService(std::move(exec), std::move(realm)));
    }

    void start(wamp::DirectRouterLink router)
    {
        realm_.observe(shared_from_this());

        auto self = shared_from_this();
        session_.connect(std::move(router));
        session_.join(
            realmUri,
            [this, self](wamp::ErrorOr<wamp::Welcome> info)
            {
                info.value(); // Throws if join failed
                enroll();
            });
    }

private:
    explicit TimeService(wamp::AnyIoExecutor exec, wamp::Realm realm)
        : realm_(std::move(realm)),
          session_(exec),
          timer_(std::move(exec))
    {}

    static std::tm getTime()
    {
        auto t = std::time(nullptr);
        return *std::localtime(&t);
    }

    void onSubscribe(const wamp::SessionInfo&,
                     const wamp::SubscriptionInfo& sub) override
    {
        if (sub.uri == "time_tick")
            subscriptionCount_ = sub.subscriberCount;
    }

    void onUnsubscribe(const wamp::SessionInfo&,
                       const wamp::SubscriptionInfo& sub) override
    {
        if (sub.uri == "time_tick")
            subscriptionCount_ = sub.subscriberCount;
    }

    void enroll()
    {
        auto self = shared_from_this();
        session_.enroll(
            "get_time",
            wamp::simpleRpc<std::tm>(&getTime),
            [this, self](wamp::ErrorOr<wamp::Registration> reg)
            {
                reg.value(); // Throws if enroll failed
                deadline_ = std::chrono::steady_clock::now();
                kickTimer();
            });
    }

    void kickTimer()
    {
        deadline_ += std::chrono::seconds(1);
        timer_.expires_at(deadline_);

        auto self = shared_from_this();
        timer_.async_wait(
            [this, self](boost::system::error_code ec)
            {
                if (ec)
                    throw boost::system::system_error(ec);
                if (subscriptionCount_ != 0)
                    publish();
                kickTimer();
            });
    }

    void publish()
    {
        auto t = std::time(nullptr);
        const std::tm* local = std::localtime(&t);
        session_.publish(wamp::Pub("time_tick").withArgs(*local));
    }

    wamp::Realm realm_;
    wamp::DirectSession session_;
    boost::asio::steady_timer timer_;
    std::chrono::steady_clock::time_point deadline_;
    std::size_t subscriptionCount_ = 0;
};

//------------------------------------------------------------------------------
wamp::ErrorOr<wamp::SslContext> makeSslContext()
{
    /*  Key/certificate pair generated using the following command:
            openssl req -x509 -newkey rsa:4096 -sha256 -days 3650 \
            -keyout localhost.key -passout pass:"test" -out localhost.crt \
            -subj "/CN=localhost"

        Diffie-Hellman parameter generated using the following command:
            openssl dhparam -dsaparam -out dh4096.pem 4096
    */

    wamp::SslContext ssl;

    ssl.setPasswordCallback(
           [](std::size_t, wamp::SslPasswordPurpose) {return "test";}).value();

    ssl.useCertificateChainFile("./certs/localhost.crt").value();

    ssl.usePrivateKeyFile("./certs/localhost.key",
                          wamp::SslFileFormat::pem).value();

#ifndef CPPWAMP_SSL_AUTO_DIFFIE_HELLMAN_AVAILABLE
    ssl.useTempDhFile("./certs/dh4096.pem").value();
#endif

    return ssl;
}

//------------------------------------------------------------------------------
wamp::ServerOptions httpsServerOptions()
{
    // These options are inherited by all blocks
    auto baseFileServerOptions =
        wamp::HttpFileServingOptions{}.withDocumentRoot("./www")
                                      .withCharset("utf-8");

    auto altFileServingOptions =
        wamp::HttpFileServingOptions{}.withDocumentRoot("./www-alt");

    auto mainRoute =
        wamp::HttpServeFiles{"/"}
            .withOptions(wamp::HttpFileServingOptions{}.withAutoIndex());

    auto altRoute =
        wamp::HttpServeFiles{"/alt"}
            .withAlias("/") // Substitutes "/alt" with "/"
                            // before appending to "./www-alt"
            .withOptions(altFileServingOptions);

    auto redirectRoute =
        wamp::HttpRedirect{"/wikipedia"}
            .withScheme("https")
            .withAuthority("en.wikipedia.org")
            .withAlias("/wiki") // Substitutes "/wikipedia" with "/wiki"
            .withStatus(wamp::HttpStatus::temporaryRedirect);

    auto wsRoute = wamp::HttpWebsocketUpgrade{"/time"};

    auto altBlockMainRoute =
        wamp::HttpServeFiles{"/"}.withOptions(altFileServingOptions);

    auto httpOptions =
        wamp::HttpServerOptions{}
            .withFileServingOptions(baseFileServerOptions)
            .addErrorPage({wamp::HttpStatus::notFound, "/notfound.html"});

    auto mainBlock =
        wamp::HttpServerBlock{}.addPrefixRoute(mainRoute)
                               .addExactRoute(altRoute)
                               .addPrefixRoute(redirectRoute)
                               .addExactRoute(wsRoute);

    auto altBlock =
        wamp::HttpServerBlock{"alt.localhost"}
            .addPrefixRoute(altBlockMainRoute);

    auto httpsEndpoint =
        wamp::HttpsEndpoint{8443, &makeSslContext}
            .withOptions(httpOptions)
            .addBlock(mainBlock)
            .addBlock(altBlock);

    auto serverOptions =
        wamp::ServerOptions("https8443", std::move(httpsEndpoint),
                            wamp::jsonWithMaxDepth(10));

    return serverOptions;
}

//------------------------------------------------------------------------------
wamp::ServerOptions httpServerOptions()
{
    auto redirectRoute =
        wamp::HttpRedirect{"/"}
            .withScheme("https")
            .withPort(8443)
            .withStatus(wamp::HttpStatus::temporaryRedirect);

    auto mainBlock = wamp::HttpServerBlock{}.addPrefixRoute(redirectRoute);

    auto httpEndpoint = wamp::HttpEndpoint{8080}.addBlock(mainBlock);

    auto serverOptions =
        wamp::ServerOptions("http8080", std::move(httpEndpoint),
                            wamp::jsonWithMaxDepth(10));

    return serverOptions;
}

//------------------------------------------------------------------------------
int main()
{
    try
    {
        auto loggerOptions =
            wamp::utils::ConsoleLoggerOptions{}.withOriginLabel("router")
                                               .withColor();
        wamp::utils::ConsoleLogger logger{std::move(loggerOptions)};

        auto routerOptions = wamp::RouterOptions()
            .withLogHandler(logger)
            .withLogLevel(wamp::LogLevel::info)
            .withAccessLogHandler(wamp::AccessLogFilter(logger));

        auto realmOptions = wamp::RealmOptions(realmUri);

        logger({wamp::LogLevel::info, "CppWAMP example web server launched"});
        wamp::IoContext ioctx;

        wamp::Router router{ioctx, routerOptions};
        auto realm = router.openRealm(realmOptions).value();
        router.openServer(httpsServerOptions());
        router.openServer(httpServerOptions());

        auto service = TimeService::create(ioctx.get_executor(), realm);
        service->start(router);

        boost::asio::signal_set signals{ioctx, SIGINT, SIGTERM};
        signals.async_wait(
            [&router](const boost::system::error_code& ec, int sig)
            {
                if (ec)
                    return;
                const char* sigName = (sig == SIGINT)  ? "SIGINT" :
                                      (sig == SIGTERM) ? "SIGTERM" : "unknown";
                router.log({wamp::LogLevel::info,
                            std::string("Received ") + sigName + " signal"});
                router.close();
            });

        ioctx.run();
        logger({wamp::LogLevel::info, "CppWAMP example web server exit"});
    }
    catch (const std::exception& e)
    {
        std::cerr << "Unhandled exception: " << e.what() << ", terminating."
                  << std::endl;
    }
    catch (...)
    {
        std::cerr << "Unhandled exception: <unknown>, terminating."
                  << std::endl;
    }

    return 0;
}
