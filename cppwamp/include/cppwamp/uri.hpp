/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2016, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_URI_HPP
#define CPPWAMP_URI_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for processing URIs. */
//------------------------------------------------------------------------------

#include <string>
#include <vector>

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains a URI split into its constituent labels. */
//------------------------------------------------------------------------------
using SplitUri = std::vector<std::string>;

//------------------------------------------------------------------------------
/** Splits a URI into its constituent labels. */
//------------------------------------------------------------------------------
SplitUri tokenizeUri(const std::string uri);

//------------------------------------------------------------------------------
/** Recombines split labels into an URI. */
//------------------------------------------------------------------------------
std::string untokenizeUri(const SplitUri& labels);

//------------------------------------------------------------------------------
/** Determines if the given URI matches the given wildcard pattern. */
//------------------------------------------------------------------------------
bool uriMatchesWildcardPattern(const SplitUri& uri, const SplitUri& pattern);

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "./internal/uri.ipp"
#endif

#endif // CPPWAMP_URI_HPP
