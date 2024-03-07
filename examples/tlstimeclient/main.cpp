/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

//******************************************************************************
// Example WAMP service consumer app using TLS transport.
//******************************************************************************

#include <ctime>
#include <iostream>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <cppwamp/session.hpp>
#include <cppwamp/unpacker.hpp>
#include <cppwamp/variant.hpp>
#include <cppwamp/codecs/json.hpp>
#include <cppwamp/transports/tlsclient.hpp>

const std::string realm = "cppwamp.examples";
const char* defaultAddress = "localhost";
const char* defaultPort = "23456";

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
class TimeClient : public std::enable_shared_from_this<TimeClient>
{
public:
    static std::shared_ptr<TimeClient> create(wamp::AnyIoExecutor exec)
    {
        return std::shared_ptr<TimeClient>(new TimeClient(std::move(exec)));
    }

    void start(wamp::ConnectionWish where)
    {
        auto self = shared_from_this();
        session_.connect(
            std::move(where),
            [this, self](wamp::ErrorOr<size_t> index)
            {
                index.value(); // Throws if connect failed
                join();
            });
    }

private:
    TimeClient(wamp::AnyIoExecutor exec)
        : session_(std::move(exec))
    {}

    static void onTimeTick(std::tm time)
    {
        std::cout << "The current time is: " << std::asctime(&time) << "\n";
    }

    void join()
    {
        auto self = shared_from_this();
        session_.join(
            realm,
            [this, self](wamp::ErrorOr<wamp::Welcome> info)
            {
                info.value(); // Throws if join failed
                getTime();
            });
    }

    void getTime()
    {
        auto self = shared_from_this();
        session_.call(
            wamp::Rpc("get_time"),
            [this, self](wamp::ErrorOr<wamp::Result> result)
            {
                // result.value() throws if the call failed
                auto time = result.value()[0].to<std::tm>();
                std::cout << "The current time is: " << std::asctime(&time) << "\n";
                subscribe();
            });
    }

    void subscribe()
    {
        session_.subscribe(
            "time_tick",
            wamp::simpleEvent<std::tm>(&TimeClient::onTimeTick),
            [](wamp::ErrorOr<wamp::Subscription> sub)
            {
                sub.value(); // Throws if subscribe failed
            });
    }

    wamp::Session session_;
};


//------------------------------------------------------------------------------
wamp::SslContext makeSslContext()
{
    // Uses the same options and certificates as the Asio SSL client example
    // (https://www.boost.org/doc/libs/release/doc/html/boost_asio/example/cpp11/ssl/client.cpp).
    wamp::SslContext ssl;
    ssl.loadVerifyFile("./certs/localhost.crt").value();
    return ssl;
}

//------------------------------------------------------------------------------
bool verifyCertificate(bool preverified, wamp::SslVerifyContext ctx)
{
    // Simply print the certificate's subject name.
    char name[256];
    auto handle = ctx.as<X509_STORE_CTX>();
    X509* cert = ::X509_STORE_CTX_get_current_cert(handle);
    ::X509_NAME_oneline(::X509_get_subject_name(cert), name, sizeof(name));
    std::cout << "Verifying " << name << "\n";

    return preverified;
}

//------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    const char* port = argc >= 2 ? argv[1] : defaultPort;
    const char* address = argc >= 3 ? argv[2] : defaultAddress;

    wamp::IoContext ioctx;
    auto client = TimeClient::create(ioctx.get_executor());

    auto verif = wamp::SslVerifyOptions{}.withMode(wamp::SslVerifyMode::peer())
                                         .withCallback(&verifyCertificate);

    auto tls = wamp::TlsHost(address, port, &makeSslContext)
                   .withSslVerifyOptions(std::move(verif))
                   .withFormat(wamp::json);

    client->start(tls);
    ioctx.run();
    return 0;
}
