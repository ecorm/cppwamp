/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_FEATURES_HPP
#define CPPWAMP_FEATURES_HPP

#include <cstddef>
#include "api.hpp"
#include "flags.hpp"
#include "variant.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Flags representing features. */
//------------------------------------------------------------------------------
enum class Feature : uint32_t
{
    basic                       = flag_bit(0),
    callCanceling               = flag_bit(1),
    callReroute                 = flag_bit(2),
    callTimeout                 = flag_bit(3),
    callTrustLevels             = flag_bit(4),
    callerIdentification        = flag_bit(5),
    eventHistory                = flag_bit(6),
    patternBasedRegistration    = flag_bit(7),
    patternBasedSubscription    = flag_bit(8),
    payloadPassthruMode         = flag_bit(9),
    procedureReflection         = flag_bit(10),
    progressiveCallInvocations  = flag_bit(11),
    progressiveCallResults      = flag_bit(12),
    publicationTrustLevels      = flag_bit(13),
    publisherExclusion          = flag_bit(14),
    publisherIdentification     = flag_bit(15),
    registrationMetaApi         = flag_bit(16),
    registrationRevocation      = flag_bit(17),
    sessionMetaApi              = flag_bit(18),
    shardedRegistration         = flag_bit(19),
    shardedSubscription         = flag_bit(20),
    sharedRegistration          = flag_bit(21),
    subscriberBlackWhiteListing = flag_bit(22),
    subscriptionMetaApi         = flag_bit(23),
    topicReflection             = flag_bit(24)
};

template <> struct is_flag<Feature> : TrueType {};

using FeatureFlags = Flags<Feature>;

//------------------------------------------------------------------------------
/** Identifies the features supported by a WAMP client.

    See [Feature Announcement][1] in the WAMP specification.
    [1]: https://wamp-proto.org/wamp_latest_ietf.html#name-feature-announcement */
//------------------------------------------------------------------------------
class CPPWAMP_API ClientFeatures
{
public:
    /** Obtains the set of client features provided by this installation
        of CppWAMP. */
    static ClientFeatures provided();

    /** Obtains the roles dictionary of client features provided by this
        installation of CppWAMP. */
    static const Object& providedRoles();

    /** Default-constructs an instance with all feature bits cleared. */
    ClientFeatures();

    /** Constructor taking feature flags for each client role. */
    ClientFeatures(FeatureFlags callee, FeatureFlags caller,
                   FeatureFlags publisher, FeatureFlags subscriber);

    /** Constructor taking a roles dictionary to be parsed for features. */
    explicit ClientFeatures(const Object& dict);

    /** Obtains the callee feature flags. */
    FeatureFlags callee() const;

    /** Obtains the caller feature flags. */
    FeatureFlags caller() const;

    /** Obtains the publisher feature flags. */
    FeatureFlags publisher() const;

    /** Obtains the subscriber feature flags. */
    FeatureFlags subscriber() const;

    /** Checks if this instance contains all the given desired features. */
    bool supports(ClientFeatures desired) const;

    /** Clears all feature bits, as if default-constructed. */
    void reset();

private:
    static const Object* findFeaturesDict(const Object& dict,
                                          const char* roleName);

    static void parse(FeatureFlags& flags, Feature pos, const Object* roleDict,
                      const char* featureName);

    void parseCalleeFeatures(const Object& dict);
    void parseCallerFeatures(const Object& dict);
    void parsePublisherFeatures(const Object& dict);
    void parseSubscriberFeatures(const Object& dict);

    FeatureFlags callee_;
    FeatureFlags caller_;
    FeatureFlags publisher_;
    FeatureFlags subscriber_;
};


//------------------------------------------------------------------------------
/** Identifies the features supported by the a WAMP router.

    See [Feature Announcement][1] in the WAMP specification.
    [1]: https://wamp-proto.org/wamp_latest_ietf.html#name-feature-announcement */
//------------------------------------------------------------------------------
class CPPWAMP_API RouterFeatures
{
public:
    /** Obtains the set of router features provided by this installation
        of CppWAMP. */
    static RouterFeatures provided();

    /** Obtains the roles dictionary of router features provided by this
        installation of CppWAMP. */
    static const Object& providedRoles();

    /** Default-constructs an instance with all feature bits cleared. */
    RouterFeatures();

    /** Constructor taking feature flags for each router role. */
    RouterFeatures(FeatureFlags broker, FeatureFlags dealer);

    /** Constructor taking a roles dictionary to be parsed for features. */
    explicit RouterFeatures(const Object& dict);

    /** Obtains the broker feature flags. */
    FeatureFlags broker() const;

    /** Obtains the dealer feature flags. */
    FeatureFlags dealer() const;

    /** Checks if this instance contains all the given desired features. */
    bool supports(RouterFeatures desired) const;

private:
    static void parse(FeatureFlags& flags, Feature pos, const Object* roleDict,
                      const char* featureName);

    static const Object* findFeaturesDict(const Object& dict,
                                          const char* roleName);

    void parseBrokerFeatures(const Object& dict);

    void parseDealerFeatures(const Object& dict);

    FeatureFlags broker_;
    FeatureFlags dealer_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/features.inl.hpp"
#endif

#endif // CPPWAMP_FEATURES_HPP
