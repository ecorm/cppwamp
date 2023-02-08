/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_LOGGING_HPP
#define CPPWAMP_LOGGING_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for logging. */
//------------------------------------------------------------------------------

#include <chrono>
#include <functional>
#include <ostream>
#include <set>
#include <string>
#include "api.hpp"
#include "erroror.hpp"
#include "variant.hpp"
#include "wampdefs.hpp"


namespace wamp
{

//------------------------------------------------------------------------------
/** Log entry severity levels.
    These match the levels of the popular
    [gabime/spdlog](https://github.com/gabime/spdlog) library. */
//------------------------------------------------------------------------------
enum class CPPWAMP_API LogLevel
{
    trace,    ///< For sent/received WAMP message dumps
    debug,    ///< Not yet used
    info,     ///< Not yet used
    warning,  ///< For detected problems that don't affect normal operation
    error,    ///< For failures where operations cannot be completed
    critical, ///< Not yet used
    off       ///< Used to disable all log events
};

//------------------------------------------------------------------------------
const std::string& logLevelLabel(LogLevel lv);

//------------------------------------------------------------------------------
/** Contains logging information. */
//------------------------------------------------------------------------------
class CPPWAMP_API LogEntry
{
public:
    using TimePoint = std::chrono::system_clock::time_point;

    /** Outputs a timestamp in RFC3339 format. */
    static std::ostream& outputTime(std::ostream& out, TimePoint when);

    /** Constructor. */
    LogEntry(LogLevel severity, std::string message, std::error_code ec = {});

    /** Obtains the entry's severity level. */
    LogLevel severity() const;

    /** Obtains the entry's information text. */
    const std::string& message() const &;

    /** Moves the entry's information text. */
    std::string&& message() &&;

    /** Appends the given text to the entry's information text. */
    LogEntry& append(std::string extra);

    /** Obtains the error code associated with this entry, if applicable. */
    const std::error_code& error() const;

    /** Obtains the entry's timestamp. */
    TimePoint when() const;

private:
    std::string message_;
    std::error_code ec_;
    TimePoint when_;
    LogLevel severity_ = LogLevel::off;
};

/** Obtains a formatted log entry string combining all available information.
    @relates LogEntry */
std::string toString(const LogEntry& entry);

/** Obtains a formatted log entry string with a custom origin field.
    @relates LogEntry */
std::string toString(const LogEntry& entry, const std::string& origin);

/** Outputs a formatted log entry combining all available information.
    @relates LogEntry */
std::ostream& toStream(std::ostream& out, const LogEntry& entry);

/** Outputs a formatted log entry with a custom origin field.
    @relates LogEntry */
std::ostream& toStream(std::ostream& out, const LogEntry& entry,
                       const std::string& origin);

/** Outputs a formatted log entry using ANSI color escape codes.
    @relates LogEntry */
std::ostream& toColorStream(std::ostream& out, const LogEntry& entry);

/** Outputs a formatted, colored log entry with a custom origin field.
    @relates LogEntry */
std::ostream& toColorStream(std::ostream& out, const LogEntry& entry,
                            std::string origin);

/** Outputs a LogEntry to an output stream.
    @relates LogEntry */
std::ostream& operator<<(std::ostream& out, const LogEntry& entry);


// TODO: Move below to separate module so that client-only users don't
// import it.

//------------------------------------------------------------------------------
struct CPPWAMP_API AccessSessionInfo
{
    std::string endpoint;
    std::string serverName;
    std::string realmUri;
    std::string authId;
    std::string scrambledWampSessionId;
    std::string agent;
    uint64_t serverSessionIndex;
    SessionId wampSessionId;
};

//------------------------------------------------------------------------------
struct CPPWAMP_API AccessActionInfo
{
    AccessActionInfo();

    AccessActionInfo(std::string action, std::string target = {},
                     Object options = {}, std::string errorUri = {});

    AccessActionInfo(std::string action, std::string target,
                     Object options, std::error_code ec);

    AccessActionInfo(std::string action, std::string target,
                     Object options, SessionErrc errc);

