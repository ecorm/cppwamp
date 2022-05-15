/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_MSGPACK_API_HPP
#define CPPWAMP_MSGPACK_API_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Defines macros related to exporting/importing APIs. */
//------------------------------------------------------------------------------

#include "../api.hpp"

#if defined(CPPWAMP_COMPILED_LIB) && !defined(CPPWAMP_IS_STATIC)
#    ifdef cppwamp_msgpack_EXPORTS // We are building this library
#        define CPPWAMP_MSGPACK_API CPPWAMP_API_EXPORT
#    else // We are using this library
#        define CPPWAMP_MSGPACK_API CPPWAMP_API_IMPORT
#    endif
#else
#   define CPPWAMP_MSGPACK_API
#endif

#endif // CPPWAMP_MSGPACK_API_HPP
