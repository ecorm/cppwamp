/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_LISTENER_HPP
#define CPPWAMP_LISTENER_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for type-erasing the method of establishing
           a router-side transport. */
//------------------------------------------------------------------------------

#include <cassert>
#include <functional>
#include <memory>
#include <set>
#include <utility>

#include "api.hpp"
#include "asiodefs.hpp"
#include "codec.hpp"
#include "routerlogger.hpp"
#include "transport.hpp"
#include "traits.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Primary template, specialized for each transport protocol tag. */
//------------------------------------------------------------------------------
template <typename TProtocol>
class Listener {};

//------------------------------------------------------------------------------
/** Classifies Listening::establish errors. */
//------------------------------------------------------------------------------
enum class ListenStatus
{
    success,    /// No error
    cancelled,  /// Server cancellation
    transient,  /// Transient error that doesn't need delay before recovering
    overload,   /// Out of memory or resources
    outage,     /// Network down
    fatal       /// Programming error
};

//------------------------------------------------------------------------------
/** Contains the outcome of a listening attempt. */
//------------------------------------------------------------------------------
class ListenResult
{
public:
    using Status = ListenStatus;

    /** Default constructor indicating success. */
    ListenResult() = default;

    /** Constructor taking information on a failed listen attempt. */
    ListenResult(std::error_code e, Status s, const char* operation)
        : error_(e),
          operation_(operation),
          status_(s)
    {}

    /** Determines if the listen attempt was successful. */
    bool ok() const {return status_ == Status::success;}

    /** Obtains the status of the listen attempt. */
    Status status() const {return status_;}

    /** Obtains the error code if the listen attempt failed. */
    std::error_code error() const {return error_;}

    /** Obtains the name of the socket (or other) operation that failed,
        for logging purposes.
        @pre `!this->ok()` */
    const char* operation() const
    {
        assert(!ok());
        return operation_;
    }

private:
    std::error_code error_;
    const char* operation_ = nullptr;
    Status status_ = Status::success;
};

//------------------------------------------------------------------------------
/** Interface for establishing router transport endpoints.
    A concrete Listening instance is used to establish a transport connection
    from a router to a client. Once the connection is established, the connector
    creates a concrete wamp::Transporting for use by a router. */
//------------------------------------------------------------------------------
class CPPWAMP_API Listening : public std::enable_shared_from_this<Listening>
{
public:
    /// Shared pointer to a Connecting
    using Ptr = std::shared_ptr<Listening>;

    /** Handler function type called when a listen attempt succeeds or fails. */
    using Handler = std::function<void (ListenResult)>;

    /** Destructor. */
    virtual ~Listening() = default;

    /** Registers the handler to invoke when a listen attempt succeeds
        or fails. */
    virtual void observe(Handler handler) = 0;

    /** Starts accepting a client connection, emitting a ListenResult to the
        observer upon success or failure. */
    virtual void establish() = 0;

    /** Creates a new transport using the connected client socket. */
    virtual Transporting::Ptr take() = 0;

    /** Drops the connected client socket due to connection limits. */
    virtual void drop() = 0;

    /** Cancels transport establishment in progress.
        A TransportErrc::aborted error code will be returned via the
        Listening::establish asynchronous handler. */
    virtual void cancel() = 0;

    /** Obtains a human-friedly string indicating the address/port/path where
        the transport is to be established. */
    const std::string& where() const {return where_;}

protected:
    explicit Listening(std::string where) : where_(std::move(where)) {}

private:
    std::string where_;
};

//------------------------------------------------------------------------------
class ListenerBuilder
{
private:
    template <typename S>
    static constexpr bool isNotSelf()
    {
        return !isSameType<ValueTypeOf<S>, ListenerBuilder>();
    }

public:
    /** Constructor taking transport settings (e.g. TcpEndpoint) */
    template <typename S, CPPWAMP_NEEDS((isNotSelf<S>())) = 0>
    explicit ListenerBuilder(S&& transportSettings)
        : builder_(makeBuilder(std::forward<S>(transportSettings)))
    {}

    /** Builds a listener appropriate for the transport settings given
        in the constructor. */
    Listening::Ptr operator()(AnyIoExecutor e, IoStrand s, CodecIdSet c,
                              RouterLogger::Ptr l) const
    {
        return builder_(std::move(e), std::move(s), std::move(c), std::move(l));
    }

private:
    using Function =
        std::function<Listening::Ptr (AnyIoExecutor, IoStrand, CodecIdSet,
                                      RouterLogger::Ptr)>;

    template <typename S>
    static Function makeBuilder(S&& settings)
    {
        using Settings = Decay<S>;
        using Protocol = typename Settings::Protocol;
        using ConcreteListener = Listener<Protocol>;

        return Function{
            [settings](AnyIoExecutor e, IoStrand s, CodecIdSet c,
                       RouterLogger::Ptr l)
            {
                return Listening::Ptr(new ConcreteListener(
                    std::move(e), std::move(s), settings, std::move(c),
                    std::move(l)));
            }};
    }

    Function builder_;
};

} // namespace wamp

#endif // CPPWAMP_LISTENER_HPP
