/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_DISCLOSURE_SETTER_HPP
#define CPPWAMP_INTERNAL_DISCLOSURE_SETTER_HPP

#include "../disclosurerule.hpp"
#include "routersession.hpp"

namespace wamp
{

namespace internal
{
//------------------------------------------------------------------------------
/** Contains information on an operation that is requesting authorization. */
//------------------------------------------------------------------------------
class DisclosureSetter
{
public:
    using Originator = internal::RouterSession;
    using Rule = DisclosureRule;

    template <typename C>
    static bool applyToCommand(
        C& command, internal::RouterSession& originator, Rule realmRule,
        Rule authRule = Rule::preset)
    {
        setDisclosed(command, originator, authRule, realmRule);
    }

private:
    template <typename C>
    static bool setDisclosed(C&, Originator&, Rule, Rule) {return true;}

    static bool setDisclosed(Pub& p, Originator& o, Rule ar, Rule rr)
    {
        return doSetDisclosed(p, o, ar, rr);
    }

    static bool setDisclosed(Rpc& r, Originator& o, Rule ar, Rule rr)
    {
        return doSetDisclosed(r, o, ar, rr);
    }

    template <typename C>
    static bool doSetDisclosed(C& command, Originator& originator,
                               Rule authRule, Rule realmRule)
    {
        auto rule = (authRule == Rule::preset) ? realmRule : authRule;
        bool disclosed = command.discloseMe();
        const bool isStrict = rule == Rule::strictConceal ||
                              rule == Rule::strictReveal;

        if (disclosed && isStrict)
        {
            if (command.wantsAck({}))
            {
                originator.sendRouterCommandError(
                    command, WampErrc::discloseMeDisallowed);
            }
            return false;
        }

        if (rule == Rule::conceal || rule == Rule::strictConceal)
            disclosed = false;
        if (rule == Rule::reveal || rule == Rule::strictReveal)
            disclosed = true;
        command.setDisclosed({}, disclosed);
        return true;
    }
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_DISCLOSURE_SETTER_HPP
