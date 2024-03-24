/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_EXAMPLES_SSLSERVER_HPP
#define CPPWAMP_EXAMPLES_SSLSERVER_HPP

#include <cppwamp/transports/sslcontext.hpp>

//------------------------------------------------------------------------------
wamp::SslContext makeServerSslContext()
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

    static const std::string key{
R"(-----BEGIN ENCRYPTED PRIVATE KEY-----
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
474yqYZGwtWKgp3w25JX1bzz8sATzy4C
-----END ENCRYPTED PRIVATE KEY-----)"};

    wamp::SslContext ssl;

    ssl.setPasswordCallback(
           [](std::size_t, wamp::SslPasswordPurpose) {return "test";}).value();

    ssl.useCertificateChain(cert.data(), cert.size()).value();
    ssl.usePrivateKey(key.data(), key.size(), wamp::SslFileFormat::pem).value();

#ifndef CPPWAMP_SSL_AUTO_DIFFIE_HELLMAN_AVAILABLE
    /* Generated using the following command:
            openssl dhparam -dsaparam -out dh4096.pem 4096 */
    static const std::string dh{
R"(-----BEGIN X9.42 DH PARAMETERS-----
MIIELAKCAgEA5/QQTsmKNJNIILsAue6WKE91fDVb9gSnuB/jwMWlNlFzg8+D9BoT
3E+9kJOS/diF1e7/4nPa/s78p9LL9ROB4p+tA/CsyxJtnKCztekz8evaj7xa5/24
av36jRbERpZNGFsBwOTkriyom/aUx+nSi8Cw9mhXFwHn/d+xNekQ7XGTSX6Nxddu
O3EWMAed5mAru4Ul49jJO8UEZaDpSDiqkNAG6bdaHcnC9flCwWaKtV8VaJ+dQlWV
rHRTSuLnAko8HO03L4MwQ9xQiu4gMwqiMbB1NTzlvmBxjf61wVaIa9SR24sXEw7/
YCZnQ89Oxd4CQjXI+kLFeaKOUiU4gglPfDQ7FGO9EBHPpeBVINTljXifUVokBI1r
XPhFzm1QYGYzyZNQoCNpCBOQHgpOv9lSd840ugGJ9z17xx/kR7Gzmbon0+INEGaf
dC0j+XqFqwHNOQGuJR4f3wJDU+yuD+pU1RDwFchNTJ8XKb7UZ2/tlQm/96VgXUqI
T4uWLmMO0E9XT/ws957ICkW5Lqb8r5oaQ5SXo+BczTmZoIH3OCYjI1bMBiOc1wgg
D6cSOOKjrXY82K/pBAXi6Grb8xOn41Qv3JflPn1fo8N/ukiFmBlPRtYR3+5WUdpH
51yGkwE0ijo2zf4Nzd0tmQyjN/x8PijCs73Arq+ijbidWtDHXyLNDhMCggIABU/N
ftF+OOL1j4Rby+tx1EnclApZYTqgEOLOKu8VL9M96Izmq3kqYDoPd+GohnpvCWpL
1fFzR4s10SKKbi9JWPVKSapP5lJi7HQSOh6LMGEhBvqixvaPGxO90dZ/K/4n8/Mn
jTZlOIzmlOYEq8EyDfnXcLBftj02yVLOMSuAGviBskOpgzY7juIFsuJhvTnTMm8p
1mHaMNfkCo6HUiw1Ve36AayEsjV/BX7GY+o6mYx1FNVPKMVbcX7LZjwDmBeWEkx7
uyYRUpXiqKeajyeStrVd335rPLJrT9CJNOI1ffPajhZHqKlL+T0oCrDaUSvzUJxt
KMOWWYob70K3sRQgoBnZq2enVHAWpBHe9YX/S4dsAZhAwVZhkd8yOL0f5Yk4GoLH
mYfWwBzYqjZ8YDF16LGGsuOdsbBddB1i94g1NqzB9iamnS8iT3qna1xQaUHgOJO/
2iNnwuIK4MLxmatADQ6sdp5AhQzCelnVU2+J4nSYXq5j1exPWrRsgUL7a4z5svy0
lDSTtW6HSzx3iHKlBAkC5wKtCwwao7cQODj7wlEB99prf7MyU19uwLGiET6loaFh
030EC+naCxJT5KhJAowJ16yd7H4kDubCVzikuYAgeWEOGBXnlmCPcqs5riEqggdx
StDNA873ouBeKE2TlvYw59FQSXHBYEsmLgJpJOMCIQCKePYDlfwwvG4n7o2VOs7e
iJXE/LGKJ23FsIwj7qMO6Q==
-----END X9.42 DH PARAMETERS-----)"};

    ssl.useTempDh(dh.data(), dh.size()).value();
#endif

    return ssl;
}

#endif // CPPWAMP_EXAMPLES_SSLSERVER_HPP
