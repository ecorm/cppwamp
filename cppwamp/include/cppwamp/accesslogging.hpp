/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ACCESSLOGGING_HPP
#define CPPWAMP_ACCESSLOGGING_HPP

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
#include "errorcodes.hpp"
#include "erroror.hpp"
#include "variantdefs.hpp"
#include "wampdefs.hpp"


namespace wamp
{

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
enum class AccessAction
{
    clientConnect,
    clientDisconnect,
    clientHello,
    clientAbort,
    clientAuthenticate,
    clientGoodbye,
    clientError,
    clientPublish,
    clientSubscribe,
    clientUnsubscribe,
    clientCall,
    clientCancel,
    clientRegister,
    clientUnregister,
    clientYield,
    serverWelcome,
    serverAbort,
    serverChallenge,
    serverGoodbye,
    serverError,
    serverPublished,
    serverSubscribed,
    serverUnsubscribed,
    serverEvent,
    serverResult,
    serverRegistered,
    serverUnregistered,
    serverInvocation,
    serverInterrupt
};

//------------------------------------------------------------------------------
const std::string& accessActionLabel(AccessAction action);

//------------------------------------------------------------------------------
struct CPPWAMP_API AccessActionInfo
{
    using Action = AccessAction;

    AccessActionInfo();

    AccessActionInfo(Action action, std::string target = {},
                     Object options = {}, std::string errorUri = {});

    AccessActionInfo(Action action, std::string target, Object options,
                     std::error_code ec);

    AccessActionInfo(Action action, std::string target, Object options,
                     WampErrc errc);

    AccessActionInfo(Action action, RequestId r, std::string target = {},
                     Object options = {}, std::string errorUri = {});

    AccessActionInfo(Action action, RequestId r, std::string target,
                     Object options, std::error_code ec);

    AccessActionInfo(Action action, RequestId r, std::string target,
                     Object options, WampErrc errc);

    template <typename T>
    AccessActionInfo(std::string action, std::string target,
                     Object options, const ErrorOr<T>& x)
        : AccessActionInfo(std::move(action), std::move(target),
                           std::move(options), toErrorUri(x))
    {}

    AccessActionInfo& withErrorUri(std::string uri);

    AccessActionInfo& withError(std::error_code ec);

    AccessActionInfo& withError(WampErrc errc);

    template <typename T>
    AccessActionInfo& withResult(const ErrorOr<T>& x)
    {
        if (!x)
            withError(x.error());
        return *this;
    }

    std::string target;
    std::string errorUri;
    Object options;
    RequestId requestId = nullId();
    Action action;

private:
    template <typename T>
    static std::string toErrorUri(const ErrorOr<T>& x)
    {
        return !x ? std::string{} : errorCodeToUri(x.error());
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
struct DefaultAccessLogFilterPolicy
{
    static bool check(AccessLogEntry& e);
};

//------------------------------------------------------------------------------
/// Access log handler wrapper that filters entries containing banned options.
//------------------------------------------------------------------------------
template <typename TPolicy>
class BasicAccessLogFilter
{
public:
    using Policy = TPolicy;

    template <typename F>
    BasicAccessLogFilter(F&& handler) : handler_(std::forward<F>(handler)) {}

    void operator()(AccessLogEntry entry) const
    {
        if (Policy::check(entry))
            handler_(std::move(entry));
    }

private:
    std::function<void (AccessLogEntry)> handler_;
};

//------------------------------------------------------------------------------
using AccessLogFilter = BasicAccessLogFilter<DefaultAccessLogFilterPolicy>;

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/accesslogging.ipp"
#endif

#endif // CPPWAMP_ACCESSLOGGING_HPP
