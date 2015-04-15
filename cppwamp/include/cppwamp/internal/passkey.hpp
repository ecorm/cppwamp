/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_PASSKEY_HPP
#define CPPWAMP_PASSKEY_HPP

namespace wamp
{

namespace internal
{
    class PassKey
    {
        PassKey() {}

        template <typename, typename> friend class Dialogue;
        template <typename, typename> friend class Client;
    };
}

}

#endif // CPPWAMP_PASSKEY_HPP
