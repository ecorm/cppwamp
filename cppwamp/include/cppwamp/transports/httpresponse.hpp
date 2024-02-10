/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2024.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_HTTPRESPONSE_HPP
#define CPPWAMP_TRANSPORTS_HTTPRESPONSE_HPP

#include <map>
#include "../api.hpp"
#include "../transport.hpp"
#include "httpstatus.hpp"
#include "../internal/httpserializer.hpp"

namespace wamp
{

namespace internal { class HttpJobImpl; }


//------------------------------------------------------------------------------
using HttpFieldMap = std::map<std::string, std::string>;


//------------------------------------------------------------------------------
class CPPWAMP_API HttpDenial
{
public:
    HttpDenial(HttpStatus status);

    HttpDenial& setStatus(HttpStatus status);

    HttpDenial& withMessage(std::string what);

    HttpDenial& withResult(AdmitResult result);

    HttpDenial withHtmlEnabled(bool enabled = true);

    HttpDenial& withFields(HttpFieldMap fields);

    HttpStatus status() const;

    const std::string& message() const &;

    std::string&& message() &&;

    AdmitResult result() const;

    bool htmlEnabled() const;

    const HttpFieldMap& fields() const &;

    HttpFieldMap&& fields() &&;

private:
    HttpFieldMap fields_;
    std::string message_;
    AdmitResult result_;
    HttpStatus status_ = HttpStatus::none;
    bool htmlEnabled_ = false;
};


//------------------------------------------------------------------------------
class CPPWAMP_API HttpResponse
{
public:
    explicit HttpResponse(HttpStatus status, const HttpFieldMap& fields = {});

    HttpStatus status() const;

protected:
    struct Access {};

    HttpResponse(Access, HttpStatus status);

    void setSerializer(internal::HttpSerializerBase* serializer);

    template <typename TResponse>
    TResponse& responseAs();

private:
    using SerializerPtr = std::unique_ptr<internal::HttpSerializerBase>;

    SerializerPtr&& takeSerializer();

    SerializerPtr serializer_;
    HttpStatus status_;

    friend class internal::HttpJobImpl;
};


//------------------------------------------------------------------------------
class CPPWAMP_API HttpStringResponse : public HttpResponse
{
public:
    HttpStringResponse(HttpStatus status, std::string body,
                       const HttpFieldMap& fields = {});

private:
    using Base = HttpResponse;
};


//------------------------------------------------------------------------------
class CPPWAMP_API HttpFile
{
public:
    HttpFile();

    std::error_code open(const std::string& filename);

    void close();

    bool isOpen() const;

    std::uint64_t size() const;

private:
    struct Impl;

    std::unique_ptr<Impl> impl_;

    friend class HttpFileResponse;
};


//------------------------------------------------------------------------------
class CPPWAMP_API HttpFileResponse : public HttpResponse
{
public:
    HttpFileResponse(HttpStatus status, HttpFile&& file,
                     const HttpFieldMap& fields = {});

    std::error_code open(const std::string& filename);

private:
    using Base = HttpResponse;

    friend class HttpJob;
};
} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/httpresponse.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_HTTPRESPONSE_HPP
