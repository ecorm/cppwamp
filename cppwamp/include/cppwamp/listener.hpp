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

#include <cerrno>
#include <functional>
#include <map>
#include <memory>
#include <set>

#if defined(_WIN32) || defined(__CYGWIN__)
#include <Winsock2.h>
#endif

#include "api.hpp"
#include "asiodefs.hpp"
#include "exceptions.hpp"
#include "transport.hpp"
#include "traits.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Categories for classifying Listening::establish errors. */
//------------------------------------------------------------------------------
enum class ListeningErrorCategory
{
    success,    /// No error
    fatal,      /// Due to programming error
    transient,  /// Transient error that doesn't need delay before recovering
    congestion, /// Out of memory or resources
    outage      /// Network down
};

//------------------------------------------------------------------------------
/** Provides functions that help in classifying socket operation errors. */
//------------------------------------------------------------------------------
struct SocketErrorHelper
{
    static bool isAcceptCongestionError(boost::system::error_code ec)
    {
        namespace sys = boost::system;
        return ec == std::errc::no_buffer_space
            || ec == std::errc::not_enough_memory
            || ec == std::errc::too_many_files_open
            || ec == std::errc::too_many_files_open_in_system
#if defined(__linux__)
            || ec == sys::error_code{ENOSR, sys::system_category()}
#endif
            ;
    }

    static bool isAcceptOutageError(boost::system::error_code ec)
    {
        namespace sys = boost::system;
#if defined(__linux__)
        return ec == std::errc::network_down
            || ec == std::errc::network_unreachable
            || ec == std::errc::no_protocol_option // "Protocol not available"
            || ec == std::errc::operation_not_permitted // Denied by firewall
            || ec == sys::error_code{ENONET, sys::system_category()};
#elif defined(_WIN32) || defined(__CYGWIN__)
        return ec == std::errc::network_down;
#else
        return false;
#endif
    }

    static bool isAcceptTransientError(boost::system::error_code ec)
    {
        // Asio already takes care of EAGAIN, EWOULDBLOCK, ECONNABORTED,
        // EPROTO, and EINTR.
        namespace sys = boost::system;
#if defined(__linux__)
        return ec == std::errc::host_unreachable
            || ec == std::errc::operation_not_supported
            || ec == std::errc::timed_out
            || ec == sys::error_code{EHOSTDOWN, sys::system_category()};
#elif defined(_WIN32) || defined(__CYGWIN__)
        return ec == std::errc::connection_refused
            || ec == std::errc::connection_reset
            || ec == sys::error_code{WSATRY_AGAIN, sys::system_category()});
#else
        return false;
#endif
    }

    static bool isAcceptFatalError(boost::system::error_code ec)
    {
        return ec == boost::asio::error::already_open
            || ec == std::errc::bad_file_descriptor
            || ec == std::errc::not_a_socket
            || ec == std::errc::invalid_argument
#if !defined(__linux__)
            || ec == std::errc::operation_not_supported
#endif
#if defined(BSD) || defined(__APPLE__)
            || ec == std::errc::bad_address // EFAULT
#elif defined(_WIN32) || defined(__CYGWIN__)
            || ec == std::errc::bad_address // EFAULT
            || ec == std::errc::permission_denied
            || ec == sys::error_code{WSANOTINITIALISED, sys::system_category()}
#endif
            ;
    }

    static bool isReceiveFatalError(boost::system::error_code ec)
    {
        return ec == std::errc::bad_address // EFAULT
            || ec == std::errc::bad_file_descriptor
            || ec == std::errc::invalid_argument
            || ec == std::errc::message_size
            || ec == std::errc::not_a_socket
            || ec == std::errc::not_connected
            || ec == std::errc::operation_not_supported
#if defined(_WIN32) || defined(__CYGWIN__)
            || ec == sys::error_code{WSANOTINITIALISED, sys::system_category()}
#endif
            ;
    }

    static bool isSendFatalError(boost::system::error_code ec)
    {
        return isReceiveFatalError(ec)
            || ec == std::errc::already_connected
            || ec == std::errc::connection_already_in_progress
            || ec == std::errc::permission_denied;
    }
};

//------------------------------------------------------------------------------
/** Primary template, specialized for each transport protocol tag. */
//------------------------------------------------------------------------------
template <typename TProtocol>
class Listener {};

//------------------------------------------------------------------------------
/** Contains the outcome of a listening attempt. */
//------------------------------------------------------------------------------
class ListenResult
{
public:
    /** Default constructor. */
    ListenResult() = default;

    /** Constructor taking a transport ready for use. */
    ListenResult(Transporting::Ptr t) : transport_(std::move(t)) {}

    /** Constructor taking information on a failed listen attempt. */
    ListenResult(std::error_code e, ListeningErrorCategory c,
                 const char* operation)
        : error_(e),
          operation_(operation),
          category_(c)
    {}

    /** Determines if the listen attempt was successful. */
    bool ok() const {return transport_ != nullptr;}

    /** Obtains the new transport instance if the listen attempt
        was successful. */
    const Transporting::Ptr transport() const
    {
        CPPWAMP_LOGIC_CHECK(ok(), "No transport available");
        return transport_;
    }

    /** Obtains the error code if the listen attempt failed. */
    std::error_code error() const {return error_;}

    /** Obtains the error category if the listen attempt failed. */
    ListeningErrorCategory errorCategory() const {return category_;}

    /** Obtains the name of the socket (or other) operation that failed,
        for logging purposes.*/
    const char* operation() const {return operation_;}

private:
    Transporting::Ptr transport_;
    std::error_code error_;
    const char* operation_ = nullptr;
    ListeningErrorCategory category_ = ListeningErrorCategory::fatal;
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

    /** Starts establishing the transport connection, emitting a
        ListenResult to the observer upon success or failure. */
    virtual void establish() = 0;

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
    /** Container type used to store a set of codec IDs. */
    using CodecIds = std::set<int>;

    /** Constructor taking transport settings (e.g. TcpEndpoint) */
    template <typename S, CPPWAMP_NEEDS((isNotSelf<S>())) = 0>
    explicit ListenerBuilder(S&& transportSettings)
        : builder_(makeBuilder(std::forward<S>(transportSettings)))
    {}

    /** Builds a listener appropriate for the transport settings given
        in the constructor. */
    Listening::Ptr operator()(AnyIoExecutor e, IoStrand s,
                              CodecIds codecIds) const
    {
        return builder_(std::move(e), std::move(s), std::move(codecIds));
    }

private:
    using Function =
        std::function<Listening::Ptr (AnyIoExecutor e, IoStrand s, CodecIds)>;

    template <typename S>
    static Function makeBuilder(S&& transportSettings)
    {
        using Settings = Decay<S>;
        using Protocol = typename Settings::Protocol;
        using ConcreteListener = Listener<Protocol>;
        return Function{
            [transportSettings](AnyIoExecutor e, IoStrand s, CodecIds codecIds)
            {
                return Listening::Ptr(new ConcreteListener(
                    std::move(e), std::move(s), transportSettings,
                    std::move(codecIds)));
            }};
    }

    Function builder_;
};

} // namespace wamp

#endif // CPPWAMP_LISTENER_HPP
