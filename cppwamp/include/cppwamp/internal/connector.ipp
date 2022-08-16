/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../connector.hpp"

namespace wamp
{

// Forward declaration
namespace internal {class ClientInterface;}

//------------------------------------------------------------------------------
CPPWAMP_INLINE Connecting::Ptr ConnectorBuilder::operator()(IoStrand s,
                                                            int codecId) const
{
    return builder_(std::move(s), codecId);
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE const AnyIoExecutor& LegacyConnector::executor() const
{
    return exec_;
}

CPPWAMP_INLINE const ConnectorBuilder& LegacyConnector::connectorBuilder() const
{
    return connectorBuilder_;
}

CPPWAMP_INLINE const BufferCodecBuilder& LegacyConnector::codecBuilder() const
{
    return codecBuilder_;
}

//------------------------------------------------------------------------------
CPPWAMP_INLINE ConnectionWish::ConnectionWish(const LegacyConnector& c)
    : connectorBuilder_(c.connectorBuilder()),
      codecBuilder_(c.codecBuilder())
{}

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
