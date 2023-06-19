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

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <type_traits>
#include "api.hpp"
#include "asiodefs.hpp"
#include "erroror.hpp"
#include "transport.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Primary template, specialized for each transport protocol tag. */
//------------------------------------------------------------------------------
template <typename TProtocol>
class Listener {};

//------------------------------------------------------------------------------
/** Interface for establishing client transport endpoints.
    A concrete Connecting instance is used to establish a transport connection
    from a client to a router. Once the connection is established, the connector
    creates a concrete wamp::Transporting for use by wamp::Session. */
//------------------------------------------------------------------------------
class CPPWAMP_API Listening : public std::enable_shared_from_this<Listening>
{
public:
    /// Shared pointer to a Connecting
    using Ptr = std::shared_ptr<Listening>;

    /** Asynchronous handler function type called by Listening::establish. */
    using Handler = std::function<void (ErrorOr<Transporting::Ptr>)>;

    /** Destructor. */
    virtual ~Listening() = default;

    /** Starts establishing the transport connection, emitting a
        Transportable::Ptr via the given handler if successful. */
    virtual void establish(Handler&& handler) = 0;

    /** Cancels a transport connection in progress.
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
public:
    using CodecIds = std::set<int>;

    /** Constructor taking transport settings (e.g. TcpEndpoint) */
    template <typename S>
    explicit ListenerBuilder(S&& transportSettings)
        : builder_(makeBuilder(std::forward<S>(transportSettings)))
    {}

    /** Builds a connector appropriate for the transport settings given
        in the constructor. */
    Listening::Ptr operator()(IoStrand s, CodecIds codecIds) const
    {
        return builder_(std::move(s), std::move(codecIds));
    }

private:
    using Function = std::function<Listening::Ptr (IoStrand s, CodecIds)>;

    template <typename S>
    static Function makeBuilder(S&& transportSettings)
    {
        using Settings = typename std::decay<S>::type;
        using Protocol = typename Settings::Protocol;
        using ConcreteListener = Listener<Protocol>;
        return Function{
            [transportSettings](IoStrand s, CodecIds codecIds)
            {
                return Listening::Ptr(new ConcreteListener(
                    std::move(s), transportSettings, std::move(codecIds)));
            }};
    }

    Function builder_;
};

} // namespace wamp

#endif // CPPWAMP_LISTENER_HPP
