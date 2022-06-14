/*------------------------------------------------------------------------------
              Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_CONFIG_HPP
#define CPPWAMP_INTERNAL_CONFIG_HPP

#ifdef __WIN32
#define CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS 0
#else
#define CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS 1
#endif

// Performs move capture if available, otherwise performs copy capture
#if defined(__cpp_init_captures) && __cpp_init_captures >= 201304
#define CPPWAMP_MVCAP(x) x=std::move(x)
#else
#define CPPWAMP_MVCAP(x) x
#endif

#endif // CPPWAMP_INTERNAL_CONFIG_HPP
