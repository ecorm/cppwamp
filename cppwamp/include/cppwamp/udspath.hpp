/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_UDSPATH_HPP
#define CPPWAMP_UDSPATH_HPP

//------------------------------------------------------------------------------
/** @file
    Contains facilities for specifying Unix domain socket transport parameters
    and options. */
//------------------------------------------------------------------------------

#include <string>
#include "rawsockoptions.hpp"

// Forward declaration
namespace boost { namespace asio { namespace local {
class stream_protocol;
}}}

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains a Unix domain socket path, as well as other socket options.
    @see RawsockOptions, connector, legacyConnector */
//------------------------------------------------------------------------------
class UdsPath : public RawsockOptions<UdsPath,
                                      boost::asio::local::stream_protocol>
{
public:
    /** Converting constructor taking a path name. */
    UdsPath(
        std::string pathName /**< Path name of the Unix domain socket. */
    );

    /** Obtains the path name. */
    const std::string& pathName() const;

private:
    using Base = RawsockOptions<UdsPath, boost::asio::local::stream_protocol>;

    std::string pathName_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "./internal/udspath.ipp"
#endif

#endif // CPPWAMP_UDSPATH_HPP
