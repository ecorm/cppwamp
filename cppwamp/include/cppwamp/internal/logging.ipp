/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../logging.hpp"
#include <cassert>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>
#include "../api.hpp"


namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::ostream& outputLogEntryTime(
    std::ostream& out, std::chrono::system_clock::time_point when)
{
    namespace chrono = std::chrono;
    auto time = chrono::system_clock::to_time_t(when);
    std::tm* tmb = std::gmtime(&time);
    auto d = when.time_since_epoch();
    auto mins = chrono::duration_cast<chrono::minutes>(d);
    auto remainder1 = (when - mins).time_since_epoch();
    auto secs = chrono::duration_cast<chrono::seconds>(remainder1);
    auto remainder2 = remainder1 - secs;
    auto ms = chrono::duration_cast<chrono::milliseconds>(remainder2);

    out << std::put_time(tmb, "%FT%H:%M:")
        << std::setfill('0') << std::setw(2) << secs.count()
        << '.' << std::setw(3) << ms.count() << 'Z';
    return out;
}

CPPWAMP_INLINE void outputAccessLogEntry(
    std::ostream& out, const AccessLogEntry& entry, std::string origin,
    bool colored)
{
    static constexpr const char* red = "\x1b[1;31m";
    static constexpr const char* plain = "\x1b[0m";

    struct PutField
    {
        std::ostream& out;

        PutField& operator<<(const std::string& field)
        {
            if (field.empty())
                out << " | -";
            else
                out << " | " << field;
            return *this;
        }
    };

    const auto& s = entry.session();
    const auto& a = entry.action();
    AccessLogEntry::outputTime(out, entry.when());
    PutField{out} << s.serverName;
    out << " | " << s.serverSessionIndex;
    PutField{out} << s.endpoint << s.realmUri << s.authId
                  << s.wampSessionIdHash << s.agent;
    if (a.requestId == nullId())
        out << " | -";
    else
        out << " | " << a.requestId;

    PutField{out} << a.action << a.target;

    out << " | ";
    if (entry.action().errorUri.empty())
        out << "-";
    else if (colored)
        out << red << entry.action().errorUri << plain;
    else
        out << entry.action().errorUri;

    out << " | " << a.options;
}

}

//******************************************************************************
// LogLevel
//******************************************************************************

//------------------------------------------------------------------------------
/** @relates LogLevel */
//------------------------------------------------------------------------------
CPPWAMP_INLINE const std::string& logLevelLabel(LogLevel lv)
{
    static const std::string labels[] =
    {
        "trace",
        "debug",
        "info",
        "warning",
        "error",
        "critical",
        "off"
    };

    assert(lv <= LogLevel::off);
    return labels[static_cast<unsigned>(lv)];
}


//******************************************************************************
// LogEntry
//******************************************************************************

