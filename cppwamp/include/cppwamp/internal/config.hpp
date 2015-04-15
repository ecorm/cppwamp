/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CONFIG_HPP
#define CPPWAMP_INTERNAL_CONFIG_HPP

#include <boost/config.hpp>

#ifdef CPPWAMP_COMPILED_LIB
#define CPPWAMP_INLINE
#else
#define CPPWAMP_INLINE inline
#endif

#ifdef __WIN32
#define CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS 0
#else
#define CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS 1
#endif

#ifdef BOOST_NO_CXX11_REF_QUALIFIERS
#define CPPWAMP_HAS_REF_QUALIFIERS 0
#else
#define CPPWAMP_HAS_REF_QUALIFIERS 1
#endif

#endif // CPPWAMP_INTERNAL_CONFIG_HPP
