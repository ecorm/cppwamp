/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <utility>
#include "callee.hpp"
#include "config.hpp"

namespace wamp
{

//******************************************************************************
// Realm
//******************************************************************************

//------------------------------------------------------------------------------
CPPWAMP_INLINE Realm::Realm(String uri) : uri_(std::move(uri)) {}

CPPWAMP_INLINE const String& Realm::uri() const {return uri_;}

CPPWAMP_INLINE String& Realm::uri(internal::PassKey) {return uri_;}


//******************************************************************************
// SessionInfo
//******************************************************************************

CPPWAMP_INLINE SessionInfo::SessionInfo() {}

CPPWAMP_INLINE SessionId SessionInfo::id() const {return sid_;}

CPPWAMP_INLINE const String& SessionInfo::realm() const {return realm_;}

CPPWAMP_INLINE String SessionInfo::agentString() const
{
    return optionOr("agent", String());
}

CPPWAMP_INLINE Object SessionInfo::roles() const
{
    return optionOr("roles", Object());
}

/** @details
    Possible role strings include:
    - `broker`
    - `dealer`

    @par Example
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    bool supported = sessionInfo.supportsRoles({"broker", "dealer"});
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
CPPWAMP_INLINE bool SessionInfo::supportsRoles(const RoleSet& roles) const
{
    if (roles.empty())
        return true;

    auto optionsIter = options().find("roles");
    if (optionsIter == options().end())
        return false;
    const auto& routerRoles = optionsIter->second.as<Object>();

    for (const auto& role: roles)
    {
        if (routerRoles.count(role) == 0)
            return false;
    }

    return true;
}

/** @details
    @par Example
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    bool supported = sessionInfo.supportsFeatures(
    {
        { "broker", {"publisher_exclusion", "publisher_identification"} },
        { "dealer", {"call_canceling"} }
    });
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
CPPWAMP_INLINE bool SessionInfo::supportsFeatures(
        const FeatureMap& features) const
{
    if (features.empty())
        return true;

    auto optionsIter = options().find("roles");
    if (optionsIter == options().end())
        return false;
    const auto& routerRoles = optionsIter->second.as<Object>();

    for (const auto& reqsKv: features)
    {
        const auto& role = reqsKv.first;
        const auto& reqFeatureSet = reqsKv.second;

        auto routerRoleIter = routerRoles.find(role);
        if (routerRoleIter == routerRoles.end())
            return false;
        const auto& routerRoleMap = routerRoleIter->second.as<Object>();

        auto routerFeaturesIter = routerRoleMap.find("features");
        if (routerFeaturesIter == routerRoleMap.end())
            return false;
        const auto& routerFeatureMap =
                routerFeaturesIter->second.as<Object>();

        for (const auto& reqFeature: reqFeatureSet)
        {
            auto routerFeatureIter = routerFeatureMap.find(reqFeature);
            if (routerFeatureIter == routerFeatureMap.end())
                return false;
            if (routerFeatureIter->second != true)
                return false;
        }
    }

    return true;
}

CPPWAMP_INLINE SessionInfo::SessionInfo(internal::PassKey, String realm,
                                        SessionId id, Object details)
    : Options<SessionInfo>(std::move(details)),
      realm_(std::move(realm)),
      sid_(id)
{}

CPPWAMP_INLINE std::ostream& operator<<(std::ostream& out,
                                        const SessionInfo& info)
{
    out << "[ Realm|uri = " << info.realm()
        << ", Session|id = " << info.id();
    if (!info.options().empty())
        out << ", Details|dict = " << info.options();
    return out << " ]";
}


//******************************************************************************
// Topic
//******************************************************************************

CPPWAMP_INLINE Topic::Topic(String uri) : uri_(std::move(uri)) {}

/** @details
    This sets the `SUBSCRIBE.Options.match|string` option to `"prefix"`.
    See [Pattern-based Subscriptions][pattern_based_sub] in the advanced
    WAMP spec.
    [pattern_based_sub]:
        https://github.com/tavendo/WAMP/blob/master/spec/advanced.md#pattern-based-subscription */
CPPWAMP_INLINE Topic& Topic::usingPrefixMatch()
{
    return withOption("match", "prefix");
}

/** @details
    This sets the `SUBSCRIBE.Options.match|string` option to `"wildcard"`.
    See [Pattern-based Subscriptions][pattern_based_sub] in the advanced
    WAMP spec.
    [pattern_based_sub]:
        https://github.com/tavendo/WAMP/blob/master/spec/advanced.md#pattern-based-subscriptions */
CPPWAMP_INLINE Topic& Topic::usingWildcardMatch()
{
    return withOption("match", "wildcard");
}

CPPWAMP_INLINE const String& Topic::uri() const {return uri_;}

CPPWAMP_INLINE String& Topic::uri(internal::PassKey) {return uri_;}


//******************************************************************************
// Pub
//******************************************************************************

CPPWAMP_INLINE Pub::Pub(String topic) : topic_(std::move(topic)) {}

/** @details
    This sets the `PUBLISH.Options.exclude|list` option. See
    [Subscriber Black- and Whitelisting][sub_black_whitelisting] in the
    advanced WAMP spec.
    [sub_black_whitelisting]:
        https://github.com/tavendo/WAMP/blob/master/spec/advanced.md#subscriber-black--and-whitelisting */
CPPWAMP_INLINE Pub& Pub::withBlacklist(Array blacklist)
{
    return withOption("exclude", std::move(blacklist));
}

/** @details
    This sets the `PUBLISH.Options.eligible|list` option. See
    [Subscriber Black- and Whitelisting][sub_black_whitelisting] in the
    advanced WAMP spec.
    [sub_black_whitelisting]:
        https://github.com/tavendo/WAMP/blob/master/spec/advanced.md#subscriber-black--and-whitelisting */
CPPWAMP_INLINE Pub& Pub::withWhitelist(Array whitelist)
{
    return withOption("eligible", std::move(whitelist));
}

/** @details
    This sets the `PUBLISH.Options.exclude_me|bool` option. See
    [Publisher Exclusion][pub_exclusion] in the advanced WAMP spec.
    [pub_exclusion]:
        https://github.com/tavendo/WAMP/blob/master/spec/advanced.md#publisher-exclusion */
CPPWAMP_INLINE Pub& Pub::withExcludeMe(bool excluded)
{
    return withOption("exclude_me", excluded);
}

/** @details
    This sets the `PUBLISH.Options.disclose_me|bool` option. See
    [Publisher Identification][pub_ident] in the advanced WAMP spec.
    [pub_ident]:
        https://github.com/tavendo/WAMP/blob/master/spec/advanced.md#publisher-identification */
CPPWAMP_INLINE Pub& Pub::withDiscloseMe(bool disclosed)
{
    return withOption("disclose_me", disclosed);
}

CPPWAMP_INLINE String&Pub::topic(internal::PassKey) {return topic_;}


//******************************************************************************
// Event
//******************************************************************************

CPPWAMP_INLINE Event::Event() {}

CPPWAMP_INLINE SubscriptionId Event::subId() const {return subId_;}

CPPWAMP_INLINE PublicationId Event::pubId() const {return pubId_;}

/** @details
    This function checks the value of the `EVENT.Details.publisher|integer`
    detail. See [Publisher Identification][pub_ident] in the advanced WAMP spec.
    [pub_ident]:
        https://github.com/tavendo/WAMP/blob/master/spec/advanced.md#publisher-identification
    @returns An integer variant if the publisher ID is available. Otherwise,
             a null variant is returned. */
CPPWAMP_INLINE Variant Event::publisher() const
{
    return optionByKey("publisher");
}

/** @details
    This function checks the value of the `EVENT.Details.trustlevel|integer`
    detail. See [Publication Trust Levels][pub_trust] in the advanced WAMP spec.
    [pub_trust]:
        https://github.com/tavendo/WAMP/blob/master/spec/advanced.md#publication-trust-levels
    @returns An integer variant if the trust level is available. Otherwise,
             a null variant is returned. */
CPPWAMP_INLINE Variant Event::trustLevel() const
{
    return optionByKey("trustlevel");
}

/** @details
    This function checks the value of the `EVENT.Details.topic|uri` detail. See
    [Pattern-based Subscriptions][pattern_based_subs] in the advanced WAMP spec.
    [pattern_based_subs]:
        https://github.com/tavendo/WAMP/blob/master/spec/advanced.md#pattern-based-subscriptions
    @returns A string variant if the topic URI is available. Otherwise,
             a null variant is returned. */
CPPWAMP_INLINE Variant Event::topic() const
{
    return optionByKey("topic");
}

CPPWAMP_INLINE Event::Event(internal::PassKey, SubscriptionId subId,
                            PublicationId pubId, Object&& details)
    : Options<Event>(std::move(details)),
      subId_(subId),
      pubId_(pubId)
{}

CPPWAMP_INLINE std::ostream& operator<<(std::ostream& out, const Event& event)
{
    out << "[ Publication|id = " << event.pubId();
    if (!event.options().empty())
        out << ", Details|dict = " << event.options();
    if (!event.args().empty())
        out << ", Arguments|list = " << event.args();
    if (!event.args().empty())
        out << ", ArgumentsKw|dict = " << event.kwargs();
    return out << " ]";
}


//******************************************************************************
// Procedure
//******************************************************************************

CPPWAMP_INLINE Procedure::Procedure(String uri) : uri_(std::move(uri)) {}

/** @details
    This sets the `REGISTER.Options.match|string` option to `"prefix"`.
    See [Pattern-based Registrations][pattern_based_reg] in the advanced
    WAMP spec.
    [pattern_based_reg]:
        https://github.com/tavendo/WAMP/blob/master/spec/advanced.md#pattern-based-registrations */
CPPWAMP_INLINE Procedure& Procedure::usingPrefixMatch()
{
    return withOption("match", "prefix");
}

/** @details
    This sets the `REGISTER.Options.match|string` option to `"wildcard"`.
    See [Pattern-based Registrations][pattern_based_reg] in the advanced
    WAMP spec.
    [pattern_based_reg]:
        https://github.com/tavendo/WAMP/blob/master/spec/advanced.md#pattern-based-registrations */
CPPWAMP_INLINE Procedure& Procedure::usingWildcardMatch()
{
    return withOption("match", "wildcard");
}

/** @details
    This sets the `REGISTER.Options.disclose_caller|bool` option. See
    [Caller Identification][caller_ident] in the advanced WAMP spec.
    [caller_ident]:
        https://github.com/tavendo/WAMP/blob/master/spec/advanced.md#caller-identification */
CPPWAMP_INLINE Procedure& Procedure::withDiscloseCaller(bool disclosed)
{
    return withOption("disclose_caller", disclosed);
}

CPPWAMP_INLINE const String& Procedure::uri() const {return uri_;}

CPPWAMP_INLINE String& Procedure::uri(internal::PassKey) {return uri_;}


//******************************************************************************
// Rpc
//******************************************************************************

CPPWAMP_INLINE Rpc::Rpc(String procedure) : procedure_(std::move(procedure)) {}

CPPWAMP_INLINE Rpc& Rpc::withDealerTimeout(Int milliseconds)
{
    return withOption("timeout", milliseconds);
}

/** @details
    This sets the `CALL.Options.exclude|list` option. See
    [Callee Black- and Whitelisting][callee_black_whitelisting] in the
    advanced WAMP spec.
    [callee_black_whitelisting]:
        https://github.com/tavendo/WAMP/blob/master/spec/advanced.md#callee-black--and-whitelisting */
CPPWAMP_INLINE Rpc& Rpc::withBlacklist(Array blacklist)
{
    return withOption("exclude", std::move(blacklist));
}

/** @details
    This sets the `CALL.Options.eligible|list` option. See
    [Callee Black- and Whitelisting][callee_black_whitelisting] in the
    advanced WAMP spec.
    [callee_black_whitelisting]:
        https://github.com/tavendo/WAMP/blob/master/spec/advanced.md#callee-black--and-whitelisting */
CPPWAMP_INLINE Rpc& Rpc::withWhitelist(Array whitelist)
{
    return withOption("eligible", std::move(whitelist));
}

/** @details
    This sets the `CALL.Options.exclude_me|bool` option. See
    [Caller Exclusion][caller_exclusion] in the advanced WAMP spec.
    [caller_exclusion]:
        https://github.com/tavendo/WAMP/blob/master/spec/advanced.md#caller-exclusion */
CPPWAMP_INLINE Rpc& Rpc::withExcludeMe(bool excluded)
{
    return withOption("exclude_me", excluded);
}

/** @details
    This sets the `CALL.Options.disclose_me|bool` option. See
    [Caller Identification][caller_ident] in the advanced WAMP spec.
    [caller_ident]:
        https://github.com/tavendo/WAMP/blob/master/spec/advanced.md#caller-identification */
CPPWAMP_INLINE Rpc& Rpc::withDiscloseMe(bool disclosed)
{
    return withOption("disclose_me", disclosed);
}

CPPWAMP_INLINE String& Rpc::procedure(internal::PassKey) {return procedure_;}


//******************************************************************************
// Result
//******************************************************************************

CPPWAMP_INLINE Result::Result() {}

CPPWAMP_INLINE Result::Result(std::initializer_list<Variant> list)
{
    withArgs(Array(list));
}

/** @details
    This sets the `YIELD.Options.progress|bool` option. See
    [Progressive Call Results][prog_calls] in the advanced WAMP spec.
    [prog_calls]:
        https://github.com/tavendo/WAMP/blob/master/spec/advanced.md#progressive-call-results */
CPPWAMP_INLINE Result& Result::withProgress(bool progressive)
{
    return withOption("progress", progressive);
}

CPPWAMP_INLINE RequestId Result::requestId() const
{
    return reqId_;
}

CPPWAMP_INLINE Result::Result(internal::PassKey, RequestId reqId,
                              Object&& details)
    : Options<Result>(std::move(details)),
      reqId_(reqId)
{}

CPPWAMP_INLINE std::ostream& operator<<(std::ostream& out, const Result& result)
{
    out << "[ Request|id = " << result.requestId();
    if (!result.options().empty())
        out << ", Details|dict = " << result.options();
    if (!result.args().empty())
        out << ", Arguments|list = " << result.args();
    if (!result.args().empty())
        out << ", ArgumentsKw|dict = " << result.kwargs();
    return out << " ]";
}

//******************************************************************************
// Invocation
//******************************************************************************

CPPWAMP_INLINE Invocation::Invocation() {}

CPPWAMP_INLINE bool Invocation::calleeHasExpired() const
{
    return callee_.expired();
}

CPPWAMP_INLINE RequestId Invocation::requestId() const {return id_;}

/** @details
    This function checks if the `INVOCATION.Details.receive_progress|bool`
    detail is `true`. See [Progressive Call Results][prog_calls] in the advanced
    WAMP spec.
    [prog_calls]:
        https://github.com/tavendo/WAMP/blob/master/spec/advanced.md#progressive-call-results */
CPPWAMP_INLINE bool Invocation::isProgressive() const
{
    return optionOr("receive_progress", false);
}

/** @details
    This function checks the value of the `INVOCATION.Details.caller|integer`
    detail. See [Caller Identification][caller_ident] in the advanced WAMP spec.
    [caller_ident]:
        https://github.com/tavendo/WAMP/blob/master/spec/advanced.md#caller-identification
    @returns An integer variant if the caller ID is available. Otherwise,
             a null variant is returned.*/
CPPWAMP_INLINE Variant Invocation::caller() const
{
    return optionByKey("caller");
}

/** @details
    This function checks the value of the `INVOCATION.Details.trustlevel|integer`
    detail. See [Call Trust Levels][call_trust] in the advanced WAMP spec.
    [call_trust]:
        https://github.com/tavendo/WAMP/blob/master/spec/advanced.md#call-trust-levels
    @returns An integer variant if the trust level is available. Otherwise,
             a null variant is returned. */
CPPWAMP_INLINE Variant Invocation::trustLevel() const
{
    return optionByKey("trustlevel");
}

/** @details
    This function checks the value of the `INVOCATION.Details.procedure|uri`
    detail. See [Pattern-based Registrations][pattern_based_reg] in the
    advanced WAMP spec.
    [pattern_based_reg]:
        https://github.com/tavendo/WAMP/blob/master/spec/advanced.md#pattern-based-registrations
    @returns A string variant if the procedure URI is available. Otherwise,
             a null variant is returned. */
CPPWAMP_INLINE Variant Invocation::procedure() const
{
    return optionByKey("procedure");
}

/** @pre `this->calleeHasExpired == false` */
CPPWAMP_INLINE void Invocation::yield(Result result)
{
    auto callee = callee_.lock();
    CPPWAMP_LOGIC_CHECK(!!callee, "Client no longer exists");
    callee->yield(id_, std::move(result));
}

/** @pre `this->calleeHasExpired == false` */
CPPWAMP_INLINE void Invocation::yield(Error error)
{
    auto callee = callee_.lock();
    CPPWAMP_LOGIC_CHECK(!!callee, "Client no longer exists");
    callee->yield(id_, std::move(error));
}

CPPWAMP_INLINE Invocation::Invocation(internal::PassKey, CalleePtr callee,
                                      RequestId id, Object&& details)
    : Options<Invocation>(std::move(details)),
      callee_(callee),
      id_(id)
{}

CPPWAMP_INLINE std::ostream& operator<<(std::ostream& out,
                                        const Invocation& inv)
{
    out << "[ Request|id = " << inv.requestId();
    if (!inv.options().empty())
        out << ", Details|dict = " << inv.options();
    if (!inv.args().empty())
        out << ", Arguments|list = " << inv.args();
    if (!inv.args().empty())
        out << ", ArgumentsKw|dict = " << inv.kwargs();
    return out << " ]";
}


} // namespace wamp
