/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../accesslogging.hpp"
#include <sstream>
#include <utility>
#include "../api.hpp"
#include "../variant.hpp"
#include "timeformatting.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
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

    const auto& s = entry.session;
    const auto& a = entry.action;
    AccessLogEntry::outputTime(out, entry.when);
    PutField{out} << s.serverName;
    out << " | " << s.serverSessionIndex;
    PutField{out} << s.endpoint << s.realmUri << s.authId << s.agent;
    if (a.requestId == nullId())
        out << " | -";
    else
        out << " | " << a.requestId;

    PutField{out} << accessActionLabel(a.action) << a.target;

    out << " | ";
    if (a.errorUri.empty())
        out << "-";
    else if (colored)
        out << red << a.errorUri << plain;
    else
        out << a.errorUri;

    out << " | " << a.options;
}

} // namespace internal


//******************************************************************************
// AccessAction
//******************************************************************************

CPPWAMP_INLINE const std::string& accessActionLabel(AccessAction action)
{
    static const std::string labels[] =
    {
         "client-connect",
         "client-disconnect",
         "client-hello",
         "client-abort",
         "client-authenticate",
         "client-goodbye",
         "client-error",
         "client-publish",
         "client-subscribe",
         "client-unsubscribe",
         "client-call",
         "client-cancel",
         "client-register",
         "client-unregister",
         "client-yield",
         "server-welcome",
         "server-abort",
         "server-challenge",
         "server-goodbye",
         "server-error",
         "server-published",
         "server-subscribed",
         "server-unsubscribed",
         "server-event",
         "server-result",
         "server-registered",
         "server-unregistered",
         "server-invocation",
         "server-interrupt"
    };

    using T = std::underlying_type<AccessAction>::type;
    auto n = static_cast<T>(action);
    assert(n >= 0 && n <= T(std::extent<decltype(labels)>::value));
    return labels[n];
}


//******************************************************************************
// AccessActionInfo
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessActionInfo::AccessActionInfo() {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessActionInfo::AccessActionInfo(
    Action action, std::string target, Object options, std::string errorUri)
    : AccessActionInfo(action, nullId(), std::move(target),
                       std::move(options), std::move(errorUri))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessActionInfo::AccessActionInfo(
    Action action, std::string target, Object options, std::error_code ec)
    : AccessActionInfo(action, nullId(), std::move(target), std::move(options),
                       ec)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessActionInfo::AccessActionInfo(
    Action action, std::string target, Object options, WampErrc errc)
    : AccessActionInfo(action, nullId(), std::move(target), std::move(options),
                       errc)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessActionInfo::AccessActionInfo(
    Action action, RequestId r, std::string target, Object options,
    std::string errorUri)
    : target(std::move(target)),
      errorUri(std::move(errorUri)),
      options(std::move(options)),
      requestId(r),
      action(action)
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessActionInfo::AccessActionInfo(
    Action action, RequestId r, std::string target, Object options,
    std::error_code ec)
    : AccessActionInfo(action, r, std::move(target), std::move(options),
                       errorCodeToUri(ec))
{}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessActionInfo::AccessActionInfo(
    Action action, RequestId r, std::string target, Object options,
    WampErrc errc)
    : AccessActionInfo(action, r, std::move(target), std::move(options),
                       make_error_code(errc))
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
    return withErrorUri(errorCodeToUri(ec));
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessActionInfo& AccessActionInfo::withError(WampErrc errc)
{
    return withError(make_error_code(errc));
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
    return internal::toRfc3339TimestampInMilliseconds(out, when);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessLogEntry::AccessLogEntry(AccessSessionInfo session,
                                              AccessActionInfo action)
    : session(std::move(session)),
      action(std::move(action)),
      when(std::chrono::system_clock::now())
{}

//------------------------------------------------------------------------------
/** @relates AccessLogEntry
    @details
    The following format is used:
    ```
    YYYY-MM-DDTHH:MM:SS.sss | server name | server session index |
    transport endpoint | realm URI | authid | agent |
    action | target URI | error URI | {action options}
    ```
    @note This function uses `std::gmtime` on platforms where `gmtime_r` is not
          available, where the former may not be thread-safe. */
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


//******************************************************************************
// DefaultAccessLogFilter
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool DefaultAccessLogFilterPolicy::check(AccessLogEntry& e)
{
    // Allow authid option in client-hello and server-welcome for
    // auditing purposes.
    // https://github.com/wamp-proto/wamp-proto/issues/442
    static const std::set<String> banned(
        {"authextra", "authrole", "caller_authid", "caller_authrole",
         "caller_id", "eligible", "eligible_authid", "eligible_authrole",
         "exclude", "exclude_authid", "exclude_authrole", "forward_for",
         "publisher_authid", "publisher_authrole", "publisher_id"});

    using AA = AccessAction;
    auto& a = e.action;
    if (a.action == AA::clientAuthenticate || a.action == AA::serverChallenge)
    {
        a.options.clear();
    }
    else
    {
        for (auto& kv: a.options)
            if (banned.count(kv.first) != 0)
                kv.second = null;
    }

    return true;
}

} // namespace wamp
