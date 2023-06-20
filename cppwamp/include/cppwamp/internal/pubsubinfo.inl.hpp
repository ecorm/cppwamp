/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../pubsubinfo.hpp"
#include <utility>
#include "../api.hpp"
#include "../exceptions.hpp"
#include "matchpolicyoption.hpp"

namespace wamp
{

//******************************************************************************
// Topic
//******************************************************************************

CPPWAMP_INLINE Topic::Topic(Uri uri)
    : Base(in_place, 0, Object{}, std::move(uri))
{}

CPPWAMP_INLINE const Uri& Topic::uri() const
{
    return message().as<String>(uriPos_);
}

CPPWAMP_INLINE AccessActionInfo Topic::info() const
{
    return {AccessAction::clientSubscribe, requestId(), uri(), options()};
}

/** @details
    This sets the `SUBSCRIBE.Options.match|string` option. */
CPPWAMP_INLINE Topic& Topic::withMatchPolicy(MatchPolicy policy)
{
    internal::setMatchPolicyOption(options(), policy);
    matchPolicy_ = policy;
    return *this;
}

CPPWAMP_INLINE MatchPolicy Topic::matchPolicy() const {return matchPolicy_;}

CPPWAMP_INLINE Topic::Topic(internal::PassKey, internal::Message&& msg)
    : Base(std::move(msg)),
      matchPolicy_(internal::getMatchPolicyOption(options()))
{}

CPPWAMP_INLINE Uri&& Topic::uri(internal::PassKey) &&
{
    return std::move(message().as<String>(uriPos_));
}

CPPWAMP_INLINE void Topic::setTrustLevel(internal::PassKey, TrustLevel)
{
    // Not applicable; do nothing
}


//******************************************************************************
// Pub
//******************************************************************************

CPPWAMP_INLINE Pub::Pub(Uri topic)
    : Base(in_place, 0, Object{}, std::move(topic), Array{}, Object{})
{}

CPPWAMP_INLINE const Uri& Pub::uri() const
{
    return message().as<String>(uriPos_);
}

CPPWAMP_INLINE AccessActionInfo Pub::info() const
{
    return {AccessAction::clientPublish, requestId(), uri(), options()};
}

/** @details
    This sets the `PUBLISH.Options.exclude|list` option. */
CPPWAMP_INLINE Pub& Pub::withExcludedSessions(Array sessionIds)
{
    return withOption("exclude", std::move(sessionIds));
}

/** @details
    This sets the `PUBLISH.Options.exclude_authid|list` option. */
CPPWAMP_INLINE Pub& Pub::withExcludedAuthIds(Array authIds)
{
    return withOption("exclude_authid", std::move(authIds));
}

/** @details
    This sets the `PUBLISH.Options.exclude_authrole|list` option. */
CPPWAMP_INLINE Pub& Pub::withExcludedAuthRoles(Array authRoles)
{
    return withOption("exclude_authrole", std::move(authRoles));
}

/** @details
    This sets the `PUBLISH.Options.eligible|list` option. */
CPPWAMP_INLINE Pub& Pub::withEligibleSessions(Array sessionIds)
{
    return withOption("eligible", std::move(sessionIds));
}

/** @details
    This sets the `PUBLISH.Options.eligible_authid|list` option. */
CPPWAMP_INLINE Pub& Pub::withEligibleAuthIds(Array authIds)
{
    return withOption("eligible_authid", std::move(authIds));
}

/** @details
    This sets the `PUBLISH.Options.eligible_authrole|list` option.  */
CPPWAMP_INLINE Pub& Pub::withEligibleAuthRoles(Array authRoles)
{
    return withOption("eligible_authrole", std::move(authRoles));
}

/** @details
    This sets the `PUBLISH.Options.exclude_me|bool` option. */
CPPWAMP_INLINE Pub& Pub::withExcludeMe(bool excluded)
{
    return withOption("exclude_me", excluded);
}

CPPWAMP_INLINE bool Pub::excludeMe() const
{
    return optionOr<bool>("exclude_me", true);
}

/** @details
    This sets the `PUBLISH.Options.disclose_me|bool` option. */
CPPWAMP_INLINE Pub& Pub::withDiscloseMe(bool disclosed)
{
    return withOption("disclose_me", disclosed);
}

CPPWAMP_INLINE bool Pub::discloseMe() const
{
    return optionOr<bool>("disclose_me", false);
}

CPPWAMP_INLINE Pub::Pub(internal::PassKey, internal::Message&& msg)
    : Base(std::move(msg))
{}

CPPWAMP_INLINE void Pub::setDisclosed(internal::PassKey, bool disclosed)
{
    disclosed_ = disclosed;
}

CPPWAMP_INLINE void Pub::setTrustLevel(internal::PassKey, TrustLevel trustLevel)
{
    trustLevel_ = trustLevel;
    hasTrustLevel_ = true;
}

CPPWAMP_INLINE bool Pub::disclosed(internal::PassKey) const {return disclosed_;}

CPPWAMP_INLINE bool Pub::hasTrustLevel(internal::PassKey) const
{
    return hasTrustLevel_;
}

CPPWAMP_INLINE TrustLevel Pub::trustLevel(internal::PassKey) const
{
    return trustLevel_;
}


//******************************************************************************
// Event
//******************************************************************************

/** @post `this->empty() == true` */
CPPWAMP_INLINE Event::Event()
    : Base(in_place, 0, 0, Object{}, Array{}, Object{})
{}

CPPWAMP_INLINE bool Event::ready() const {return executor_ != nullptr;}

CPPWAMP_INLINE SubscriptionId Event::subscriptionId() const
{
    return message().to<SubscriptionId>(subscriptionIdPos_);
}

CPPWAMP_INLINE PublicationId Event::publicationId() const
{
    return message().to<PublicationId>(publicationIdPos_);
}

/** @returns the same object as Session::fallbackExecutor().
    @pre `this->empty() == false` */
CPPWAMP_INLINE const AnyCompletionExecutor& Event::executor() const
{
    CPPWAMP_LOGIC_CHECK(ready(), "Event has not been initialized");
    return executor_;
}

CPPWAMP_INLINE AccessActionInfo Event::info(Uri topic) const
{
    return {AccessAction::serverEvent, std::move(topic), options()};
}

/** @details
    This function returns the value of the `EVENT.Details.publisher|integer`
    detail.
    @returns The publisher ID, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<SessionId> Event::publisher() const
{
    return toUnsignedInteger("publisher");
}

/** @details
    This function returns the value of the `EVENT.Details.trustlevel|integer`
    detail.
    @returns The trust level, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<TrustLevel> Event::trustLevel() const
{
    return toUnsignedInteger("trustlevel");
}

/** @details
    This function checks the value of the `EVENT.Details.topic|uri` detail.
    @returns The topic URI, if available, or an error code. */
CPPWAMP_INLINE ErrorOr<Uri> Event::topic() const
{
    return optionAs<Uri>("topic");
}

CPPWAMP_INLINE Event::Event(internal::PassKey, internal::Message&& msg)
    : Base(std::move(msg))
{}

CPPWAMP_INLINE Event::Event(internal::PassKey, Pub&& pub, SubscriptionId sid,
                            PublicationId pid)
    : Base(std::move(pub))
{
    message().setKind(internal::MessageKind::event);
    message().at(subscriptionIdPos_) = sid;
    message().at(publicationIdPos_) = pid;
    message().at(optionsPos_) = Object{};

    if (pub.hasTrustLevel({}))
        withOption("trustlevel", pub.trustLevel({}));
}

CPPWAMP_INLINE void Event::setExecutor(internal::PassKey,
                                       AnyCompletionExecutor exec)
{
    executor_ = std::move(exec);
}

CPPWAMP_INLINE void Event::setSubscriptionId(internal::PassKey,
                                             SubscriptionId subId)
{
    message().at(subscriptionIdPos_) = subId;
}

} // namespace wamp
