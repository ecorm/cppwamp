/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <cppwamp/internal/config.hpp>

#ifdef CPPWAMP_COMPILED_LIB

#include <cppwamp/dialoguedata.hpp>
#include <cppwamp/error.hpp>
#include <cppwamp/legacytcpconnector.hpp>
#include <cppwamp/registration.hpp>
#include <cppwamp/session.hpp>
#include <cppwamp/sessiondata.hpp>
#include <cppwamp/subscription.hpp>
#include <cppwamp/tcpconnector.hpp>
#include <cppwamp/version.hpp>
#include <cppwamp/internal/messagetraits.hpp>

#include <cppwamp/internal/dialoguedata.ipp>
#include <cppwamp/internal/error.ipp>
#include <cppwamp/internal/legacytcpconnector.ipp>
#include <cppwamp/internal/messagetraits.ipp>
#include <cppwamp/internal/registration.ipp>
#include <cppwamp/internal/session.ipp>
#include <cppwamp/internal/sessiondata.ipp>
#include <cppwamp/internal/subscription.ipp>
#include <cppwamp/internal/tcpconnector.ipp>
#include <cppwamp/internal/version.ipp>

#if CPPWAMP_HAS_UNIX_DOMAIN_SOCKETS
#include <cppwamp/udsconnector.hpp>
#include <cppwamp/legacyudsconnector.hpp>

#include <cppwamp/internal/udsconnector.ipp>
#include <cppwamp/internal/legacyudsconnector.ipp>
#endif

#endif
