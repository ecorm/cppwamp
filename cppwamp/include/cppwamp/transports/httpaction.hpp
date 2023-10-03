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
#include <map>
#include <memory>
#include <string>
#include "../api.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TOptions>
class CPPWAMP_API HttpAction
{};

//------------------------------------------------------------------------------
class CPPWAMP_API PolymorphicHttpActionInterface
{
public:
    virtual ~PolymorphicHttpActionInterface() = default;

    virtual void execute(const std::string& target) = 0;
};

//------------------------------------------------------------------------------
template <typename TOptions>
class CPPWAMP_API PolymorphicHttpAction : public PolymorphicHttpActionInterface
{
public:
    using Options = TOptions;

    PolymorphicHttpAction() = default;

    explicit PolymorphicHttpAction(Options options)
        : action_(std::move(options))
    {}

    virtual void execute(const std::string& target) override
    {
        action_.execute(target);
    };

private:
    HttpAction<Options> action_;
};

} // namespace internal

//------------------------------------------------------------------------------
/** Wrapper that type-erases a polymorphic HTTP action. */
//------------------------------------------------------------------------------
class CPPWAMP_API AnyHttpAction
{
public:
    /** Constructs an empty AnyCodec. */
    AnyHttpAction() = default;

    /** Converting constructor taking action options. */
    template <typename TOptions>
    AnyHttpAction( // NOLINT(google-explicit-constructor)
        TOptions o)
        : action_(std::make_shared<internal::PolymorphicHttpAction<TOptions>>(
            std::move(o)))
    {}

    /** Returns false if the AnyHttpAction is empty. */
    explicit operator bool() const {return action_ != nullptr;}

private:
    void execute(const std::string& target)
    {
        assert(action_ != nullptr);
        action_->execute(target);
    };

    std::shared_ptr<internal::PolymorphicHttpActionInterface> action_;
};


//------------------------------------------------------------------------------
/** Options for serving static files via HTTP. */
//------------------------------------------------------------------------------
class HttpServeStaticFile
{
public:
    explicit HttpServeStaticFile(std::string path) : path_(std::move(path)) {}

private:
    std::string path_;
};

//------------------------------------------------------------------------------
/** Options for upgrading an HTTP request to a Websocket connection. */
//------------------------------------------------------------------------------
class HttpWebsocketUpgrade
{
public:
    /** Specifies the maximum length permitted for incoming messages. */
    HttpWebsocketUpgrade& withMaxRxLength(std::size_t length)
    {
        maxRxLength_ = length;
        return *this;
    }

    /** Obtains the specified maximum incoming message length. */
    std::size_t maxRxLength() const {return maxRxLength_;}

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
    HttpAction(HttpServeStaticFile options) : options_(options) {}

    void execute(const std::string& target)
    {
    };

private:
    HttpServeStaticFile options_;
};

//------------------------------------------------------------------------------
template <>
class HttpAction<HttpWebsocketUpgrade>
{
public:
    HttpAction(HttpWebsocketUpgrade options) : options_(options) {}

    void execute(const std::string& target)
    {
    };

private:
    HttpWebsocketUpgrade options_;
};

} // namespace internal

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "../internal/httpaction.inl.hpp"
#endif

#endif // CPPWAMP_TRANSPORTS_HTTPACTION_HPP
