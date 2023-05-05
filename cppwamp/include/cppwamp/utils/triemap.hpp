/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_UTILS_TRIEMAP_HPP
#define CPPWAMP_UTILS_TRIEMAP_HPP

#ifdef CPPWAMP_WITHOUT_BUNDLED_TESSIL_HTRIE
#include <tsl/htrie_map.h>
#else
#include "../bundled/tessil_htrie/htrie_map.h"
#endif

namespace wamp
{

namespace utils
{

//------------------------------------------------------------------------------
template <class CharT, class T, class Hash = tsl::ah::str_hash<CharT>,
          class KeySizeT = std::uint16_t>
using BasicTrieMap = tsl::htrie_map<CharT, T, Hash, KeySizeT>;

//------------------------------------------------------------------------------
template <typename T>
using TrieMap = BasicTrieMap<char, T, tsl::ah::str_hash<char>, std::uint32_t>;

} // namespace utils

} // namespace wamp

#endif // CPPWAMP_UTILS_TRIEMAP_HPP
