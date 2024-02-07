/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_HTTPSERIALIZER_HPP
#define CPPWAMP_INTERNAL_HTTPSERIALIZER_HPP

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http/write.hpp>
#include "../anyhandler.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class HttpSerializerBase
{
public:
    using Ptr = std::unique_ptr<HttpSerializerBase>;
    using Socket = boost::asio::ip::tcp::socket;
    using Handler = AnyCompletionHandler<void (boost::beast::error_code,
                                               std::size_t)>;

    virtual ~HttpSerializerBase() = default;

    virtual void prepare(std::size_t limit, unsigned httpVersion,
                         const std::string& agent, bool keepAlive) = 0;

    virtual void asyncWriteSome(Socket& tcp, Handler handler) = 0;

    virtual bool done() const = 0;

protected:
    HttpSerializerBase() = default;
};


//------------------------------------------------------------------------------
// Type-erases a boost::beast::http::serializer so that the same incremental
// write algorithm can work with any response body type.
//------------------------------------------------------------------------------
template <typename R>
class HttpSerializer : public HttpSerializerBase
{
public:
    using Response = R;

    explicit HttpSerializer(Response&& response)
        : response_(std::move(response)),
          serializer_(response_)
    {}

    void prepare(std::size_t limit, unsigned httpVersion,
                 const std::string& agent, bool keepAlive) override
    {
        // Beast will adjust the Connection field automatically depending on
        // the HTTP version.
        // https://datatracker.ietf.org/doc/html/rfc7230#section-6.3
        response_.version(httpVersion);
        response_.keep_alive(keepAlive);

        response_.set(boost::beast::http::field::server, agent);
        response_.prepare_payload();

        serializer_.limit(limit);
    }

    void asyncWriteSome(Socket& tcp, Handler handler) override
    {
        struct Written
        {
            Handler handler;

            void operator()(boost::beast::error_code netEc, std::size_t n)
            {
                handler(netEc, n);
            }
        };

        boost::beast::http::async_write_some(tcp, serializer_,
                                             Written{std::move(handler)});
    }

    bool done() const override
    {
        return serializer_.is_done();
    }

    Response& response() {return response_;}

    const Response& response() const {return response_;}

private:
    using Body = typename Response::body_type;
    using Fields = typename Response::fields_type;
    using Serializer = boost::beast::http::serializer<false, Body, Fields>;

    Response response_;
    Serializer serializer_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_HTTPSERIALIZER_HPP
