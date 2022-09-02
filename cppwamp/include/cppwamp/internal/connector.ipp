/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../connector.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connecting::Ptr ConnectorBuilder::operator()(IoStrand s,
                                                            int codecId) const
{
    return builder_(std::move(s), codecId);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE int ConnectionWish::codecId() const {return codecBuilder_.id();}

CPPWAMP_INLINE Connecting::Ptr ConnectionWish::makeConnector(IoStrand s) const
{
    return connectorBuilder_(std::move(s), codecId());
}

CPPWAMP_INLINE AnyBufferCodec ConnectionWish::makeCodec() const
{
    return codecBuilder_();
}

} // namespace wamp
