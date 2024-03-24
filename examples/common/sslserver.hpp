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
-----END CERTIFICATE-----)"};

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
