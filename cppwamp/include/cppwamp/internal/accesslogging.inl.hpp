/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../accesslogging.hpp"
#include <array>
#include <sstream>
#include <utility>
#include "../api.hpp"
#include "timeformatting.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE void outputAccessLogEntry(
    std::ostream& out, const AccessLogEntry& entry, bool colored)
{
    static constexpr const char* red = "\x1b[1;31m";
    static constexpr const char* plain = "\x1b[0m";

    struct PutField
    {
        PutField(std::ostream& out) : out_(&out) {}

        PutField& operator<<(const std::string& field)
        {
            if (field.empty())
                *out_ << " | -";
            else
                *out_ << " | " << field;
            return *this;
        }

    private:
        std::ostream* out_;
    };

    const auto& c = entry.connection;
    const auto& s = entry.session;
    const auto& a = entry.action;
    AccessLogEntry::outputTime(out, entry.when);
    PutField{out} << c.server();
    out << " | " << c.serverSessionNumber();
    PutField{out} << c.endpoint() << s.realmUri() << s.auth().id() << s.agent();
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

    if (a.options.empty())
        out << " | -";
    else
        out << " | " << a.options;
}

} // namespace internal


//******************************************************************************
// AccessAction
//******************************************************************************

CPPWAMP_INLINE const std::string& accessActionLabel(AccessAction action)
{
    static const std::array<std::string, 30> labels{
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
         "server-disconnect",
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
    }};

    using T = std::underlying_type<AccessAction>::type;
    auto n = static_cast<T>(action);
    assert(n >= 0);
    return labels.at(n);
}


//******************************************************************************
// AccessActionInfo
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessActionInfo::AccessActionInfo() = default;

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


//******************************************************************************
// AccessLogEntry
//******************************************************************************

//------------------------------------------------------------------------------
/** @copydetails LogEntry::outputTime */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::ostream& AccessLogEntry::outputTime(std::ostream& out,
                                                        TimePoint when)
{
    return internal::outputRfc3339Timestamp<3>(out, when);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE AccessLogEntry::AccessLogEntry(ConnectionInfo connection,
                                              SessionInfo session,
                                              AccessActionInfo action)
    : connection(std::move(connection)),
      session(std::move(session)),
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
    request ID | action | target URI | error URI | {action options}
    ```
    @note This function uses `std::gmtime` on platforms where `gmtime_r` is not
          available, where the former may not be thread-safe. */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::string toString(const AccessLogEntry& entry)
{
    std::ostringstream oss;
    toStream(oss, entry);
    return oss.str();
}

//------------------------------------------------------------------------------
/** @relates AccessLogEntry
    @copydetails toString(const AccessLogEntry&) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::ostream& toStream(std::ostream& out,
                                      const AccessLogEntry& entry)
{
    internal::outputAccessLogEntry(out, entry, false);
    return out;
}

//------------------------------------------------------------------------------
/** @relates AccessLogEntry
    @copydetails toString(const AccessLogEntry&) */
//------------------------------------------------------------------------------
CPPWAMP_INLINE std::ostream& toColorStream(std::ostream& out,
                                           const AccessLogEntry& entry)
{
    internal::outputAccessLogEntry(out, entry, true);
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
