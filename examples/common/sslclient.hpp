/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_EXAMPLES_SSLCLIENT_HPP
#define CPPWAMP_EXAMPLES_SSLCLIENT_HPP

#include <iostream>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <cppwamp/transports/sslcontext.hpp>

//------------------------------------------------------------------------------
wamp::SslContext makeClientSslContext()
{
    /*  Generated using the following command:
            openssl req -x509 -newkey rsa:4096 -sha256 -days 3650 \
            -keyout localhost.key -passout pass:"test" -out localhost.crt \
            -subj "/CN=localhost" */
    static const std::string cert{
R"(-----BEGIN CERTIFICATE-----
MIIFCTCCAvGgAwIBAgIUGu8YUgY0nQUw733Hkw10RjH0QjkwDQYJKoZIhvcNAQEL
BQAwFDESMBAGA1UEAwwJbG9jYWxob3N0MB4XDTI0MDMwNzA0NDQyM1oXDTM0MDMw
NTA0NDQyM1owFDESMBAGA1UEAwwJbG9jYWxob3N0MIICIjANBgkqhkiG9w0BAQEF
AAOCAg8AMIICCgKCAgEApEV7+AWqf5Y6KRn0L9lkF6uEb/aSLO76gpYU48YMMU1t
UIcBdgmJLHoqT/r1cyyQqLxGp2IWqAPrLjCLrzeQ246ZxzpxlAnEfhbICo96jIF8
g7aXiryAWDRUcCjR9wjPBZx6M9qqw9FlehrEhV54CPG7fssT/6xR5Pv3MNhrKffq
h5e8aLkUDKcrubhGbXZ18OquvEOymZ4UvLmD6NACexeJahrmt0ZsrOwMqZ+hbpIz
2+QqhiXx42PzMTgHnCRvkrmijB+3QbBMl5TshFB5BHvgYcHKqoy0ZKmurzTfwycj
rcNYGA7hJDT8EmJGK2R7/2thE/TdqFNv0V928HyaWJzQm4AFceTkDsjFTJxLqWAL
H6jF0FJw5GIFjF2JCRffypB0aohgR4ZnQasdd61dbqxpTMVs7ySjDxXhQy5wzzsU
1ZhmiYw7iWf1eX0AE0qPlqnOTm0yBpRNQMrXRwIg7R1zUhWj+KKrqTXGP0kfFRP5
azn+OzT+8PZt4dUgWJpyehdcSUrdG4L6THYtpU+2k5Uu29/3TwwTO6Y3MsEDjhYb
Pv+t4v1ueCUdHlYRRN7jeG/K2D6IgBbA+cW2hRvxdPStUCsT16GpHckMqbrAY9t8
xkkvmmetSgLmQica2iD5yOVL3OOLpCTiYZcKROhRRK19qPQ+3dWw1hj/poN1kdEC
AwEAAaNTMFEwHQYDVR0OBBYEFLjP3KdU4KJSzui5ZcYuWCO/FCZfMB8GA1UdIwQY
MBaAFLjP3KdU4KJSzui5ZcYuWCO/FCZfMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZI
hvcNAQELBQADggIBAJxxFejECHs3lB8W6iCr8Qz+j/OZgZJ3H2DSjFuvHtRUiJ3I
x760wPS7x1ofa3wjE5g8DIwuaDJyd/xH36VjFl9n6ibwi0dXR4ymM8kbWjQtpwxD
0XPOtk/2nk+BaiTA35j7euhcuFNd0XyZdfnEC9Z/OEq7M783NELjvIcWb/K1Xf+p
6xyGKrqcUpVDRXem9K25/TruzUYWaFliKKalOk2iHPAlxvPKG1aBn879/OJJRwgc
QP6PmUEmNJPvJiPmIUrXCthhqwjD3L1aBKn/q0UUqM0JpEzC4w+n1lGdCFcr0toY
MeyAnx8i/4gfli3CX4ec1DYSsH6qj2efEYghPP5m/Bw761tvbpW4Wrk0Y4Rf66N2
sjQchZisIx+uRZ0ZTNujF3yCOKXOHBMknafgAjtk33vHkCCXWzNnP4i/WIwOw9ri
A1lE2wwR+fY7P1tAVHeK8rO3x5Wzat+FHaNzt54oMuoZxPA2x9oljUyDeV9e+Hw9
QF89ktu5FmQ8YO7LG4L9eVUuesyUSgQEeOkJz/3ATQprpWVUaL5GjVQUiltQEmlt
YzzfQ6STTyvmcropIHKBm9qIzI/c5ZqPzaMu9ZGuDcvajM8k7aDcow9hlcJSfsov
fEgX9rn6OuBLrld1SgCej/0da2lBb26uohpBoDRzlB9Fw4qsuIAWQoUnNF3t
-----END CERTIFICATE-----)"
    };

    wamp::SslContext ssl;
    ssl.addVerifyCertificate(cert.data(), cert.size()).value();
    return ssl;
}

//------------------------------------------------------------------------------
bool verifySslCertificate(bool preverified, wamp::SslVerifyContext ctx)
{
    // Simply print the certificate's subject name.
    char name[256];
    auto handle = ctx.as<X509_STORE_CTX>();
    X509* cert = ::X509_STORE_CTX_get_current_cert(handle);
    ::X509_NAME_oneline(::X509_get_subject_name(cert), name, sizeof(name));
    std::cout << "Verifying " << name << "\n";
    return preverified;
}

#endif // CPPWAMP_EXAMPLES_SSLCLIENT_HPP
