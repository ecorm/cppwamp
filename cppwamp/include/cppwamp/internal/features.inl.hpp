/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../features.hpp"
#include "../api.hpp"
#include "../variant.hpp"

namespace wamp
{

//******************************************************************************
// ClientFeatures
//******************************************************************************

CPPWAMP_INLINE ClientFeatures ClientFeatures::provided()
{
    using F = Feature;
    ClientFeatures f;

    /* Not supported: callReroute, payloadPassthruMode, registrationRevocation,
       shardedRegistration */
    f.callee_ = F::basic |
                F::callCanceling |
                F::callTimeout |
                F::callTrustLevels |
                F::callerIdentification |
                F::patternBasedRegistration |
                F::progressiveCallInvocations |
                F::progressiveCallResults |
                F::sharedRegistration;

    /* Not supported: payloadPassthruMode */
    f.caller_ = F::basic |
                F::callCanceling |
                F::callTimeout |
                F::callerIdentification |
                F::progressiveCallInvocations |
                F::progressiveCallResults;

    /* Not supported: payloadPassthruMode */
    f.publisher_ = F::basic |
                   F::publisherExclusion |
                   F::publisherIdentification |
                   F::subscriberBlackWhiteListing;

    /* Not supported: payloadPassthruMode, shardedSubscription,
       subscriptionRevocation */
    f.subscriber_ = F::basic |
                    F::patternBasedSubscription |
                    F::publicationTrustLevels |
                    F::publisherIdentification;

    return f;
}

CPPWAMP_INLINE const Object& ClientFeatures::providedRoles()
{
    static const Object roles =
    {
        {"callee", Object{{"features", Object{{
            {"call_canceling",                true},
            {"call_timeout",                  true},
            {"call_trustlevels",              true},
            {"caller_identification",         true},
            {"pattern_based_registration",    true},
            {"progressive_call_results",      true},
            {"progressive_call_invocations",  true},
            {"shared_registration",           true}
        }}}}},
        {"caller", Object{{"features", Object{{
            {"call_canceling",                true},
            {"call_timeout",                  true},
            {"caller_identification",         true},
            {"progressive_call_results",      true},
            {"progressive_call_invocations",  true}
        }}}}},
        {"publisher", Object{{"features", Object{{
            {"publisher_exclusion",           true},
            {"publisher_identification",      true},
            {"subscriber_blackwhite_listing", true}
        }}}}},
        {"subscriber", Object{{"features", Object{{
            {"pattern_based_subscription",    true},
            {"publication_trustlevels",       true},
            {"publisher_identification",      true},
        }}}}}
    };
    return roles;
}

CPPWAMP_INLINE ClientFeatures::ClientFeatures() = default;

CPPWAMP_INLINE ClientFeatures::ClientFeatures(
    FeatureFlags callee, FeatureFlags caller,
    FeatureFlags publisher, FeatureFlags subscriber)
    : callee_(callee),
      caller_(caller),
      publisher_(publisher),
      subscriber_(subscriber)
{}

CPPWAMP_INLINE ClientFeatures::ClientFeatures(const Object& dict)
{
    parseCalleeFeatures(dict);
    parseCallerFeatures(dict);
    parsePublisherFeatures(dict);
    parseSubscriberFeatures(dict);
}

CPPWAMP_INLINE FeatureFlags ClientFeatures::callee() const {return callee_;}

CPPWAMP_INLINE FeatureFlags ClientFeatures::caller() const {return caller_;}

CPPWAMP_INLINE FeatureFlags ClientFeatures::publisher() const
{
    return publisher_;
}

CPPWAMP_INLINE FeatureFlags ClientFeatures::subscriber() const
{
    return subscriber_;
}

CPPWAMP_INLINE bool ClientFeatures::supports(ClientFeatures desired) const
{
    return callee_.all_of(desired.callee_) &&
           caller_.all_of(desired.caller_) &&
           publisher_.all_of(desired.publisher_) &&
           subscriber_.all_of(desired.subscriber_);
}

CPPWAMP_INLINE void ClientFeatures::reset()
{
    callee_.reset();
    caller_.reset();
    publisher_.reset();
    subscriber_.reset();
}

CPPWAMP_INLINE const Object*
ClientFeatures::findFeaturesDict(const Object& dict, const char* roleName)
{
    auto found = dict.find(roleName);
    if (found == dict.end() || !found->second.is<Object>())
        return nullptr;
    const auto& roleDict = found->second.as<Object>();

    found = roleDict.find("features");
    if (found == dict.end() || !found->second.is<Object>())
        return nullptr;
    return &(found->second.as<Object>());
}

CPPWAMP_INLINE void ClientFeatures::parse(
    FeatureFlags& flags, Feature pos, const Object* roleDict,
    const char* featureName)
{
    if (roleDict->count(featureName) != 0)
        flags.set(pos);
}

CPPWAMP_INLINE void ClientFeatures::parseCalleeFeatures(const Object& dict)
{
    using F = Feature;
    const auto* d = findFeaturesDict(dict, "callee");
    if (d == nullptr)
        return;
    callee_.set(F::basic, true);
    parse(callee_, F::callCanceling,              d, "call_canceling");
    parse(callee_, F::callReroute,                d, "call_reroute");
    parse(callee_, F::callTimeout,                d, "call_timeout");
    parse(callee_, F::callTrustLevels,            d, "call_trustlevels");
    parse(callee_, F::callerIdentification,       d, "caller_identification");
    parse(callee_, F::patternBasedRegistration,   d, "pattern_based_registration");
    parse(callee_, F::payloadPassthruMode,        d, "payload_passthru_mode");
    parse(callee_, F::progressiveCallInvocations, d, "progressive_call_invocations");
    parse(callee_, F::progressiveCallResults,     d, "progressive_call_results");
    parse(callee_, F::registrationRevocation,     d, "registration_revocation");
    parse(callee_, F::shardedRegistration,        d, "sharded_registration");
    parse(callee_, F::sharedRegistration,         d, "shared_registration");

    // Legacy feature keys
    parse(callee_, F::progressiveCallInvocations, d, "progressive_calls");
}

CPPWAMP_INLINE void ClientFeatures::parseCallerFeatures(const Object& dict)
{
    using F = Feature;
    const auto* d = findFeaturesDict(dict, "caller");
    if (d == nullptr)
        return;
    caller_.set(F::basic, true);
    parse(caller_, F::callCanceling,              d, "call_canceling");
    parse(caller_, F::callTimeout,                d, "call_timeout");
    parse(caller_, F::callerIdentification,       d, "caller_identification");
    parse(caller_, F::payloadPassthruMode,        d, "payload_passthru_mode");
    parse(caller_, F::progressiveCallInvocations, d, "progressive_call_invocations");
    parse(caller_, F::progressiveCallResults,     d, "progressive_call_results");

    // Legacy feature keys
    parse(caller_, F::progressiveCallInvocations, d, "progressive_calls");

    // Alternate spelling for call_canceling
    parse(caller_, F::callCanceling,              d, "call_cancelling");
}

CPPWAMP_INLINE void ClientFeatures::parsePublisherFeatures(const Object& dict)
{
    using F = Feature;
    const auto* d = findFeaturesDict(dict, "publisher");
    if (d == nullptr)
        return;
    publisher_.set(F::basic, true);
    parse(publisher_, F::payloadPassthruMode,         d, "payload_passthru_mode");
    parse(publisher_, F::publisherExclusion,          d, "publisher_exclusion");
    parse(publisher_, F::publisherIdentification,     d, "publisher_identification");
    parse(publisher_, F::subscriberBlackWhiteListing, d, "subscriber_blackwhite_listing");
}

CPPWAMP_INLINE void ClientFeatures::parseSubscriberFeatures(const Object& dict)
{
    using F = Feature;
    const auto* d = findFeaturesDict(dict, "subscriber");
    if (d == nullptr)
        return;
    subscriber_.set(F::basic, true);
    parse(subscriber_, F::patternBasedSubscription, d, "pattern_based_subscription");
    parse(subscriber_, F::payloadPassthruMode,      d, "payload_passthru_mode");
    parse(subscriber_, F::publicationTrustLevels,   d, "publication_trustlevels");
    parse(subscriber_, F::publisherIdentification,  d, "publisher_identification");
    parse(subscriber_, F::shardedSubscription,      d, "sharded_subscription");
    parse(subscriber_, F::subscriptionRevocation,   d, "subscription_revocation");
}


//******************************************************************************
// RouterFeatures
//******************************************************************************

CPPWAMP_INLINE RouterFeatures RouterFeatures::provided()
{
    using F = Feature;
    RouterFeatures f;

    /* Not supported: eventHistory, eventRetention, payloadPassthruMode,
       shardedSubscription, subscriptionRevocation, topicReflection */
    f.broker_ = F::basic |
                F::patternBasedSubscription |
                F::publicationTrustLevels |
                F::publisherExclusion |
                F::publisherIdentification |
                F::sessionMetaApi |
                F::subscriberBlackWhiteListing |
                F::subscriptionMetaApi;

    /* Not supported: callReroute, patternBasedRegistration,
       payloadPassthruMode, procedureReflection, registrationRevocation,
       shardedRegistration, sharedRegistration, sessionTestament */
    f.dealer_ = F::basic |
                F::callCanceling |
                F::callTimeout |
                F::callTrustLevels |
                F::callerIdentification |
                F::progressiveCallInvocations |
                F::progressiveCallResults |
                F::registrationMetaApi |
                F::sessionMetaApi;

    return f;
}

CPPWAMP_INLINE const Object& RouterFeatures::providedRoles()
{
    static const Object roles =
    {
        {"dealer", Object{{"features", Object{{
            {"call_canceling",                true},
            {"call_timeout",                  true},
            {"call_trustlevels",              true},
            {"caller_identification",         true},
            {"progressive_call_invocations",  true},
            {"progressive_call_results",      true},
            {"registration_meta_api",         true},
            {"session_meta_api",              true}
        }}}}},
        {"broker", Object{{"features", Object{{
            {"pattern_based_subscription",    true},
            {"publisher_exclusion",           true},
            {"publisher_identification",      true},
            {"session_meta_api",              true},
            {"subscriber_blackwhite_listing", true},
            {"subscription_meta_api",         true}
        }}}}}
    };
    return roles;
}

CPPWAMP_INLINE RouterFeatures::RouterFeatures() = default;

CPPWAMP_INLINE RouterFeatures::RouterFeatures(FeatureFlags broker,
                                              FeatureFlags dealer)
    : broker_(broker),
      dealer_(dealer)
{}

CPPWAMP_INLINE RouterFeatures::RouterFeatures(const Object& dict)
{
    parseBrokerFeatures(dict);
    parseDealerFeatures(dict);
}

CPPWAMP_INLINE FeatureFlags RouterFeatures::broker() const {return broker_;}

CPPWAMP_INLINE FeatureFlags RouterFeatures::dealer() const {return dealer_;}

CPPWAMP_INLINE bool RouterFeatures::supports(RouterFeatures desired) const
{
    return broker_.all_of(desired.broker_) && dealer_.all_of(desired.dealer_);
}

CPPWAMP_INLINE void RouterFeatures::parse(FeatureFlags& flags, Feature pos,
                                          const Object* roleDict,
                                          const char* featureName)
{
    if (roleDict->count(featureName) != 0)
        flags.set(pos, true);
}

CPPWAMP_INLINE const Object*
RouterFeatures::findFeaturesDict(const Object& dict, const char* roleName)
{
    auto found = dict.find(roleName);
    if (found == dict.end() || !found->second.is<Object>())
        return nullptr;
    const auto& roleDict = found->second.as<Object>();

    found = roleDict.find("features");
    if (found == roleDict.end() || !found->second.is<Object>())
        return nullptr;
    return &(found->second.as<Object>());
}

CPPWAMP_INLINE void RouterFeatures::parseBrokerFeatures(const Object& dict)
{
    using F = Feature;
    const auto* d = findFeaturesDict(dict, "broker");
    if (d == nullptr)
        return;
    broker_.set(F::basic, true);
    parse(broker_, F::eventHistory,                d, "event_history");
    parse(broker_, F::eventRetention,              d, "event_retention");
    parse(broker_, F::patternBasedSubscription,    d, "pattern_based_subscription");
    parse(broker_, F::payloadPassthruMode,         d, "payload_passthru_mode");
    parse(broker_, F::publicationTrustLevels,      d, "publication_trustlevels");
    parse(broker_, F::publisherExclusion,          d, "publisher_exclusion");
    parse(broker_, F::publisherIdentification,     d, "publisher_identification");
    parse(broker_, F::sessionMetaApi,              d, "session_meta_api");
    parse(broker_, F::sessionTestament,            d, "session_testament");
    parse(broker_, F::shardedSubscription,         d, "sharded_subscription");
    parse(broker_, F::subscriberBlackWhiteListing, d, "subscriber_blackwhite_listing");
    parse(broker_, F::subscriptionMetaApi,         d, "subscription_meta_api");
    parse(broker_, F::subscriptionRevocation,      d, "subscription_revocation");
    parse(broker_, F::topicReflection,             d, "topic_reflection");
}

CPPWAMP_INLINE void RouterFeatures::parseDealerFeatures(const Object& dict)
{
    using F = Feature;
    const auto* d = findFeaturesDict(dict, "dealer");
    if (d == nullptr)
        return;
    dealer_.set(F::basic, true);
    parse(dealer_, F::callCanceling,              d, "call_canceling");
    parse(dealer_, F::callReroute,                d, "call_reroute");
    parse(dealer_, F::callTimeout,                d, "call_timeout");
    parse(dealer_, F::callTrustLevels,            d, "call_trustlevels");
    parse(dealer_, F::callerIdentification,       d, "caller_identification");
    parse(dealer_, F::patternBasedRegistration,   d, "pattern_based_registration");
    parse(dealer_, F::payloadPassthruMode,        d, "payload_passthru_mode");
    parse(dealer_, F::procedureReflection,        d, "procedure_reflection");
    parse(dealer_, F::progressiveCallInvocations, d, "progressive_call_invocations");
    parse(dealer_, F::progressiveCallResults,     d, "progressive_call_results");
    parse(dealer_, F::registrationMetaApi,        d, "registration_meta_api");
    parse(dealer_, F::registrationRevocation,     d, "registration_revocation");
    parse(dealer_, F::sessionMetaApi,             d, "session_meta_api");
    parse(dealer_, F::shardedRegistration,        d, "sharded_registration");
    parse(dealer_, F::sharedRegistration,         d, "shared_registration");

    // Legacy feature keys
    parse(dealer_, F::progressiveCallInvocations, d, "progressive_calls");
}

} // namespace wamp
