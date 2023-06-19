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
    ClientFeatures f;

    {
        using F = CalleeFeatures;
        f.callee_ = F::basic |
                    F::callCanceling |
                    /*F::callTimeout |*/ // TODO: Callee-initiated timeouts
                    F::callTrustLevels |
                    F::callerIdentification |
                    F::patternBasedRegistration |
                    F::progressiveCallInvocations |
                    F::progressiveCallResults;
    }

    {
        using F = CallerFeatures;
        f.caller_ = F::basic |
                    F::callCanceling |
                    F::callTimeout |
                    F::callerIdentification |
                    F::progressiveCallInvocations |
                    F::progressiveCallResults;
    }

    {
        using F = PublisherFeatures;
        f.publisher_ = F::basic |
                       F::publisherExclusion |
                       F::publisherIdentification |
                       F::subscriberBlackWhiteListing;
    }

    {
        using F = SubscriberFeatures;
        f.subscriber_ = F::basic |
                        F::patternBasedSubscription |
                        F::publicationTrustLevels |
                        F::publisherIdentification;
    }

    return f;
}

CPPWAMP_INLINE const Object& ClientFeatures::providedRoles()
{
    static const Object roles =
    {
        {"callee", Object{{"features", Object{{
            {"call_canceling",                true},
            {"call_trustlevels",              true},
            {"caller_identification",         true},
            {"pattern_based_registration",    true},
            {"progressive_call_results",      true},
            {"progressive_call_invocations",  true}
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
    Flags<CalleeFeatures> callee, Flags<CallerFeatures> caller,
    Flags<PublisherFeatures> publisher, Flags<SubscriberFeatures> subscriber)
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

CPPWAMP_INLINE Flags<CalleeFeatures>
ClientFeatures::callee() const {return callee_;}

CPPWAMP_INLINE Flags<CallerFeatures>
ClientFeatures::caller() const {return caller_;}

CPPWAMP_INLINE Flags<PublisherFeatures>
ClientFeatures::publisher() const {return publisher_;}

CPPWAMP_INLINE Flags<SubscriberFeatures>
ClientFeatures::subscriber() const {return subscriber_;}

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

template <typename E>
void ClientFeatures::parse(Flags<E>& flags, E pos, const Object* roleDict,
                           const char* featureName)
{
    if (roleDict->count(featureName) != 0)
        flags.set(pos);
}

CPPWAMP_INLINE void ClientFeatures::parseCalleeFeatures(const Object& dict)
{
    using F = CalleeFeatures;
    auto d = findFeaturesDict(dict, "callee");
    if (!d)
        return;
    callee_.set(F::basic, true);
    parse(callee_, F::callCanceling,              d, "call_canceling");
    parse(callee_, F::callTimeout,                d, "call_timeout");
    parse(callee_, F::callTrustLevels,            d, "call_trustlevels");
    parse(callee_, F::callerIdentification,       d, "caller_identification");
    parse(callee_, F::patternBasedRegistration,   d, "pattern_based_registration");
    parse(callee_, F::progressiveCallInvocations, d, "progressive_call_invocations");
    parse(callee_, F::progressiveCallResults,     d, "progressive_call_results");

    // Legacy feature keys
    parse(callee_, F::progressiveCallInvocations, d, "progressive_calls");
}

CPPWAMP_INLINE void ClientFeatures::parseCallerFeatures(const Object& dict)
{
    using F = CallerFeatures;
    auto d = findFeaturesDict(dict, "caller");
    if (!d)
        return;
    caller_.set(F::basic, true);
    parse(caller_, F::callCanceling,              d, "call_canceling");
    parse(caller_, F::callTimeout,                d, "call_timeout");
    parse(caller_, F::callerIdentification,       d, "caller_identification");
    parse(caller_, F::progressiveCallInvocations, d, "progressive_call_invocations");
    parse(caller_, F::progressiveCallResults,     d, "progressive_call_results");

    // Legacy feature keys
    parse(caller_, F::progressiveCallInvocations, d, "progressive_calls");

    // Alternate spelling for call_canceling
    parse(caller_, F::callCanceling,              d, "call_cancelling");
}

CPPWAMP_INLINE void ClientFeatures::parsePublisherFeatures(const Object& dict)
{
    using F = PublisherFeatures;
    auto d = findFeaturesDict(dict, "publisher");
    if (!d)
        return;
    publisher_.set(F::basic, true);
    parse(publisher_, F::publisherExclusion,          d, "publisher_exclusion");
    parse(publisher_, F::publisherIdentification,     d, "publisher_identification");
    parse(publisher_, F::subscriberBlackWhiteListing, d, "subscriber_blackwhite_listing");
}

CPPWAMP_INLINE void ClientFeatures::parseSubscriberFeatures(const Object& dict)
{
    using F = SubscriberFeatures;
    auto d = findFeaturesDict(dict, "subscriber");
    if (!d)
        return;
    subscriber_.set(F::basic, true);
    parse(subscriber_, F::patternBasedSubscription, d, "pattern_based_subscription");
    parse(subscriber_, F::publicationTrustLevels,   d, "publication_trustlevels");
    parse(subscriber_, F::publisherIdentification,  d, "publisher_identification");
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


//******************************************************************************
// RouterFeatures
//******************************************************************************

CPPWAMP_INLINE RouterFeatures RouterFeatures::provided()
{
    RouterFeatures f;

    {
        using F = BrokerFeatures;
        f.broker_ = F::basic |
                    F::patternBasedSubscription |
                    F::publicationTrustLevels |
                    F::publisherExclusion |
                    F::publisherIdentification |
                    F::sessionMetaApi |
                    F::subscriberBlackWhiteListing |
                    F::subscriptionMetaApi;
    }

    {
        using F = DealerFeatures;
        f.dealer_ = F::basic |
                    F::callCanceling |
                    F::callTimeout |
                    F::callTrustLevels |
                    F::callerIdentification |
                    // Not supported: F::patternBasedRegistration |
                    F::progressiveCallInvocations |
                    F::progressiveCallResults |
                    F::registrationMetaApi |
                    F::sessionMetaApi;
    }

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

CPPWAMP_INLINE RouterFeatures::RouterFeatures(Flags<BrokerFeatures> broker,
                                              Flags<DealerFeatures> dealer)
    : broker_(broker),
      dealer_(dealer)
{}

CPPWAMP_INLINE RouterFeatures::RouterFeatures(const Object& dict)
{
    parseBrokerFeatures(dict);
    parseDealerFeatures(dict);
}

CPPWAMP_INLINE Flags<BrokerFeatures>
RouterFeatures::broker() const {return broker_;}

CPPWAMP_INLINE Flags<DealerFeatures>
RouterFeatures::dealer() const {return dealer_;}

CPPWAMP_INLINE bool RouterFeatures::supports(RouterFeatures desired) const
{
    return broker_.all_of(desired.broker_) && dealer_.all_of(desired.dealer_);
}

template <typename E>
void RouterFeatures::parse(Flags<E>& flags, E pos, const Object* roleDict,
                           const char* featureName)
{
    if (roleDict->count(featureName) != 0)
        flags.set(pos, true);
}

CPPWAMP_INLINE void RouterFeatures::parseBrokerFeatures(const Object& dict)
{
    using F = BrokerFeatures;
    auto d = findFeaturesDict(dict, "broker");
    if (!d)
        return;
    broker_.set(F::basic, true);
    parse(broker_, F::patternBasedSubscription,    d, "pattern_based_subscription");
    parse(broker_, F::publicationTrustLevels,      d, "publication_trustlevels");
    parse(broker_, F::publisherExclusion,          d, "publisher_exclusion");
    parse(broker_, F::publisherIdentification,     d, "publisher_identification");
    parse(broker_, F::sessionMetaApi,              d, "session_meta_api");
    parse(broker_, F::subscriberBlackWhiteListing, d, "subscriber_blackwhite_listing");
    parse(broker_, F::subscriptionMetaApi,         d, "subscription_meta_api");
}

CPPWAMP_INLINE void RouterFeatures::parseDealerFeatures(const Object& dict)
{
    using F = DealerFeatures;
    auto d = findFeaturesDict(dict, "dealer");
    if (!d)
        return;
    dealer_.set(F::basic, true);
    parse(dealer_, F::callCanceling,              d, "call_canceling");
    parse(dealer_, F::callTimeout,                d, "call_timeout");
    parse(dealer_, F::callTrustLevels,            d, "call_trustlevels");
    parse(dealer_, F::callerIdentification,       d, "caller_identification");
    parse(dealer_, F::patternBasedRegistration,   d, "pattern_based_registration");
    parse(dealer_, F::progressiveCallInvocations, d, "progressive_call_invocations");
    parse(dealer_, F::progressiveCallResults,     d, "progressive_call_results");
    parse(dealer_, F::registrationMetaApi,        d, "registration_meta_api");
    parse(dealer_, F::sessionMetaApi,             d, "session_meta_api");

    // Legacy feature keys
    parse(dealer_, F::progressiveCallInvocations, d, "progressive_calls");
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

} // namespace wamp
