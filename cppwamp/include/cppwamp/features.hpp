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
#include "variantdefs.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Flags representing callee features. */
//------------------------------------------------------------------------------
enum class CalleeFeatures : uint16_t
{
    basic                       = 1u << 0,
    callCanceling               = 1u << 1,
    callTimeout                 = 1u << 2,
    callTrustLevels             = 1u << 3,
    callerIdentification        = 1u << 4,
    patternBasedRegistration    = 1u << 5,
    progressiveCallInvocations  = 1u << 6,
    progressiveCallResults      = 1u << 7
};

//------------------------------------------------------------------------------
/** Flags representing caller features. */
//------------------------------------------------------------------------------
enum class CallerFeatures : uint16_t
{
    basic                       = 1u << 0,
    callCanceling               = 1u << 1,
    callTimeout                 = 1u << 2,
    callerIdentification        = 1u << 3,
    progressiveCallInvocations  = 1u << 4,
    progressiveCallResults      = 1u << 5
};

//------------------------------------------------------------------------------
/** Flags representing publisher features. */
//------------------------------------------------------------------------------
enum class PublisherFeatures : uint16_t
{
    basic                       = 1u << 0,
    publisherExclusion          = 1u << 1,
    publisherIdentification     = 1u << 2,
    subscriberBlackWhiteListing = 1u << 3
};

//------------------------------------------------------------------------------
/** Flags representing subscriber features. */
//------------------------------------------------------------------------------
enum class SubscriberFeatures : uint16_t
{
    basic                    = 1u << 0,
    patternBasedSubscription = 1u << 1,
    publicationTrustLevels   = 1u << 2,
    publisherIdentification  = 1u << 3
};

template <> struct IsFlag<CalleeFeatures> : TrueType {};
template <> struct IsFlag<CallerFeatures> : TrueType {};
template <> struct IsFlag<PublisherFeatures> : TrueType {};
template <> struct IsFlag<SubscriberFeatures> : TrueType {};

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
    ClientFeatures(Flags<CalleeFeatures> callee,
                   Flags<CallerFeatures> caller,
                   Flags<PublisherFeatures> publisher,
                   Flags<SubscriberFeatures> subscriber);

    /** Constructor taking a roles dictionary to be parsed for features. */
    ClientFeatures(const Object& dict);

    /** Obtains the callee feature flags. */
    Flags<CalleeFeatures> callee() const;

    /** Obtains the caller feature flags. */
    Flags<CallerFeatures> caller() const;

    /** Obtains the publisher feature flags. */
    Flags<PublisherFeatures> publisher() const;

    /** Obtains the subscriber feature flags. */
    Flags<SubscriberFeatures> subscriber() const;

    /** Checks if this instance contains all the given desired features. */
    bool supports(ClientFeatures desired) const;

private:
    template <typename E>
    static void parse(Flags<E>& flags, E pos, const Object* roleDict,
                      const char* featureName);

    void parseCalleeFeatures(const Object& dict);
    void parseCallerFeatures(const Object& dict);
    void parsePublisherFeatures(const Object& dict);
    void parseSubscriberFeatures(const Object& dict);
    const Object* getRoleDict(const Object& dict, const char* roleName);

    Flags<CalleeFeatures> callee_;
    Flags<CallerFeatures> caller_;
    Flags<PublisherFeatures> publisher_;
    Flags<SubscriberFeatures> subscriber_;
};


//------------------------------------------------------------------------------
/** Flags representing broker features. */
//------------------------------------------------------------------------------
enum class BrokerFeatures : uint16_t
{
    basic                       = 1u << 0,
    patternBasedSubscription    = 1u << 1,
    publicationTrustLevels      = 1u << 2,
    publisherExclusion          = 1u << 3,
    publisherIdentification     = 1u << 4,
    subscriberBlackWhiteListing = 1u << 5
};

//------------------------------------------------------------------------------
/** Flags representing dealer features. */
//------------------------------------------------------------------------------
enum class DealerFeatures : uint16_t
{
    basic                      = 1u << 0,
    callCanceling              = 1u << 1,
    callTimeout                = 1u << 2,
    callTrustLevels            = 1u << 3,
    callerIdentification       = 1u << 4,
    patternBasedRegistration   = 1u << 5,
    progressiveCallInvocations = 1u << 6,
    progressiveCallResults     = 1u << 7
};

template <> struct IsFlag<BrokerFeatures> : TrueType {};
template <> struct IsFlag<DealerFeatures> : TrueType {};


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
    RouterFeatures(Flags<BrokerFeatures> broker, Flags<DealerFeatures> dealer);

    /** Constructor taking a roles dictionary to be parsed for features. */
    RouterFeatures(const Object& dict);

    /** Obtains the broker feature flags. */
    Flags<BrokerFeatures> broker() const;

    /** Obtains the dealer feature flags. */
    Flags<DealerFeatures> dealer() const;

    /** Checks if this instance contains all the given desired features. */
    bool supports(RouterFeatures desired) const;

private:
    template <typename E>
    static void parse(Flags<E>& flags, E pos, const Object* roleDict,
                      const char* featureName);

    void parseBrokerFeatures(const Object& dict);
    void parseDealerFeatures(const Object& dict);
    const Object* getRoleDict(const Object& dict, const char* roleName);

    Flags<BrokerFeatures> broker_;
    Flags<DealerFeatures> dealer_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/features.ipp"
#endif

#endif // CPPWAMP_FEATURES_HPP
