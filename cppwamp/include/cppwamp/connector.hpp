/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CONNECTOR_HPP
#define CPPWAMP_CONNECTOR_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains the declaration of the Connecting abstract base class. */
//------------------------------------------------------------------------------

#include <functional>
#include <memory>
#include <system_error>
#include <vector>
#include "api.hpp"
#include "asiodefs.hpp"
#include "codec.hpp"
#include "erroror.hpp"
#include "transport.hpp"

namespace wamp
{

// Forward declaration
namespace internal {class ClientInterface;}

//------------------------------------------------------------------------------
template <typename TProtocol>
class Connector {};

//------------------------------------------------------------------------------
/** Interface for establishing client transport endpoints.
    A concrete Connecting instance is used to establish a transport connection
    from a client to a router. Once the connection is established, the connector
    creates a concrete wamp::Transporting for use by wamp::Session. */
//------------------------------------------------------------------------------
class CPPWAMP_API Connecting : public std::enable_shared_from_this<Connecting>
{
public:
    /// Shared pointer to a Connecting
    using Ptr = std::shared_ptr<Connecting>;

    /** Asynchronous handler function type called by Connecting::establish. */
    using Handler = std::function<void (ErrorOr<Transporting::Ptr>)>;

    /** Destructor. */
    virtual ~Connecting() {}

    /** Starts establishing a transport connection. */
    virtual void establish(Handler&& handler) = 0;

    /** Cancels a transport connection in progress.
        A TransportErrc::aborted error code will be returned via the
        Connecting::establish asynchronous handler. */
    virtual void cancel() = 0;
};


//------------------------------------------------------------------------------
class CPPWAMP_API ConnectorBuilder
{
public:
    template <typename S>
    explicit ConnectorBuilder(S&& transportSettings)
        : builder_(makeBuilder(std::forward<S>(transportSettings)))
    {}

    Connecting::Ptr operator()(IoStrand s, int codecId) const
    {
        return builder_(std::move(s), codecId);
    }

private:
    using Function = std::function<Connecting::Ptr (IoStrand s, int codecId)>;

    template <typename S>
    static Function makeBuilder(S&& transportSettings)
    {
        using Protocol = typename S::Protocol;
        using ConcreteConnector = Connector<Protocol>;
        return Function{
            [transportSettings](IoStrand s, int codecId) mutable
            {
                return ConcreteConnector::create(
                    std::move(s), std::move(transportSettings), codecId);
            }};
    }

    Function builder_;
};

//------------------------------------------------------------------------------
class CPPWAMP_API LegacyConnector
{
public:
    template <typename S, typename TFormat>
    LegacyConnector(AnyIoExecutor exec, S&& settings, TFormat)
        : exec_(std::move(exec)),
          connectorBuilder_(std::forward<S>(settings)),
          codecBuilder_(TFormat{})
    {}

    const AnyIoExecutor& executor() const {return exec_;}

    const ConnectorBuilder& connectorBuilder() const {return connectorBuilder_;}

    const BufferCodecBuilder& codecBuilder() const {return codecBuilder_;}

private:
    AnyIoExecutor exec_;
    ConnectorBuilder connectorBuilder_;
    BufferCodecBuilder codecBuilder_;
};

//------------------------------------------------------------------------------
/** List of Connecting objects to use when attempting connection. */
//------------------------------------------------------------------------------
using ConnectorList = std::vector<LegacyConnector>;

//------------------------------------------------------------------------------
class ConnectionWish
{
public:
    template <typename W, typename TFormat>
    ConnectionWish(W&& wish, TFormat)
        : connectorBuilder_(std::forward<W>(wish)),
          codecBuilder_(TFormat{})
    {}

    explicit ConnectionWish(const LegacyConnector& c)
        : connectorBuilder_(c.connectorBuilder()),
          codecBuilder_(c.codecBuilder())
    {}

    int codecId() const {return codecBuilder_.id();}

    Connecting::Ptr makeConnector(IoStrand s) const
    {
        return connectorBuilder_(std::move(s), codecId());
    }

    AnyBufferCodec makeCodec() const {return codecBuilder_();}

private:
    ConnectorBuilder connectorBuilder_;
    BufferCodecBuilder codecBuilder_;
};

//------------------------------------------------------------------------------
using ConnectionWishList = std::vector<ConnectionWish>;

} // namespace wamp

#endif // CPPWAMP_CONNECTOR_HPP
