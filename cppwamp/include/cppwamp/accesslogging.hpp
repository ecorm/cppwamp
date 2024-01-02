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
#include <string>
#include "api.hpp"
#include "errorcodes.hpp"
#include "sessioninfo.hpp"
#include "variant.hpp"
#include "wampdefs.hpp"


namespace wamp
{

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
    clientHttpGet,
    clientHttpHead,
    clientHttpPost,
    clientHttpPut,
    clientHttpDelete,
    clientHttpConnect,
    clientHttpOptions,
    clientHttpTrace,
    clientHttpOther,
    serverReject,
    serverDisconnect,
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

    AccessActionInfo( // NOLINT(google-explicit-constructor)
        Action action, std::string target = {}, Object options = {},
        std::string errorUri = {});

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

    AccessActionInfo(Action action, std::error_code ec);

    AccessActionInfo(Action action, WampErrc errc);

    std::string target;
    std::string errorUri;
    Object options;
    RequestId requestId = nullId();
    Action action = {};

private:
    static Object optionsWithErrorDesc(Object options, std::error_code ec);
};


//------------------------------------------------------------------------------
/** Contains access logging HTTP request information. */
//------------------------------------------------------------------------------
struct CPPWAMP_API HttpAccessInfo
{
    /** Default constructor. */
    HttpAccessInfo();

    /** Constructor. */
    HttpAccessInfo(std::string host, std::string agent);

    /** The client host field string. */
    std::string host;

    /** The client user agent string. */
    std::string agent;
};


//------------------------------------------------------------------------------
/** Contains access logging information. */
//------------------------------------------------------------------------------
struct CPPWAMP_API AccessLogEntry
{
    /// Type used for timestamps.
    using TimePoint = std::chrono::system_clock::time_point;

    /** Outputs a timestamp in RFC3339 format. */
    static std::ostream& outputTime(std::ostream& out, TimePoint when);

    /** Constructor. */
    AccessLogEntry(ConnectionInfo connection, AccessActionInfo action);

    /** Constructor taking WAMP session information. */
    AccessLogEntry(ConnectionInfo connection, SessionInfo session,
                   AccessActionInfo action);

    /** Constructor taking HTTP request information. */
    AccessLogEntry(ConnectionInfo connection, HttpAccessInfo http,
                   AccessActionInfo action);

    /** The connection information. */
    ConnectionInfo connection;

    /** The WAMP session information. */
    SessionInfo session;

    /** The HTTP request information. */
    HttpAccessInfo http;

    /** The action information. */
    AccessActionInfo action;

    /** Timestamp. */
    TimePoint when;

    /** Determines if the entry corresponds to an HTTP request. */
    bool isHttp;
};

/** Obtains a formatted log entry string combining all available information.
    @relates AccessLogEntry */
std::string toString(const AccessLogEntry& entry);

/** Outputs a formatted log entry combining all available information.
    @relates AccessLogEntry */
std::ostream& toStream(std::ostream& out, const AccessLogEntry& entry);

/** Outputs a formatted access log entry using ANSI color escape codes.
    @relates AccessLogEntry */
std::ostream& toColorStream(std::ostream& out, const AccessLogEntry& entry);

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
    using Handler = std::function<void (AccessLogEntry)>;

    explicit BasicAccessLogFilter(Handler handler)
        : handler_(std::move(handler))
    {}

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
#include "internal/accesslogging.inl.hpp"
#endif

#endif // CPPWAMP_ACCESSLOGGING_HPP
