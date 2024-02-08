/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/httpresponse.hpp"
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/file_body.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include "../api.hpp"

namespace wamp
{

//******************************************************************************
// HttpDenial
//******************************************************************************

CPPWAMP_INLINE HttpDenial::HttpDenial(HttpStatus status)
    : status_(status)
{}

CPPWAMP_INLINE HttpDenial& HttpDenial::setStatus(HttpStatus status)
{
    status_ = status;
    return *this;
}

CPPWAMP_INLINE HttpDenial& HttpDenial::withMessage(std::string what)
{
    message_ = std::move(what);
    return *this;
}

CPPWAMP_INLINE HttpDenial& HttpDenial::withResult(AdmitResult result)
{
    result_ = result;
    return *this;
}

CPPWAMP_INLINE HttpDenial& HttpDenial::withFields(HttpFieldMap fields)
{
    fields_ = std::move(fields);
    return *this;
}

CPPWAMP_INLINE HttpDenial HttpDenial::withHtmlEnabled(bool enabled)
{
    htmlEnabled_ = enabled;
    return *this;
}

CPPWAMP_INLINE HttpStatus HttpDenial::status() const {return status_;}

CPPWAMP_INLINE const std::string& HttpDenial::message() const &
{
    return message_;
}

CPPWAMP_INLINE std::string&& HttpDenial::message() &&
{
    return std::move(message_);
}

CPPWAMP_INLINE AdmitResult HttpDenial::result() const {return result_;}

CPPWAMP_INLINE bool HttpDenial::htmlEnabled() const {return htmlEnabled_;}

CPPWAMP_INLINE const HttpFieldMap& HttpDenial::fields() const &
{
    return fields_;
}

CPPWAMP_INLINE HttpFieldMap&& HttpDenial::fields() &&
{
    return std::move(fields_);
}


//******************************************************************************
// HttpResponse
//******************************************************************************

CPPWAMP_INLINE HttpResponse::HttpResponse(HttpStatus status,
                                          const HttpFieldMap& fields)
    : status_(status)
{
    namespace http = boost::beast::http;
    using Response = http::response<http::empty_body>;
    using Serializer = internal::HttpSerializer<Response>;

    Response response;
    response.result(static_cast<http::status>(status));
    for (const auto& kv: fields)
        response.set(kv.first, kv.second);
    serializer_.reset(new Serializer(std::move(response)));
}

CPPWAMP_INLINE HttpStatus HttpResponse::status() const {return status_;}

CPPWAMP_INLINE HttpResponse::HttpResponse(Access, HttpStatus status)
    : status_(status)
{}

CPPWAMP_INLINE void
HttpResponse::setSerializer(internal::HttpSerializerBase* serializer)
{
    serializer_.reset(serializer);
}

template <typename TResponse>
TResponse& HttpResponse::responseAs()
{
    using Serializer = internal::HttpSerializer<TResponse>;
    return dynamic_cast<Serializer&>(*serializer_).response();
}

CPPWAMP_INLINE std::unique_ptr<internal::HttpSerializerBase>&&
HttpResponse::takeSerializer()
{
    return std::move(serializer_);
}


//******************************************************************************
// HttpStringResponse
//******************************************************************************

CPPWAMP_INLINE HttpStringResponse::HttpStringResponse(
    HttpStatus status, std::string body, const HttpFieldMap& fields)
    : Base(Access{}, status)
{
    namespace http = boost::beast::http;
    using Response = http::response<http::string_body>;
    using Serializer = internal::HttpSerializer<Response>;

    Response response;
    response.body() = std::move(body);
    response.result(static_cast<http::status>(status));
    for (const auto& kv: fields)
        response.set(kv.first, kv.second);
    setSerializer(new Serializer(std::move(response)));
}


//******************************************************************************
// HttpFile
//******************************************************************************

struct HttpFile::Impl
{
    boost::beast::http::file_body::value_type file;
};

CPPWAMP_INLINE HttpFile::HttpFile()
    : impl_(new Impl)
{}

CPPWAMP_INLINE std::error_code HttpFile::open(const std::string& filename)
{
    boost::beast::error_code netEc;
    impl_->file.open(filename.c_str(), boost::beast::file_mode::scan, netEc);
    return static_cast<std::error_code>(netEc);
}

CPPWAMP_INLINE void HttpFile::close()
{
    impl_->file.close();
}

CPPWAMP_INLINE bool HttpFile::isOpen() const
{
    return impl_->file.is_open();
}

CPPWAMP_INLINE uint64_t HttpFile::size() const
{
    return impl_->file.size();
}


//******************************************************************************
// HttpFileResponse
//******************************************************************************

CPPWAMP_INLINE HttpFileResponse::HttpFileResponse(
    HttpStatus status, HttpFile&& file, const HttpFieldMap& fields)
    : Base(Access{}, status)
{
    namespace http = boost::beast::http;
    using Response = http::response<http::file_body>;
    using Serializer = internal::HttpSerializer<Response>;

    Response response;
    response.body() = std::move(file.impl_->file);
    for (const auto& kv: fields)
        response.set(kv.first, kv.second);
    setSerializer(new Serializer(std::move(response)));
}

std::error_code HttpFileResponse::open(const std::string& filename)
{
    namespace beast = boost::beast;
    namespace http = boost::beast::http;

    auto& response = responseAs<http::response<http::file_body>>();
    beast::error_code netEc;
    response.body().open(filename.c_str(), beast::file_mode::scan, netEc);
    return static_cast<std::error_code>(netEc);
}

} // namespace wamp
