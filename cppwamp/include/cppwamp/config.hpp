/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CONFIG_HPP
#define CPPWAMP_CONFIG_HPP

#ifndef _WIN32
#define CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS 1
#endif

#if defined(__cpp_inline_variables) || defined(CPPWAMP_FOR_DOXYGEN)
#define CPPWAMP_INLINE_VARIABLE inline
#else
#define CPPWAMP_INLINE_VARIABLE
#endif

#if (defined(__has_cpp_attribute) && __has_cpp_attribute(nodiscard)) \
    || defined(CPPWAMP_FOR_DOXYGEN)
#define CPPWAMP_NODISCARD [[nodiscard]]
#else
#define CPPWAMP_NODISCARD
#endif

#if (defined(__has_cpp_attribute) && __has_cpp_attribute(deprecated)) \
    || defined(CPPWAMP_FOR_DOXYGEN)
#define CPPWAMP_DEPRECATED [[deprecated]]
#else
#define CPPWAMP_DEPRECATED
#endif

#endif // CPPWAMP_CONFIG_HPP
