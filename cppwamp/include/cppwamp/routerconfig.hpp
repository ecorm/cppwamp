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

namespace wamp
{

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
