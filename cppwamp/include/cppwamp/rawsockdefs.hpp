/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_RAWSOCKDEFS_HPP
#define CPPWAMP_RAWSOCKDEFS_HPP

//------------------------------------------------------------------------------
/** @file
    Contains public constants and definitions used only for raw socket
    transports. */
//------------------------------------------------------------------------------

namespace wamp
{

//------------------------------------------------------------------------------
/** Enumerators used to establish the maximum length of messages that a raw
    socket transport can receive. */
//------------------------------------------------------------------------------
enum class RawsockMaxLength
{
    B_512,  ///< 512 bytes
    kB_1,   ///< 1 kilobyte
    kB_2,   ///< 2 kilobytes
    kB_4,   ///< 4 kilobytes
    kB_8,   ///< 8 kilobytes
    kB_16,  ///< 16 kilobytes
    kB_32,  ///< 32 kilobytes
    kB_64,  ///< 64 kilobytes
    kB_128, ///< 128 kilobytes
    kB_256, ///< 256 kilobytes
    kB_512, ///< 512 kilobytes
    MB_1,   ///< 1 megabyte
    MB_2,   ///< 2 megabytes
    MB_4,   ///< 4 megabytes
    MB_8,   ///< 8 megabytes
    MB_16   ///< 16 megabytes
};

} // namespace wamp

#endif // CPPWAMP_RAWSOCKDEFS_HPP
