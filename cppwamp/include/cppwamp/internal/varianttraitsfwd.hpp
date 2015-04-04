/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_VARIANTTRAITSFWD_HPP
#define CPPWAMP_INTERNAL_VARIANTTRAITSFWD_HPP

namespace wamp { namespace internal {

template <typename T> struct FieldTraits;

template <typename T, typename Enable = void> struct ArgTraits;

template <typename T> struct Access;

}}

#endif // CPPWAMP_INTERNAL_VARIANTTRAITSFWD_HPP
