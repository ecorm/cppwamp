/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include <boost/asio/local/stream_protocol.hpp>

namespace wamp
{

UdsPath::UdsPath(std::string pathName) : pathName_(pathName) {}

const std::string& UdsPath::pathName() const {return pathName_;}

} // namespace wamp
