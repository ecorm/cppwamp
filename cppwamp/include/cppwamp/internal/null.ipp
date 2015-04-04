/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

namespace wamp
{

inline bool Null::operator==(Null) const {return true;}

inline bool Null::operator!=(Null) const {return false;}

inline bool Null::operator<(Null)  const {return false;}

inline std::ostream& operator<<(std::ostream& out, Null) {return out << "null";}

} // namespace wamp