    AccessActionInfo(RequestId r, std::string action, std::string target = {},
                     Object options = {}, std::string errorUri = {});

    AccessActionInfo(RequestId r, std::string action, std::string target,
                     Object options, std::error_code ec);

    AccessActionInfo(RequestId r, std::string action, std::string target,
                     Object options, SessionErrc errc);

    template <typename T>
    AccessActionInfo(std::string action, std::string target,
                     Object options, const ErrorOr<T>& x)
        : AccessActionInfo(std::move(action), std::move(target),
                           std::move(options), toErrorUri(x))
    {}

    AccessActionInfo& withErrorUri(std::string uri);

    AccessActionInfo& withError(std::error_code ec);

    AccessActionInfo& withError(SessionErrc errc);

    template <typename T>
    AccessActionInfo& withResult(const ErrorOr<T>& x)
    {
        if (!x)
            withError(x.error());
        return *this;
    }

    std::string name;
    std::string target;
    std::string errorUri;
    Object options;
    RequestId requestId = nullId();

private:
    static std::string toErrorUri(std::error_code ec);

    template <typename T>
    static std::string toErrorUri(const ErrorOr<T>& x)
    {
        return !x ? std::string{} : toErrorUri(x.error());
    }
};


//------------------------------------------------------------------------------
/** Contains access logging information. */
//------------------------------------------------------------------------------
struct CPPWAMP_API AccessLogEntry
{
    using TimePoint = std::chrono::system_clock::time_point;

    /** Outputs a timestamp in RFC3339 format. */
    static std::ostream& outputTime(std::ostream& out, TimePoint when);

    /** Constructor. */
    AccessLogEntry(AccessSessionInfo session, AccessActionInfo action);

    /** The session information. */
    AccessSessionInfo session;

    /** The action information. */
    AccessActionInfo action;

    /** Timestamp. */
    TimePoint when;
};

/** Obtains a formatted log entry string combining all available information.
    @relates AccessLogEntry */
std::string toString(const AccessLogEntry& entry);

/** Obtains a formatted log entry string with a custom origin field.
    @relates AccessLogEntry */
std::string toString(const AccessLogEntry& entry, const std::string& origin);

/** Outputs a formatted log entry combining all available information.
    @relates AccessLogEntry */
std::ostream& toStream(std::ostream& out, const AccessLogEntry& entry);

/** Outputs a formatted log entry with a custom origin field.
    @relates AccessLogEntry */
std::ostream& toStream(std::ostream& out, const AccessLogEntry& entry,
                       const std::string& origin);

/** Outputs a formatted access log entry using ANSI color escape codes.
    @relates AccessLogEntry */
std::ostream& toColorStream(std::ostream& out, const AccessLogEntry& entry);

/** Outputs a formatted, colored access log entry with a custom origin field.
    @relates AccessLogEntry */
std::ostream& toColorStream(std::ostream& out, const AccessLogEntry& entry,
                            std::string origin);

/** Outputs a AccessLogEntry to an output stream.
    @relates AccessLogEntry */
std::ostream& operator<<(std::ostream& out, const AccessLogEntry& entry);


//------------------------------------------------------------------------------
/** Function type for filtering entries in the access log.
    If the function returns false, the entry will not be logged.
    The log entry is passed by reference so that the function may
    sanitize any of the fields. */
//------------------------------------------------------------------------------
using AccessLogFilter = std::function<bool (AccessLogEntry&)>;

//------------------------------------------------------------------------------
/// Default access log filter function used by the router if none is specified.
//------------------------------------------------------------------------------
struct DefaultAccessLogFilter
{
    static const std::set<String>& bannedOptions();

    bool operator()(AccessLogEntry& entry) const;
};

//------------------------------------------------------------------------------
/** Access log filter that allows everything and does not touch the entries.
    May be used to allow the filtering logic to be performed by a custom
    log handler. */
//------------------------------------------------------------------------------
struct AccessLogPassthruFilter
{
    bool operator()(AccessLogEntry&) const {return true;}
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/logging.ipp"
#endif

#endif // CPPWAMP_LOGGING_HPP_HPP
