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

#include <chrono>
#include <functional>
#include <memory>
#include <system_error>
#include <type_traits>
#include <vector>
#include "api.hpp"
#include "asiodefs.hpp"
#include "codec.hpp"
#include "erroror.hpp"
#include "timeout.hpp"
#include "transport.hpp"
#include "traits.hpp"

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
    virtual ~Connecting() = default;

    /** Starts establishing a transport connection. */
    virtual void establish(Handler handler) = 0;

    /** Cancels a transport connection in progress.
        A TransportErrc::aborted error code will be returned via the
        Connecting::establish asynchronous handler. */
    virtual void cancel() = 0;
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
    template <typename TTransportSettings,
             typename TCodecFormat,
             CPPWAMP_NEEDS(IsCodecFormat<TCodecFormat>::value) = 0>
    ConnectionWish(TTransportSettings&& wish, TCodecFormat)
        : connectorBuilder_(std::forward<TTransportSettings>(wish)),
          codecBuilder_(TCodecFormat{})
    {}

    /** Constructor taking a @ref TransportSettings instance and a
        @ref CodecOptions instance. */
    template <typename TTransportSettings, typename TFormat>
    ConnectionWish(TTransportSettings&& wish,
                   const CodecOptions<TFormat>& codecOptions)
        : connectorBuilder_(std::forward<TTransportSettings>(wish)),
          codecBuilder_(codecOptions)
    {}

    /** Specifies the connection timeout duration.
        @throws error::Logic if the given timeout duration is negative. */
    ConnectionWish& withTimeout(Timeout timeout)
    {
        timeout_ = internal::checkTimeout(timeout);
        return *this;
    }

    /** Obtains the connection timeout duration. */
    Timeout timeout() const {return timeout_;}

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
    class Builder
    {
    private:
        template <typename S>
        static constexpr bool isNotSelf()
        {
            return !isSameType<ValueTypeOf<S>, Builder>();
        }

    public:
        template <typename S, CPPWAMP_NEEDS((isNotSelf<S>())) = 0>
        explicit Builder(S&& transportSettings)
            : builder_(makeBuilder(std::forward<S>(transportSettings)))
        {}

        Connecting::Ptr operator()(IoStrand s, int codecId) const
        {
            return builder_(std::move(s), codecId);
        }

    private:
        using Function =
            std::function<Connecting::Ptr (IoStrand s, int codecId)>;

        template <typename S>
        static Function makeBuilder(S&& transportSettings)
        {
            using Settings = Decay<S>;
            using Protocol = typename Settings::Protocol;
            using ConcreteConnector = Connector<Protocol>;
            return Function{
                [transportSettings](IoStrand s, int codecId)
                {
                    auto cnct = std::make_shared<ConcreteConnector>(
                        std::move(s), transportSettings, codecId);
                    return std::static_pointer_cast<Connecting>(cnct);
                }};
        }

        Function builder_;
    };

    Builder connectorBuilder_;
    BufferCodecBuilder codecBuilder_;
    Timeout timeout_ = unspecifiedTimeout;
};

//------------------------------------------------------------------------------
/** List desired transport/codec couplings. */
//------------------------------------------------------------------------------
using ConnectionWishList = std::vector<ConnectionWish>;

} // namespace wamp

#endif // CPPWAMP_CONNECTOR_HPP
