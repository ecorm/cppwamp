/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_PUBSUBINFO_HPP
#define CPPWAMP_PUBSUBINFO_HPP

#include "accesslogging.hpp"
#include "api.hpp"
#include "anyhandler.hpp"
#include "erroror.hpp"
#include "options.hpp"
#include "payload.hpp"
#include "variant.hpp"
#include "wampdefs.hpp"
#include "internal/message.hpp"
#include "internal/passkey.hpp"

//------------------------------------------------------------------------------
/** @file
    @brief Provides data structures for information exchanged via WAMP
           pub-sub messages. */
//------------------------------------------------------------------------------

namespace wamp
{

//------------------------------------------------------------------------------
/** Provides the topic URI and other options contained within WAMP
    `SUBSCRIBE' messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Topic
    : public Options<Topic, internal::MessageKind::subscribe>
{
public:
    /** Converting constructor taking a topic URI. */
    Topic(Uri uri); // NOLINT(google-explicit-constructor)

    /** Obtains the topic URI. */
    const Uri& uri() const;

    /** Obtains information for the access log. */
    AccessActionInfo info() const;

    /** @name Pattern-based Subscription
        See [Pattern-based Subscription in the WAMP Specification]
        (https://wamp-proto.org/wamp_latest_ietf.html#name-pattern-based-subscription)
        @{ */

    /** Sets the matching policy to be used for this subscription. */
    Topic& withMatchPolicy(MatchPolicy);

    /** Obtains the matching policy used for this subscription. */
    MatchPolicy matchPolicy() const;
    /// @}

private:
    static constexpr unsigned uriPos_ = 3;

    using Base = Options<Topic, internal::MessageKind::subscribe>;

    MatchPolicy matchPolicy_ = MatchPolicy::exact;

public:
    // Internal use only
    Topic(internal::PassKey, internal::Message&& msg);
    Uri&& uri(internal::PassKey) &&;
    void setTrustLevel(internal::PassKey, TrustLevel);
};


//------------------------------------------------------------------------------
/** Provides the topic URI, options, and payload contained within WAMP
    `PUBLISH` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Pub : public Payload<Pub, internal::MessageKind::publish>
{
public:
    /** Converting constructor taking a topic URI. */
    Pub(Uri topic); // NOLINT(google-explicit-constructor)

    /** Obtains the topic URI. */
    const Uri& uri() const;

    /** Obtains information for the access log. */
    AccessActionInfo info() const;

    /** @name Subscriber Allow/Deny Lists
        See [Subscriber Black- and Whitelisting in the WAMP Specification]
        (https://wamp-proto.org/wamp_latest_ietf.html#name-subscriber-black-and-whitel)
        @{ */

    /** Specifies the list of (potential) _Subscriber_ session IDs that
        won't receive the published event. */
    Pub& withExcludedSessions(Array sessionIds);

    /** Specifies a deny list of authid strings. */
    Pub& withExcludedAuthIds(Array authIds);

    /** Specifies a deny list of authrole strings. */
    Pub& withExcludedAuthRoles(Array authRoles);

    /** Specifies the list of (potential) _Subscriber_ session IDs that
        are allowed to receive the published event. */
    Pub& withEligibleSessions(Array sessionIds);

    /** Specifies an allow list of authid strings. */
    Pub& withEligibleAuthIds(Array authIds);

    /** Specifies an allow list of authrole strings. */
    Pub& withEligibleAuthRoles(Array authRoles);
    /// @}

    /** @name Publisher Exclusion
        See [Publisher Exclusion in the WAMP Specification]
        (https://wamp-proto.org/wamp_latest_ietf.html#name-publisher-exclusion)
        @{ */

    /** Specifies if this session should be excluded from receiving the
        event. */
    Pub& withExcludeMe(bool excluded = true);

    /** Determines if this session should be excluded from receiving the
        event. */
    bool excludeMe() const;
    /// @}

    /** @name Publisher Identification
        See [Publisher Identification in the WAMP Specification]
        (https://wamp-proto.org/wamp_latest_ietf.html#name-publisher-identification)
        @{ */

    /** Requests that the identity of the publisher be disclosed
        in the event. */
    Pub& withDiscloseMe(bool disclosed = true);

    /** Determines if publisher disclosure was requested. */
    bool discloseMe() const;
    /// @}

private:
    static constexpr unsigned uriPos_  = 3;
    static constexpr unsigned argsPos_ = 4;

    using Base = Payload<Pub, internal::MessageKind::publish>;

    TrustLevel trustLevel_ = 0;
    bool hasTrustLevel_ = false;
    bool disclosed_ = false;

public:
    // Internal use only
    Pub(internal::PassKey, internal::Message&& msg);
    void setDisclosed(internal::PassKey, bool disclosed);
    void setTrustLevel(internal::PassKey, TrustLevel trustLevel);
    bool disclosed(internal::PassKey) const;
    bool hasTrustLevel(internal::PassKey) const;
    TrustLevel trustLevel(internal::PassKey) const;
};


//------------------------------------------------------------------------------
/** Provides the subscription/publication ids, options, and payload contained
    within WAMP `EVENT` messages. */
//------------------------------------------------------------------------------
class CPPWAMP_API Event : public Payload<Event, internal::MessageKind::event>
{
public:
    /** Default constructor. */
    Event();

    /** Determines if the Event has been initialized and is ready for use. */
    bool ready() const;

    /** Obtains the subscription ID associated with this event. */
    SubscriptionId subscriptionId() const;

    /** Obtains the publication ID associated with this event. */
    PublicationId publicationId() const;

    /** Obtains the executor used to execute user-provided handlers. */
    const AnyCompletionExecutor& executor() const;

    /** Obtains information for the access log. */
    AccessActionInfo info(Uri topic = {}) const;

    /** @name Publisher Identification
        See [Publisher Identification in the WAMP Specification]
        (https://wamp-proto.org/wamp_latest_ietf.html#name-publisher-identification)
        @{ */

    /** Obtains the publisher ID integer. */
    ErrorOr<SessionId> publisher() const;

    /** Obtains the publisher authid string. */
    ErrorOr<String> publisherAuthId() const;

    /** Obtains the publisher authrole string. */
    ErrorOr<String> publisherAuthRole() const;
    /// @}

    /** @name Publication Trust Levels
        See [Publication Trust Levels in the WAMP Specification]
        (https://wamp-proto.org/wamp_latest_ietf.html#name-publication-trust-levels)
        @{ */

    /** Obtains the trust level integer. */
    ErrorOr<TrustLevel> trustLevel() const;
    /// @}

    /** @name Pattern-based Subscription
        See [Pattern-based Subscription in the WAMP Specification]
        (https://wamp-proto.org/wamp_latest_ietf.html#name-pattern-based-subscription)
        @{ */

    /** Obtains the original topic URI string used to make the publication. */
    ErrorOr<Uri> topic() const;
    /// @}

private:
    static constexpr unsigned subscriptionIdPos_ = 1;
    static constexpr unsigned publicationIdPos_  = 2;
    static constexpr unsigned optionsPos_        = 3;

    using Base = Payload<Event, internal::MessageKind::event>;

    AnyCompletionExecutor executor_;

public:
    // Internal use only
    Event(internal::PassKey, internal::Message&& msg);

    Event(internal::PassKey, Pub&& pub, SubscriptionId sid, PublicationId pid);

    void setExecutor(internal::PassKey, AnyCompletionExecutor exec);

    void setSubscriptionId(internal::PassKey, SubscriptionId subId);
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/pubsubinfo.inl.hpp"
#endif

#endif // CPPWAMP_PUBSUBINFO_HPP
