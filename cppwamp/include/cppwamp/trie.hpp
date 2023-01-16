/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRIE_HPP
#define CPPWAMP_TRIE_HPP

#ifdef CPPWAMP_WITHOUT_BUNDLED_TESSIL_HTRIE
#include <tsl/htrie_map.h>
#else
#include "bundled/tessil_htrie/htrie_map.h"
#endif

namespace wamp
{

//------------------------------------------------------------------------------
template <class CharT, class T, class Hash = tsl::ah::str_hash<CharT>,
          class KeySizeT = std::uint16_t>
using BasicTrieMap = tsl::htrie_map<CharT, T, Hash, KeySizeT>;

//------------------------------------------------------------------------------
template <typename T>
using TrieMap = BasicTrieMap<char, T>;

} // namespace wamp

#endif // CPPWAMP_TRIE_HPP
