/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_UDS_HPP
#define CPPWAMP_UDS_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for creating Unix domain socket
           transport connectors. */
//------------------------------------------------------------------------------

#include <memory>
#include <utility>
#include "api.hpp"
#include "asiodefs.hpp"
#include "connector.hpp"
#include "listener.hpp"
#include "udspath.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Connector specialization that establishes a Unix domain socket transport.
    Users do not need to use this class directly and should use
    ConnectionWish instead. */
//------------------------------------------------------------------------------
template <>
class CPPWAMP_API Connector<Uds> : public Connecting
{
public:
    /** Type containing the transport settings. */
    using Settings = UdsPath;

    /** Constructor. */
    Connector(IoStrand i, Settings s, int codecId);

    /** Destructor. */
    ~Connector();

    /** Starts establishing the transport connection, emitting a
        Transportable::Ptr via the given handler if successful. */
    void establish(Handler&& handler) override;

    /** Cancels transport connection in progress, emitting an error code
        via the handler passed to the establish method. */
    void cancel() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

//------------------------------------------------------------------------------
/** Listener specialization that establishes a server-side TCP transport.
    Users do not need to use this class directly and should use
    ConnectionWish instead. */
//------------------------------------------------------------------------------
template <>
class CPPWAMP_API Listener<Uds> : public Listening
{
public:
    /** Type containing the transport settings. */
    using Settings = UdsPath;

    /** Collection type used for codec IDs. */
    using CodecIds = std::set<int>;

    /** Constructor. */
    Listener(IoStrand i, Settings s, CodecIds codecIds);

    /** Destructor. */
    ~Listener();

    void establish(Handler&& handler) override;

    void cancel() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/uds.inl.hpp"
#endif

#endif // CPPWAMP_UDS_HPP
