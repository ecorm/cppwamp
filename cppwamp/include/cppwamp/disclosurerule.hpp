/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_DISCLOSURE_RULE_HPP
#define CPPWAMP_DISCLOSURE_RULE_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the DisclosureRule enumeration. */
//------------------------------------------------------------------------------

namespace wamp
{

//------------------------------------------------------------------------------
/** Determines how callers and publishers are disclosed. */
//------------------------------------------------------------------------------
enum class DisclosureRule
{
    preset,       ///< Reveal originator as per the realm configuration preset.
    originator,   ///< Reveal originator as per its `disclose_me` option.
    reveal,       ///< Reveal originator even if disclosure was not requested.
    conceal,      ///< Conceal originator even if disclosure was requested.
    strictReveal, ///< Reveal originator and disallow `disclose_me` option.
    strictConceal ///< Conceal originator and disallow `disclose_me` option.
};

} // namespace wamp


#endif // CPPWAMP_DISCLOSURE_RULE_HPP
