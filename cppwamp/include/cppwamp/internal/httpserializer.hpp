/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_HTTPSERIALIZER_HPP
#define CPPWAMP_INTERNAL_HTTPSERIALIZER_HPP

#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/file_body.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class HttpSerializerVisitor
{
public:
    template <typename TBody>
    using Serializer = boost::beast::http::response_serializer<TBody>;

    using EmptyBody = boost::beast::http::empty_body;
    using StringBody = boost::beast::http::string_body;
    using FileBody = boost::beast::http::file_body;

    virtual ~HttpSerializerVisitor() = default;

    virtual void visit(Serializer<EmptyBody>& serializer) = 0;

    virtual void visit(Serializer<StringBody>& serializer) = 0;

    virtual void visit(Serializer<FileBody>& serializer) = 0;
};

//------------------------------------------------------------------------------
class HttpSerializerBase
{
public:
    using Ptr = std::unique_ptr<HttpSerializerBase>;

    virtual ~HttpSerializerBase() = default;

    virtual void prepare(std::size_t limit, unsigned httpVersion,
                         const std::string& agent, bool keepAlive) = 0;

    virtual void apply(HttpSerializerVisitor& visitor) = 0;

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

    void apply(HttpSerializerVisitor& visitor) override
    {
        visitor.visit(serializer_);
    }

    bool done() const override
    {
        return serializer_.is_done();
    }

    Response& response() {return response_;}

    const Response& response() const {return response_;}

private:
    using Body = typename Response::body_type;
    using Serializer = boost::beast::http::response_serializer<Body>;

    Response response_;
    Serializer serializer_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_HTTPSERIALIZER_HPP
