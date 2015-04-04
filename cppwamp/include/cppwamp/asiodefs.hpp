/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_ASIODEFS_HPP
#define CPPWAMP_ASIODEFS_HPP

//------------------------------------------------------------------------------
/** @file
    Common type definitions used by transports that rely on Boost.Asio. */
//------------------------------------------------------------------------------

#include <boost/asio/io_service.hpp>
#include <boost/system/error_code.hpp>

namespace wamp
{

/** The program's link to the operating system's I/O services. */
using AsioService = boost::asio::io_service;

/** Type used by Boost.Asio for reporting errors. */
using AsioErrorCode = boost::system::error_code;

} // namespace wamp

#endif // CPPWAMP_ASIODEFS_HPP
