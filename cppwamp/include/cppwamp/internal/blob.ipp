/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#include "config.hpp"
#include "base64.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE Blob::Blob() {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Blob::Blob(Data data) : data_(std::move(data)) {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Blob::Blob(std::initializer_list<uint8_t> list) : data_(list) {}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Blob::Data& Blob::data() {return data_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const Blob::Data& Blob::data() const {return data_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool Blob::operator==(const Blob& other) const
    {return data_ == other.data_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool Blob::operator!=(const Blob& other) const
    {return data_ != other.data_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE bool Blob::operator<(const Blob& other) const
    {return data_ < other.data_;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE std::ostream& operator<<(std::ostream& out, const Blob& blob)
{
    internal::Base64::encode(blob.data(), out);
    return out;
}

} // namespace wamp
