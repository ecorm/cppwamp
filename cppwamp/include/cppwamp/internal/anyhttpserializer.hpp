/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_ANYHTTPSERIALIZER_HPP
#define CPPWAMP_INTERNAL_ANYHTTPSERIALIZER_HPP

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http/write.hpp>
#include "../anyhandler.hpp"
#include "../traits.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class HttpSerializerBase
{
public:
    using Socket = boost::asio::ip::tcp::socket;
    using Handler = AnyCompletionHandler<void (boost::beast::error_code,
                                               std::size_t)>;

    virtual ~HttpSerializerBase() = default;

    virtual void asyncWriteSome(Socket& tcp, Handler handler) = 0;

    virtual bool done() const = 0;

protected:
    HttpSerializerBase() = default;
};


//------------------------------------------------------------------------------
template <typename R>
class PolymorphicHttpSerializer : public HttpSerializerBase
{
public:
    using Response = R;

    template <typename T>
    explicit PolymorphicHttpSerializer(T&& response, std::size_t limit)
        : response_(std::forward<T>(response)),
          serializer_(response_)
    {
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

private:
    using Body = typename Response::body_type;
    using Fields = typename Response::fields_type;
    using Serializer = boost::beast::http::serializer<false, Body, Fields>;

    Response response_;
    Serializer serializer_;
};

//------------------------------------------------------------------------------
// Type-erases a boost::beast::http::serializer so that the same incremental
// write algorithm can work with any response body type.
//------------------------------------------------------------------------------
class AnyHttpSerializer
{
public:
    using Socket = boost::asio::ip::tcp::socket;
    using Handler = AnyCompletionHandler<void (boost::beast::error_code,
                                               std::size_t)>;

    AnyHttpSerializer() = default;

    template <typename R>
    AnyHttpSerializer(R&& response, std::size_t limit)
        : serializer_(makeSerializer(std::forward<R>(response), limit))
    {}

    explicit operator bool() const {return empty();}

    bool empty() const {return serializer_ == nullptr;}

    bool done() const {return empty() ? true : serializer_->done();}

    void reset() {serializer_.reset();}

    template <typename R>
    void reset(R&& response, std::size_t limit)
    {
        serializer_.reset(makeSerializer(std::forward<R>(response), limit));
    }

    void asyncWriteSome(Socket& tcp, Handler handler)
    {
        assert(!empty());
        serializer_->asyncWriteSome(tcp, std::move(handler));
    }

private:
    template <typename R>
    static PolymorphicHttpSerializer<Decay<R>>*
    makeSerializer(R&& response, std::size_t limit)
    {
        using T = Decay<R>;
        return new PolymorphicHttpSerializer<T>(std::forward<R>(response),
                                                limit);
    }

    std::unique_ptr<HttpSerializerBase> serializer_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_ANYHTTPSERIALIZER_HPP