//------------------------------------------------------------------------------
/** @details
    The following format is used:
    ```
    YYYY-MM-DDTHH:MM:SS.sss
    ```
    @note This function uses std::gmtime which may or may not be thread-safe
          on the target platform. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::ostream& LogEntry::outputTime(std::ostream& out,
                                                  TimePoint when)
{
    return internal::outputLogEntryTime(out, when);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE LogEntry::LogEntry(LogLevel severity, std::string message,
                                  std::error_code ec)
    : message_(std::move(message)),
      ec_(ec),
      when_(std::chrono::system_clock::now()),
      severity_(severity)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE LogLevel LogEntry::severity() const {return severity_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const std::string& LogEntry::message() const & {return message_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::string&& LogEntry::message() && {return std::move(message_);}

//------------------------------------------------------------------------------
CPPWAMP_INLINE LogEntry& LogEntry::append(std::string extra)
{
    message_ += std::move(extra);
    return *this;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const std::error_code& LogEntry::error() const {return ec_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE LogEntry::TimePoint LogEntry::when() const {return when_;}

//------------------------------------------------------------------------------
/** @relates LogEntry
    @details
    The following format is used:
    ```
    YYYY-MM-DDTHH:MM:SS.sss | origin | level | message | error code info
    ```
    @note This function uses std::gmtime which may or may not be thread-safe
          on the target platform. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::string toString(const LogEntry& entry)
{
    return toString(entry, "cppwamp");
}

//------------------------------------------------------------------------------
/** @relates LogEntry
    @copydetails toString(const LogEntry&) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::string toString(const LogEntry& entry,
                                    const std::string& origin)
{
    std::ostringstream oss;
    toStream(oss, entry, origin);
    return oss.str();
}

//------------------------------------------------------------------------------
/** @relates LogEntry
    @copydetails toString(const LogEntry&) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::ostream& toStream(std::ostream& out, const LogEntry& entry)
{
    return toStream(out, entry, "cppwamp");
}

//------------------------------------------------------------------------------
/** @relates LogEntry
    @copydetails toString(const LogEntry&) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::ostream& toStream(std::ostream& out, const LogEntry& entry,
                                      const std::string& origin)
{
    static constexpr const char* sep = " | ";

    LogEntry::outputTime(out, entry.when());
    out << sep << origin << sep << logLevelLabel(entry.severity())
        << sep << entry.message();

    auto ec = entry.error();
    if (ec)
        out << sep << ec << " (" << ec.message() << ")";
    else
        out << sep << '-';

    return out;
}

//------------------------------------------------------------------------------
/** @relates LogEntry
    @copydetails toString(const LogEntry&) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::ostream& toColorStream(std::ostream& out,
                                           const LogEntry& entry)
{
    return toColorStream(out, entry, "cppwamp");
}

//------------------------------------------------------------------------------
/** @relates LogEntry
    @copydetails toString(const LogEntry&) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::ostream&
    toColorStream(std::ostream& out, const LogEntry& entry, std::string origin)
{
    static constexpr const char* sep = " | ";
    static constexpr const char* red = "\x1b[1;31m";
    static constexpr const char* green = "\x1b[1;32m";
    static constexpr const char* yellow = "\x1b[1;33m";
    static constexpr const char* plain = "\x1b[0m";

    const char* color = nullptr;
    switch (entry.severity())
    {
    case LogLevel::info:
        color = green;
        break;

    case LogLevel::warning:
        color = yellow;
        break;

    case LogLevel::error:
    case LogLevel::critical:
        color = red;
        break;

    default:
        color = nullptr;
        break;
    }

    LogEntry::outputTime(out, entry.when());
    out << sep << origin << sep;

    if (color != nullptr)
    {
        out << color;
        out << logLevelLabel(entry.severity());
        out << plain;
    }
    else
    {
        out << logLevelLabel(entry.severity());
    }

    out << sep << entry.message();

    auto ec = entry.error();
    if (ec)
        out << sep << ec << " (" << ec.message() << ")";
    else
        out << sep << '-';

    return out;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::ostream& operator<<(std::ostream& out,
                                        const LogEntry& entry)
{
    return toStream(out, entry);
}

//******************************************************************************
// AccessActionInfo
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessActionInfo::AccessActionInfo() {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessActionInfo::AccessActionInfo(
    std::string action, std::string target, Object options,
    std::string errorUri)
    : AccessActionInfo(nullId(), std::move(action), std::move(target),
                       std::move(options), std::move(errorUri))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessActionInfo::AccessActionInfo(
    std::string action, std::string target, Object options, std::error_code ec)
    : AccessActionInfo(nullId(), std::move(action), std::move(target),
                       std::move(options), ec)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessActionInfo::AccessActionInfo(
    std::string action, std::string target, Object options, SessionErrc errc)
    : AccessActionInfo(nullId(), std::move(action), std::move(target),
                       std::move(options), errc)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessActionInfo::AccessActionInfo(
    RequestId r, std::string action, std::string target, Object options,
    std::string errorUri)
    : action(std::move(action)),
      target(std::move(target)),
      errorUri(std::move(errorUri)),
      options(std::move(options)),
      requestId(r)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessActionInfo::AccessActionInfo(
    RequestId r, std::string action, std::string target, Object options,
    std::error_code ec)
    : AccessActionInfo(r, std::move(action), std::move(target),
                       std::move(options), toErrorUri(ec))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessActionInfo::AccessActionInfo(
    RequestId r, std::string action, std::string target, Object options,
    SessionErrc errc)
    : AccessActionInfo(r, std::move(action), std::move(target),
                       std::move(options), make_error_code(errc))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessActionInfo&
AccessActionInfo::withErrorUri(std::string uri)
{
    errorUri = std::move(uri);
    return *this;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessActionInfo& AccessActionInfo::withError(std::error_code ec)
{
    return withErrorUri(toErrorUri(ec));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessActionInfo& AccessActionInfo::withError(SessionErrc errc)
{
    return withError(make_error_code(errc));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::string AccessActionInfo::toErrorUri(std::error_code ec)
{
    std::ostringstream oss;
    if (ec)
        oss << ec << " (" << ec.message() << ")";
    return oss.str();
}


//******************************************************************************
// AccessLogEntry
//******************************************************************************

//------------------------------------------------------------------------------
/** @copydetails LogEntry::outputTime */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::ostream& AccessLogEntry::outputTime(std::ostream& out,
                                                        TimePoint when)
{
    return internal::outputLogEntryTime(out, when);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessLogEntry::AccessLogEntry(AccessSessionInfo session,
                                              AccessActionInfo action)
    : session_(std::move(session)),
      action_(std::move(action)),
      when_(std::chrono::system_clock::now())
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const AccessSessionInfo& AccessLogEntry::session() const
{
    return session_;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const AccessActionInfo& AccessLogEntry::action() const
{
    return action_;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessLogEntry::TimePoint AccessLogEntry::when() const
{
    return when_;
}

//------------------------------------------------------------------------------
/** @relates AccessLogEntry
    @details
    The following format is used:
    ```
    YYYY-MM-DDTHH:MM:SS.sss | server name | server session index |
    transport endpoint | realmUri | authid | wamp session id hash | agent |
    action | target URI | ok/error | status | {action options}
    ```
    @note This function uses std::gmtime which may or may not be thread-safe
          on the target platform. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::string toString(const AccessLogEntry& entry)
{
    return toString(entry, "cppwamp");
}

//------------------------------------------------------------------------------
/** @relates AccessLogEntry
    @copydetails toString(const AccessLogEntry&) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::string toString(const AccessLogEntry& entry,
                                    const std::string& origin)
{
    std::ostringstream oss;
    toStream(oss, entry, origin);
    return oss.str();
}

//------------------------------------------------------------------------------
/** @relates AccessLogEntry
    @copydetails toString(const AccessLogEntry&) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::ostream& toStream(std::ostream& out,
                                      const AccessLogEntry& entry)
{
    return toStream(out, entry, "cppwamp");
}

//------------------------------------------------------------------------------
/** @relates AccessLogEntry
    @copydetails toString(const AccessLogEntry&) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::ostream& toStream(std::ostream& out,
                                      const AccessLogEntry& entry,
                                      const std::string& origin)
{
    internal::outputAccessLogEntry(out, entry, origin, false);
    return out;
}

//------------------------------------------------------------------------------
/** @relates AccessLogEntry
    @copydetails toString(const AccessLogEntry&) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::ostream& toColorStream(std::ostream& out,
                                           const AccessLogEntry& entry)
{
    return toColorStream(out, entry, "cppwamp");
}

//------------------------------------------------------------------------------
/** @relates AccessLogEntry
    @copydetails toString(const AccessLogEntry&) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::ostream&
toColorStream(std::ostream& out, const AccessLogEntry& entry,
              std::string origin)
{
    internal::outputAccessLogEntry(out, entry, origin, true);
    return out;
}


//------------------------------------------------------------------------------
CPPWAMP_INLINE std::ostream& operator<<(std::ostream& out,
                                        const AccessLogEntry& entry)
{
    return toStream(out, entry);
}

} // namespace wamp
