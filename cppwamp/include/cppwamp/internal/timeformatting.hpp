/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TIMEFORMATTING_HPP
#define CPPWAMP_INTERNAL_TIMEFORMATTING_HPP

// TODO: Test

#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <istream>
#include <ostream>
#include <sstream>

#if _DEFAULT_SOURCE || _BSD_SOURCE || _SVID_SOURCE
    #define CPPWAMP_HAS_TIMEGM
#endif

#if _POSIX_C_SOURCE >= 1 || _XOPEN_SOURCE || _BSD_SOURCE || _SVID_SOURCE || \
    _POSIX_SOURCE
    #define CPPWAMP_HAS_GMTIME_R
#endif

namespace wamp
{

namespace internal
{

template <unsigned Precision>
struct Rfc3339Subseconds
{};

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
template <> struct Rfc3339Subseconds<0> {using Type = std::chrono::seconds;};
template <> struct Rfc3339Subseconds<3> {using Type = std::chrono::milliseconds;};
template <> struct Rfc3339Subseconds<6> {using Type = std::chrono::microseconds;};
template <> struct Rfc3339Subseconds<9> {using Type = std::chrono::nanoseconds;};
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

//------------------------------------------------------------------------------
template <unsigned Precision>
std::ostream& outputRfc3339Timestamp(
    std::ostream& out, std::chrono::system_clock::time_point when)
{
#ifndef CPPWAMP_ALLOW_32_BIT_TIME_T
    static_assert(sizeof(std::time_t) >= sizeof(int64_t),
                  "std::time_t is 32-bit and will overflow in 2038");
#endif

    namespace chrono = std::chrono;
    using SubSeconds = typename Rfc3339Subseconds<Precision>::Type;
    auto time = chrono::system_clock::to_time_t(when);

#ifdef CPPWAMP_HAS_GMTIME_R
    std::tm tmbResult; // NOLINT(cppcoreguidelines-pro-type-member-init)
    std::memset(&tmbResult, 0, sizeof(tmbResult));
    std::tm* tmb = ::gmtime_r(&time, &tmbResult);
#else
    std::tm* tmb = std::gmtime(&time);
#endif
    auto d = when.time_since_epoch();
    auto mins = chrono::duration_cast<chrono::minutes>(d);
    auto remainder1 = (when - mins).time_since_epoch();
    auto secs = chrono::duration_cast<chrono::seconds>(remainder1);
    auto remainder2 = remainder1 - secs;
    auto ms = chrono::duration_cast<SubSeconds>(remainder2);

    auto locale = out.getloc();
    out.imbue(std::locale::classic());
    out << std::put_time(tmb, "%FT%H:%M:")
        << std::setfill('0') << std::setw(2) << secs.count();
    if (Precision != 0)
        out << '.' << std::setw(Precision) << ms.count();
    out << 'Z';
    out.imbue(locale);
    return out;
}

//------------------------------------------------------------------------------
template <unsigned Precision>
std::string toRfc3339Timestamp(std::chrono::system_clock::time_point when)
{
    std::ostringstream oss;
    outputRfc3339Timestamp<Precision>(oss, when);
    return oss.str();
}

//------------------------------------------------------------------------------
inline std::istream& inputRfc3339Timestamp(
    std::istream& in, std::chrono::system_clock::time_point& when)
{
#ifdef CPPWAMP_HAS_TIMEGM
    #ifndef CPPWAMP_ALLOW_32_BIT_TIME_T
    static_assert(sizeof(std::time_t) >= sizeof(int64_t),
                  "std::time_t is 32-bit and will overflow in 2038");
    #endif
    struct SplitTmToTime
    {
        using TickType = std::time_t;

        static TickType toTime(std::tm& tmb)
        {
            return ::timegm(&tmb);
        };
    };
#elif defined (_MSC_VER)
    struct SplitTmToTime
    {
        using TickType = __time64_t;

        static TickType toTime(std::tm& tmb)
        {
            return ::_mkgmtime64(&tmb)
        };
    };
#else
    #error No timegm() runtime function avaiable
#endif

    namespace chrono = std::chrono;
    using Clock = chrono::system_clock;

    std::tm tmb; // NOLINT(cppcoreguidelines-pro-type-member-init)
    std::memset(&tmb, 0, sizeof(tmb));
    SplitTmToTime::TickType ticks = 0;
    chrono::duration<double, std::ratio<1>> fpSeconds = {};

    auto locale = in.getloc();
    in.imbue(std::locale::classic());
    in >> std::get_time(&tmb, "%Y-%m-%dT%H:%M:");
    tmb.tm_isdst = 0;
    double seconds = 0;
    in >> seconds;
    in.imbue(locale);
    char zone = 0;
    in >> zone;
    if (!in)
        return in;
    if (zone != 'Z')
    {
        in.setstate(std::ios::failbit);
        return in;
    }

    ticks = SplitTmToTime::toTime(tmb);
    if (ticks == decltype(ticks)(-1))
    {
        in.setstate(std::ios::failbit);
        return in;
    }

    when = Clock::time_point{chrono::seconds{ticks}};
    fpSeconds = decltype(fpSeconds){seconds};
    when += chrono::duration_cast<Clock::duration>(fpSeconds);
    return in;
}

//------------------------------------------------------------------------------
inline bool parseRfc3339Timestamp(std::string s,
                                  std::chrono::system_clock::time_point& when)
{
    std::istringstream iss{s};
    inputRfc3339Timestamp(iss, when);
    return bool(iss);
}

} // namespace internal

} // namespace wamp

#undef CPPWAMP_HAS_TIMEGM
#undef CPPWAMP_HAS_GMTIME_R

#endif // CPPWAMP_INTERNAL_TIMEFORMATTING_HPP
