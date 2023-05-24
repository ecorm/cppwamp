/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TIMEFORMATTING_HPP
#define CPPWAMP_INTERNAL_TIMEFORMATTING_HPP

#include <chrono>
#include <ctime>
#include <iomanip>
#include <ostream>
#include <sstream>

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
inline std::ostream& outputRfc3339TimestampInMilliseconds(
    std::ostream& out, std::chrono::system_clock::time_point when)
{
    namespace chrono = std::chrono;
    auto time = chrono::system_clock::to_time_t(when);

#if _POSIX_C_SOURCE >= 1 || _XOPEN_SOURCE || _BSD_SOURCE || _SVID_SOURCE || \
    _POSIX_SOURCE
    std::tm tmbResult;
    std::tm* tmb = ::gmtime_r(&time, &tmbResult);
#else
    std::tm* tmb = std::gmtime(&time);
#endif
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

//------------------------------------------------------------------------------
inline std::string toRfc3339TimestampInMilliseconds(
    std::chrono::system_clock::time_point when)
{
    std::ostringstream oss;
    outputRfc3339TimestampInMilliseconds(oss, when);
    return oss.str();
}

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TIMEFORMATTING_HPP
