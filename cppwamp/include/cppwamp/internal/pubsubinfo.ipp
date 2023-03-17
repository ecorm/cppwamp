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

CPPWAMP_INLINE Topic::Topic(String uri) : Base(std::move(uri)) {}

CPPWAMP_INLINE const String& Topic::uri() const {return message().uri();}

CPPWAMP_INLINE AccessActionInfo Topic::info() const
{
    return {AccessAction::clientSubscribe, message().requestId(), uri(),
            options()};
}

/** @details
    This sets the `SUBSCRIBE.Options.match|string` option. */
CPPWAMP_INLINE Topic& Topic::withMatchPolicy(MatchPolicy policy)
{
    internal::setMatchPolicyOption(*this, policy);
    matchPolicy_ = policy;
    return *this;
}

CPPWAMP_INLINE MatchPolicy Topic::matchPolicy() const {return matchPolicy_;}

CPPWAMP_INLINE Topic::Topic(internal::PassKey, internal::SubscribeMessage&& msg)
    : Base(std::move(msg))
{
    matchPolicy_ = internal::getMatchPolicyOption(*this);
}

CPPWAMP_INLINE RequestId Topic::requestId(internal::PassKey) const
{
    return message().requestId();
}

CPPWAMP_INLINE String&& Topic::uri(internal::PassKey) &&
{
    return std::move(message()).uri();
}

//******************************************************************************
// Pub
//******************************************************************************

CPPWAMP_INLINE Pub::Pub(String topic) : Base(std::move(topic)) {}

CPPWAMP_INLINE const String& Pub::uri() const {return message().uri();}

CPPWAMP_INLINE AccessActionInfo Pub::info() const
{
    return {AccessAction::clientPublish, message().requestId(), uri(),
            options()};
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

CPPWAMP_INLINE Pub::Pub(internal::PassKey, internal::PublishMessage&& msg)
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

CPPWAMP_INLINE RequestId Pub::requestId(internal::PassKey) const
{
    return message().requestId();
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
CPPWAMP_INLINE Event::Event() {}

CPPWAMP_INLINE Event::Event(PublicationId pubId, Object opts)
    : Base(pubId, std::move(opts))
{}

CPPWAMP_INLINE Event& Event::withSubscriptionId(SubscriptionId subId)
{
    Base::message().setSubscriptionId(subId);
    return *this;
}

CPPWAMP_INLINE bool Event::empty() const {return executor_ == nullptr;}

CPPWAMP_INLINE SubscriptionId Event::subId() const
{
    return message().subscriptionId();
}

CPPWAMP_INLINE PublicationId Event::pubId() const
{
    return message().publicationId();
}

/** @returns the same object as Session::fallbackExecutor().
    @pre `this->empty() == false` */
CPPWAMP_INLINE const AnyCompletionExecutor& Event::executor() const
{
    CPPWAMP_LOGIC_CHECK(!empty(), "Event is empty");
    return executor_;
}

CPPWAMP_INLINE AccessActionInfo Event::info(String topic) const
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
CPPWAMP_INLINE ErrorOr<String> Event::topic() const
{
    return optionAs<String>("topic");
}

CPPWAMP_INLINE Event::Event(internal::PassKey, AnyCompletionExecutor executor,
                            internal::EventMessage&& msg)
    : Base(std::move(msg)),
      executor_(executor)
{}

CPPWAMP_INLINE Event::Event(internal::PassKey, Pub&& pub, SubscriptionId sid,
                            PublicationId pid)
    : Base(std::move(pub.message({})).fields(), sid, pid)
{
    withOptions({});
    if (pub.hasTrustLevel({}))
        withOption("trustlevel", pub.trustLevel({}));
}

} // namespace wamp
