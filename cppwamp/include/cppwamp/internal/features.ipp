/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../features.hpp"
#include "../api.hpp"

namespace wamp
{

//******************************************************************************
// ClientFeatures
//******************************************************************************

CPPWAMP_INLINE ClientFeatures::ForCallee ClientFeatures::ForCallee::provided()
{
    ForCallee f;
    f.supported                  = true;
    f.callCanceling              = true;
    f.callTimeout                = false; // TODO: Callee-initiated timeouts
    f.callTrustLevels            = true;
    f.callerIdentification       = true;
    f.patternBasedRegistration   = true;
    f.progressiveCallInvocations = true;
    f.progressiveCallResults     = true;
    return f;
}

CPPWAMP_INLINE ClientFeatures::ForCaller ClientFeatures::ForCaller::provided()
{
    ForCaller f;
    f.supported                  = true;
    f.callCanceling              = true;
    f.callTimeout                = true;
    f.callerIdentification       = true;
    f.progressiveCallInvocations = true;
    f.progressiveCallResults     = true;
    return f;
}

CPPWAMP_INLINE ClientFeatures::ForPublisher
ClientFeatures::ForPublisher::provided()
{
    ForPublisher f;
    f.supported                   = true;
    f.publisherExclusion          = true;
    f.publisherIdentification     = true;
    f.subscriberBlackWhiteListing = true;
    return f;
};

CPPWAMP_INLINE ClientFeatures::ForSubscriber
ClientFeatures::ForSubscriber::provided()
{
    ForSubscriber f;
    f.supported                = true;
    f.patternBasedSubscription = true;
    f.publicationTrustLevels   = true;
    f.publisherIdentification  = true;
    return f;
};

CPPWAMP_INLINE ClientFeatures ClientFeatures::provided()
{
    ClientFeatures f;
    f.callee_ = ForCallee::provided();
    f.caller_ = ForCaller::provided();
    f.publisher_ = ForPublisher::provided();
    f.subscriber_ = ForSubscriber::provided();
    return f;
}

CPPWAMP_INLINE const Object& ClientFeatures::providedRoles()
{
    // TODO: progressive_call_invocations
    // https://github.com/wamp-proto/wamp-proto/pull/453

    static const Object roles =
    {
        {"callee", Object{{"features", Object{{
            {"call_canceling",                true},
            {"call_trustlevels",              true},
            {"caller_identification",         true},
            {"pattern_based_registration",    true},
            {"progressive_call_results",      true},
            {"progressive_calls",             true}
        }}}}},
        {"caller", Object{{"features", Object{{
            {"call_canceling",                true},
            {"call_timeout",                  true},
            {"caller_identification",         true},
            {"progressive_call_results",      true},
            {"progressive_calls",             true}
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

CPPWAMP_INLINE ClientFeatures::ClientFeatures()
{
    std::memset(this, 0, sizeof(ClientFeatures));
}

CPPWAMP_INLINE ClientFeatures::ClientFeatures(const Object& dict)
{
    parseCalleeFeatures(dict);
    parseCallerFeatures(dict);
    parsePublisherFeatures(dict);
    parseSubscriberFeatures(dict);
}

CPPWAMP_INLINE ClientFeatures::ForCallee ClientFeatures::callee() const
{
    return callee_;
}

CPPWAMP_INLINE ClientFeatures::ForCaller ClientFeatures::caller() const
{
    return caller_;
}

CPPWAMP_INLINE ClientFeatures::ForPublisher ClientFeatures::publisher() const
{
    return publisher_;
}

CPPWAMP_INLINE ClientFeatures::ForSubscriber ClientFeatures::subscriber() const
{
    return subscriber_;
}

CPPWAMP_INLINE bool ClientFeatures::has(const Object* roleDict,
                                        const char* featureName)
{
    return roleDict->count(featureName) != 0;
}

CPPWAMP_INLINE void ClientFeatures::parseCalleeFeatures(const Object& dict)
{
    auto d = getRoleDict(dict, "callee");
    if (!d)
        return;
    callee_.supported = true;
    callee_.callCanceling              = has(d, "call_canceling");
    callee_.callTimeout                = has(d, "call_timeout");
    callee_.callTrustLevels            = has(d, "call_trustlevels");
    callee_.callerIdentification       = has(d, "caller_identification");
    callee_.patternBasedRegistration   = has(d, "pattern_based_registration");
    callee_.progressiveCallInvocations = has(d, "progressive_calls");
    callee_.progressiveCallResults     = has(d, "progressive_call_results");
}

CPPWAMP_INLINE void ClientFeatures::parseCallerFeatures(const Object& dict)
{
    auto d = getRoleDict(dict, "caller");
    if (!d)
        return;
    caller_.supported = true;
    caller_.callCanceling              = has(d, "call_cancelling");
    caller_.callTimeout                = has(d, "call_timeout");
    caller_.callerIdentification       = has(d, "caller_identification");
    caller_.progressiveCallInvocations = has(d, "progressive_calls");
    caller_.progressiveCallResults     = has(d, "progressive_call_results");
}

CPPWAMP_INLINE void ClientFeatures::parsePublisherFeatures(const Object& dict)
{
    auto d = getRoleDict(dict, "publisher");
    if (!d)
        return;
    publisher_.supported = true;
    publisher_.publisherExclusion          = has(d, "publisher_exclusion");
    publisher_.publisherIdentification     = has(d, "publisher_identification");
    publisher_.subscriberBlackWhiteListing = has(d, "subscriber_blackwhite_listing");
}

CPPWAMP_INLINE void ClientFeatures::parseSubscriberFeatures(const Object& dict)
{
    auto d = getRoleDict(dict, "subscriber");
    if (!d)
        return;
    subscriber_.supported = true;
    subscriber_.patternBasedSubscription = has(d, "pattern_based_subscription");
    subscriber_.publicationTrustLevels   = has(d, "publication_trustlevels");
    subscriber_.publisherIdentification  = has(d, "publisher_identification");
}

CPPWAMP_INLINE const Object* ClientFeatures::getRoleDict(const Object& dict,
                                                         const char* roleName)
{
    auto found = dict.find(roleName);
    if (found == dict.end() || !found->second.is<Object>())
        return nullptr;
    return &(found->second.as<Object>());
}


//******************************************************************************
// RouterFeatures
//******************************************************************************

CPPWAMP_INLINE RouterFeatures::ForBroker RouterFeatures::ForBroker::provided()
{
    ForBroker f;
    f.supported                   = true;
    f.patternBasedSubscription    = true;
    f.publicationTrustLevels      = true;
    f.publisherExclusion          = true;
    f.publisherIdentification     = true;
    f.subscriberBlackWhiteListing = true;
    return f;
};

CPPWAMP_INLINE RouterFeatures::ForDealer RouterFeatures::ForDealer::provided()
{
    ForDealer f;
    f.supported                  = true;
    f.callCanceling              = true;
    f.callTimeout                = true;
    f.callTrustLevels            = true;
    f.callerIdentification       = true;
    f.patternBasedRegistration   = false;
    f.progressiveCallInvocations = true;
    f.progressiveCallResults     = true;
    return f;
}


CPPWAMP_INLINE RouterFeatures RouterFeatures::provided()
{
    RouterFeatures f;
    f.broker_ = ForBroker::provided();
    f.dealer_ = ForDealer::provided();
    return f;
}

CPPWAMP_INLINE const Object& RouterFeatures::providedRoles()
{
    // TODO: progressive_call_invocations
    // https://github.com/wamp-proto/wamp-proto/pull/453

    static const Object roles =
    {
        {"dealer", Object{{"features", Object{{
            {"call_canceling",                true},
            {"call_timeout",                  true},
            {"call_trustlevels",              true},
            {"caller_identification",         true},
            {"progressive_calls",             true},
            {"progressive_call_results",      true},
        }}}}},
        {"broker", Object{{"features", Object{{
            {"pattern_based_subscription",    true},
            {"publisher_exclusion",           true},
            {"publisher_identification",      true},
            {"subscriber_blackwhite_listing", true}
        }}}}}
    };
    return roles;
}

CPPWAMP_INLINE RouterFeatures::RouterFeatures()
{
    std::memset(this, 0, sizeof(RouterFeatures));
}

CPPWAMP_INLINE RouterFeatures::RouterFeatures(const Object& dict)
{
    parseBrokerFeatures(dict);
    parseDealerFeatures(dict);
}

CPPWAMP_INLINE RouterFeatures::ForBroker RouterFeatures::broker() const
{
    return broker_;
}

CPPWAMP_INLINE RouterFeatures::ForDealer RouterFeatures::dealer() const
{
    return dealer_;
}

CPPWAMP_INLINE bool RouterFeatures::has(const Object* roleDict,
                                        const char* featureName)
{
    return roleDict->count(featureName) != 0;
}

CPPWAMP_INLINE void RouterFeatures::parseBrokerFeatures(const Object& dict)
{
    auto d = getRoleDict(dict, "broker");
    if (!d)
        return;
    broker_.supported = true;
    broker_.patternBasedSubscription    = has(d, "pattern_based_subscription");
    broker_.publicationTrustLevels      = has(d, "publication_trustlevels");
    broker_.publisherExclusion          = has(d, "publisher_exclusion");
    broker_.publisherIdentification     = has(d, "publisher_identification");
    broker_.subscriberBlackWhiteListing = has(d, "subscriber_blackwhite_listing");
}

CPPWAMP_INLINE void RouterFeatures::parseDealerFeatures(const Object& dict)
{
    auto d = getRoleDict(dict, "dealer");
    if (!d)
        return;
    dealer_.supported = true;
    dealer_.callCanceling              = has(d, "call_canceling");
    dealer_.callTimeout                = has(d, "call_timeout");
    dealer_.callTrustLevels            = has(d, "call_trustlevels");
    dealer_.callerIdentification       = has(d, "caller_identification");
    dealer_.patternBasedRegistration   = has(d, "pattern_based_registration");
    dealer_.progressiveCallInvocations = has(d, "progressive_calls");
    dealer_.progressiveCallResults     = has(d, "progressive_call_results");
}

CPPWAMP_INLINE const Object* RouterFeatures::getRoleDict(const Object& dict,
                                                         const char* roleName)
{
    auto found = dict.find(roleName);
    if (found == dict.end() || !found->second.is<Object>())
        return nullptr;
    return &(found->second.as<Object>());
}

} // namespace wamp
