/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../udspath.hpp"
#include <utility>
#include "../api.hpp"

namespace wamp
{

CPPWAMP_INLINE UdsPath::UdsPath(std::string pathName, UdsOptions options,
                                RawsockMaxLength maxRxLength,
                                bool deletePath)
    : pathName_(std::move(pathName)),
      options_(std::move(options)),
      maxRxLength_(maxRxLength),
      deletePathEnabled_(deletePath)
{}

CPPWAMP_INLINE UdsPath& UdsPath::withOptions(UdsOptions options)
{
    options_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE UdsPath& UdsPath::withMaxRxLength(RawsockMaxLength length)
{
    maxRxLength_ = length;
    return *this;
}

CPPWAMP_INLINE const std::string& UdsPath::pathName() const {return pathName_;}

CPPWAMP_INLINE const UdsOptions& UdsPath::options() const {return options_;}

CPPWAMP_INLINE RawsockMaxLength UdsPath::maxRxLength() const
{
    return maxRxLength_;
}

CPPWAMP_INLINE bool UdsPath::deletePathEnabled() const
{
    return deletePathEnabled_;
}

CPPWAMP_INLINE std::string UdsPath::label() const
{
    return "Unix domain socket path '" + pathName_ + "'";
}

} // namespace wamp
