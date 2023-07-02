/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2022-2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_CONNECTIONINFO_HPP
#define CPPWAMP_CONNECTIONINFO_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for reporting session connection information. */
//------------------------------------------------------------------------------

#include <cstdint>
#include <memory>
#include "api.hpp"
#include "variant.hpp"
#include "internal/passkey.hpp"

namespace wamp
{

namespace internal {class ConnectionInfoImpl;}

//------------------------------------------------------------------------------
/** Contains connection information associated with a WAMP client session.

    This is a reference-counted lightweight proxy to the actual object
    containing the information. */
//------------------------------------------------------------------------------
class CPPWAMP_API ConnectionInfo
{
public:
    using ServerSessionNumber = uint64_t;

    /** Default constructor. */
    ConnectionInfo();

    ConnectionInfo(Object transport, std::string endpoint);

    const Object& transport() const;

    const std::string& endpoint() const;

    const std::string& server() const;

    ServerSessionNumber serverSessionNumber() const;

    /** Returns true if this proxy object points to an actual
        information object. */
    explicit operator bool() const;

private:
    std::shared_ptr<internal::ConnectionInfoImpl> impl_;

public: // Internal use only
    ConnectionInfo(internal::PassKey,
                   std::shared_ptr<internal::ConnectionInfoImpl> impl);

    void setServer(internal::PassKey, std::string server,
                   ServerSessionNumber n);
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/connectioninfo.inl.hpp"
#endif

#endif // CPPWAMP_CONNECTIONINFO_HPP