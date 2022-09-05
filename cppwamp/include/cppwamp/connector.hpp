/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CONNECTOR_HPP
#define CPPWAMP_CONNECTOR_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for type-erasing the method of establishing
           a transport. */
//------------------------------------------------------------------------------

#include <functional>
#include <memory>
#include <system_error>
#include <type_traits>
#include <vector>
#include "api.hpp"
#include "asiodefs.hpp"
#include "codec.hpp"
#include "erroror.hpp"
#include "transport.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Primary template, specialized for each transport protocol tag. */
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
/** Builds a transport connector on demand when needed. */
//------------------------------------------------------------------------------
class CPPWAMP_API ConnectorBuilder
{
public:
    /** Constructor taking transport settings (e.g. TcpHost) */
    template <typename S>
    explicit ConnectorBuilder(S&& transportSettings)
        : builder_(makeBuilder(std::forward<S>(transportSettings)))
    {}

    /** Builds a connector appropriate for the transport settings given
        in the constructor. */
    Connecting::Ptr operator()(IoStrand s, int codecId) const
    {
        return builder_(std::move(s), codecId);
    }

private:
    using Function = std::function<Connecting::Ptr (IoStrand s, int codecId)>;

    template <typename S>
    static Function makeBuilder(S&& transportSettings)
    {
        using Settings = typename std::decay<S>::type;
        using Protocol = typename Settings::Protocol;
        using ConcreteConnector = Connector<Protocol>;
        return Function{
            [transportSettings](IoStrand s, int codecId)
            {
                return Connecting::Ptr(new ConcreteConnector(
                    std::move(s), transportSettings, codecId));
            }};
    }

    Function builder_;
};

//------------------------------------------------------------------------------
/** Couples desired transport settings together with a desired serialization
    format, to allow the generation of connectors and codecs on demand. */
//------------------------------------------------------------------------------
class ConnectionWish
{
public:
    /** Constructor taking a @ref TransportSettings instance and a
        @ref CodecFormat instance. */
    template <typename TTransportSettings, typename TCodecFormat>
    ConnectionWish(TTransportSettings&& wish, TCodecFormat)
        : connectorBuilder_(std::forward<TTransportSettings>(wish)),
          codecBuilder_(TCodecFormat{})
    {}

    /** Obtains the numeric codec ID of the desired serialization format. */
    int codecId() const {return codecBuilder_.id();}

    /** Generates a `Connector` for the desired transport. */
    Connecting::Ptr makeConnector(IoStrand s) const
    {
        return connectorBuilder_(std::move(s), codecId());
    }

    /** Generates a codec for the desired serialization format. */
    AnyBufferCodec makeCodec() const
    {
        return codecBuilder_();
    }

private:
    ConnectorBuilder connectorBuilder_;
    BufferCodecBuilder codecBuilder_;
};

//------------------------------------------------------------------------------
/** List desired transport/codec couplings. */
//------------------------------------------------------------------------------
using ConnectionWishList = std::vector<ConnectionWish>;

} // namespace wamp

#endif // CPPWAMP_CONNECTOR_HPP
