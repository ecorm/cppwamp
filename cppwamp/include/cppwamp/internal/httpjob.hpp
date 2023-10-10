/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_HTTPJOB_HPP
#define CPPWAMP_INTERNAL_HTTPJOB_HPP

#include <initializer_list>
#include <boost/beast/core/string_type.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/string_body.hpp>
#include "../transport.hpp"
#include "../transports/httpprotocol.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
class HttpJob
{
public:
    using Body = boost::beast::http::string_body;
    using Request = boost::beast::http::request<Body>;
    using Field = boost::beast::http::field;
    using StringView = boost::beast::string_view;
    using AnyMessage = boost::beast::http::message_generator;
    using FieldList = std::initializer_list<std::pair<Field, StringView>>;

    virtual ~HttpJob() = default;

    const Request& request() const {return request_;}

    const HttpEndpoint& settings() const {return *settings_;}

    void respond(AnyMessage response);

    void upgrade(Transporting::Ptr transport, int codecId);

    void balk(
        HttpStatus status, std::string what = {}, bool simple = false,
        FieldList fields = {})
    {
        doBalk(status, std::move(what), simple, fields);
    }

protected:
    using SettingsPtr = std::shared_ptr<HttpEndpoint>;

    HttpJob(SettingsPtr s) : settings_(std::move(s)) {}

    virtual void doRespond(AnyMessage response) = 0;

    virtual void doUpgrade(Transporting::Ptr transport, int codecId) = 0;

    virtual void doBalk(
        HttpStatus status, std::string what, bool simple,
        std::initializer_list<std::pair<Field, StringView>> fields) = 0;

    void setRequest(Request&& req) {request_ = std::move(req);}

private:
    Request request_;
    SettingsPtr settings_;
};

//------------------------------------------------------------------------------
inline std::string httpStaticFilePath(boost::beast::string_view base,
                                      boost::beast::string_view path)
{
    if (base.empty())
        return std::string(path);

#ifdef _WIN32
    constexpr char separator = '\\';
#else
    constexpr char separator = '/';
#endif

    std::string result{base};
    if (result.back() == separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());

#ifdef _WIN32
    for (auto& c: result)
    {
        if (c == '/')
            c = separator;
    }
#endif
    return result;
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_HTTPJOB_HPP
