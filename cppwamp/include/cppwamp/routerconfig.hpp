/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ROUTERCONFIG_HPP
#define CPPWAMP_ROUTERCONFIG_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the API used by a _router_ peer in WAMP applications. */
//------------------------------------------------------------------------------

#include "anyhandler.hpp"
#include "api.hpp"
#include "logging.hpp"
#include "wampdefs.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
struct CPPWAMP_API AuthorizationInfo
{
public:
    AuthorizationInfo() = default;

    AuthorizationInfo(std::string realmUri, std::string authRole,
                      std::string authId)
        : realmUri_(std::move(realmUri)),
          authRole_(std::move(authRole)),
          authId_(std::move(authId))
    {}

    void setSessionId(SessionId id) {sessionId_ = id;}

    const std::string& realmUri() const {return realmUri_;}

    const std::string& authRole() const {return authRole_;}

    const std::string& authId() const {return authId_;}

    SessionId sessionId() const {return sessionId_;}

private:
    std::string realmUri_;
    std::string authRole_;
    std::string authId_;
    SessionId sessionId_ = 0;
};

//------------------------------------------------------------------------------
class CPPWAMP_API RouterConfig
{
public:
    using LogHandler = AnyReusableHandler<void (LogEntry)>;

    RouterConfig& withLogHandler(LogHandler f)
    {
        logHandler_ = std::move(f);
        return *this;
    }

    RouterConfig& withLogLevel(LogLevel l)
    {
        logLevel_ = l;
        return *this;
    }

    const LogHandler& logHandler() const {return logHandler_;}

    LogLevel logLevel() const {return logLevel_;}

private:
    LogHandler logHandler_;
    LogLevel logLevel_ = LogLevel::warning;
};

} // namespace wamp

#endif // CPPWAMP_ROUTERCONFIG_HPP
