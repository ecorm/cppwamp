/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORTS_HTTPACTION_HPP
#define CPPWAMP_TRANSPORTS_HTTPACTION_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for specifying HTTP server parameters and
           options. */
//------------------------------------------------------------------------------

#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include "../api.hpp"
#include "../internal/polymorphichttpaction.hpp"

namespace wamp
{

namespace internal { class HttpJob; }

//------------------------------------------------------------------------------
/** Wrapper that type-erases a polymorphic HTTP action. */
//------------------------------------------------------------------------------
class CPPWAMP_API AnyHttpAction
{
public:
    /** Constructs an empty AnyHttpAction. */
    AnyHttpAction();

    /** Converting constructor taking action options. */
    template <typename TOptions>
    AnyHttpAction(TOptions o) // NOLINT(google-explicit-constructor)
        : action_(std::make_shared<internal::PolymorphicHttpAction<TOptions>>(
            std::move(o)))
    {}

    /** Returns false if the AnyHttpAction is empty. */
    explicit operator bool() const;

    template <typename OptionsType>
    bool is() const
    {
        using Derived = internal::PolymorphicHttpAction<OptionsType>;
        return std::dynamic_pointer_cast<Derived>(action_) != nullptr;
    }

private:
    void execute(internal::HttpJob& job);

    std::shared_ptr<internal::PolymorphicHttpActionInterface> action_;

    friend class internal::HttpJob;
};


//------------------------------------------------------------------------------
/** Options for serving static files via HTTP. */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpServeStaticFile
{
public:
    using MimeTypeMapper = std::function<std::string (const std::string&)>;

    /** Constructor taking a path to the document root. */
    explicit HttpServeStaticFile(std::string documentRoot);

    /** Specifies the mapping function for determining MIME type based on
        file extension. */
    HttpServeStaticFile& withMimeTypes(MimeTypeMapper f);

    /** Obtains the path to the document root. */
    std::string documentRoot() const;

    /** Obtains the MIME type associated with the given path. */
    std::string lookupMimeType(std::string extension);

private:
    static char toLower(char c);

    std::string defaultMimeType(const std::string& extension);

    std::string documentRoot_;
    MimeTypeMapper mimeTypeMapper_;
};

//------------------------------------------------------------------------------
/** Options for upgrading an HTTP request to a Websocket connection. */
//------------------------------------------------------------------------------
class CPPWAMP_API HttpWebsocketUpgrade
{
public:
    /** Specifies the maximum length permitted for incoming messages. */
    HttpWebsocketUpgrade& withMaxRxLength(std::size_t length);

    /** Obtains the specified maximum incoming message length. */
    std::size_t maxRxLength() const;

private:
    std::size_t maxRxLength_ = 16*1024*1024;
};


namespace internal
{

//------------------------------------------------------------------------------
template <>
class HttpAction<HttpServeStaticFile>
{
public:
    HttpAction(HttpServeStaticFile options);

    void execute(HttpJob& job);

private:
    HttpServeStaticFile options_;
};

//------------------------------------------------------------------------------
template <>
class HttpAction<HttpWebsocketUpgrade>
{
public:
    HttpAction(HttpWebsocketUpgrade options);

    void execute(HttpJob& job);

private:
    HttpWebsocketUpgrade options_;
};

} // namespace internal

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/httpaction.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_HTTPACTION_HPP
