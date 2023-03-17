/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_FEATURES_HPP
#define CPPWAMP_FEATURES_HPP

#include <cstring>
#include "api.hpp"
#include "variantdefs.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains bitfields identifying the features supported by the a WAMP client.

    See [Feature Announcement][1] in the WAMP specification.
    [1]: https://wamp-proto.org/wamp_latest_ietf.html#name-feature-announcement */
//------------------------------------------------------------------------------
class CPPWAMP_API ClientFeatures
{
public:
    struct ForCallee
    {
        bool supported                  : 1;
        bool callCanceling              : 1;
        bool callTimeout                : 1;
        bool callTrustLevels            : 1;
        bool callerIdentification       : 1;
        bool patternBasedRegistration   : 1;
        bool progressiveCallInvocations : 1;
        bool progressiveCallResults     : 1;

        static ForCallee provided();
    };

    struct ForCaller
    {
        bool supported                  : 1;
        bool callCanceling              : 1;
        bool callTimeout                : 1;
        bool callerIdentification       : 1;
        bool progressiveCallInvocations : 1;
        bool progressiveCallResults     : 1;

        static ForCaller provided();
    };

    struct ForPublisher
    {
        bool supported                   : 1;
        bool publisherExclusion          : 1;
        bool publisherIdentification     : 1;
        bool subscriberBlackWhiteListing : 1;

        static ForPublisher provided();
    };

    struct ForSubscriber
    {
        bool supported                : 1;
        bool patternBasedSubscription : 1;
        bool publicationTrustLevels   : 1;
        bool publisherIdentification  : 1;

        static ForSubscriber provided();
    };

    static ClientFeatures provided();

    static const Object& providedRoles();

    ClientFeatures();

    ClientFeatures(const Object& dict);

    ForCallee callee() const;

    ForCaller caller() const;

    ForPublisher publisher() const;

    ForSubscriber subscriber() const;

private:
    static bool has(const Object* roleDict, const char* featureName);
    void parseCalleeFeatures(const Object& dict);
    void parseCallerFeatures(const Object& dict);
    void parsePublisherFeatures(const Object& dict);
    void parseSubscriberFeatures(const Object& dict);
    const Object* getRoleDict(const Object& dict, const char* roleName);

    ForCallee callee_;
    ForCaller caller_;
    ForPublisher publisher_;
    ForSubscriber subscriber_;
};


//------------------------------------------------------------------------------
/** Contains bitfields identifying the features supported by the a WAMP router.

    See [Feature Announcement][1] in the WAMP specification.
    [1]: https://wamp-proto.org/wamp_latest_ietf.html#name-feature-announcement */
//------------------------------------------------------------------------------
class CPPWAMP_API RouterFeatures
{
public:
    struct ForBroker
    {
        bool supported                   : 1;
        bool patternBasedSubscription    : 1;
        bool publicationTrustLevels      : 1;
        bool publisherExclusion          : 1;
        bool publisherIdentification     : 1;
        bool subscriberBlackWhiteListing : 1;

        static ForBroker provided();
    };

    struct ForDealer
    {
        bool supported                  : 1;
        bool callCanceling              : 1;
        bool callTimeout                : 1;
        bool callTrustLevels            : 1;
        bool callerIdentification       : 1;
        bool patternBasedRegistration   : 1;
        bool progressiveCallInvocations : 1;
        bool progressiveCallResults     : 1;

        static ForDealer provided();
    };

    static RouterFeatures provided();

    static const Object& providedRoles();

    RouterFeatures();

    RouterFeatures(const Object& dict);

    ForBroker broker() const;

    ForDealer dealer() const;

private:
    static bool has(const Object* roleDict, const char* featureName);
    void parseBrokerFeatures(const Object& dict);
    void parseDealerFeatures(const Object& dict);
    const Object* getRoleDict(const Object& dict, const char* roleName);

    ForBroker broker_;
    ForDealer dealer_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/features.ipp"
#endif

#endif // CPPWAMP_FEATURES_HPP
