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

//------------------------------------------------------------------------------
CPPWAMP_INLINE LogEntry::LogEntry(LogLevel severity, std::string info,
                                  std::error_code ec)
    : info_(std::move(info)),
      ec_(ec),
      when_(std::chrono::system_clock::now()),
      severity_(severity)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE LogLevel LogEntry::severity() const {return severity_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const std::string& LogEntry::message() const & {return info_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::string&& LogEntry::message() && {return std::move(info_);}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const std::error_code& LogEntry::error() const {return ec_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE LogEntry::TimePoint LogEntry::when() const {return when_;}

//------------------------------------------------------------------------------
/** @details
    The following format is used:
    ```
    [YYYY-MM-DDTHH:MM:SS.sss] [origin] [level] message (optional error code info)
    ```
    @note This function uses std::gmtime which may or may not be thread-safe
          on the target platform. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::string LogEntry::formatted(std::string origin) const
{
    namespace chrono = std::chrono;
    auto time = chrono::system_clock::to_time_t(when_);
    std::tm* tmb = std::gmtime(&time);
    auto d = when_.time_since_epoch();
    auto mins = chrono::duration_cast<chrono::minutes>(d);
    auto remainder1 = (when_ - mins).time_since_epoch();
    auto secs = chrono::duration_cast<chrono::seconds>(remainder1);
    auto remainder2 = remainder1 - secs;
    auto ms = chrono::duration_cast<chrono::milliseconds>(remainder2);

    std::ostringstream oss;
    oss << std::put_time(tmb, "[%FT%H:%M:")
        << std::setfill('0') << std::setw(2) << secs.count()
        << '.' << std::setw(3) << ms.count() << "Z] ["
        << origin << "] ["
        << logLevelLabel(severity_) << "] "
        << info_;

    if (ec_)
        oss << " (with error code " << ec_ << " '" << ec_.message() << "')";
    return oss.str();
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::ostream& operator<<(std::ostream& out,
                                        const LogEntry& entry)
{
    out << entry.formatted();
    return out;
}

} // namespace wamp


//#ifndef CPPWAMP_COMPILED_LIB
//#include "internal/logging.ipp"
//#endif
