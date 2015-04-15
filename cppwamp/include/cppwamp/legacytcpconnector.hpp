/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_LEGACYTCPCONNECTOR_HPP
#define CPPWAMP_LEGACYTCPCONNECTOR_HPP

//------------------------------------------------------------------------------
/** @file
    Contains the declaration of the legacy::TcpConnector class. */
//------------------------------------------------------------------------------

#include <memory>
#include <string>
#include "asiodefs.hpp"
#include "codec.hpp"
#include "connector.hpp"
#include "error.hpp"
#include "rawsockdefs.hpp"

namespace wamp
{

// Forward declarations
namespace internal
{
    template <typename TEstablisher> class LegacyAsioEndpoint;
    class TcpOpener;
}

namespace legacy
{

//------------------------------------------------------------------------------
/** Establishes a client connection, over TCP raw socket, to non-conformant
    routers.
    This is an interim Connector for connecting to routers that do not yet
    support handshaking on their raw socket transports. Handshaking was
    introduced in [version e2c4e57][e2c4e57] of the advanced WAMP specification.
    [e2c4e57]: https://github.com/tavendo/WAMP/commit/e2c4e5775d89fa6d991eb2e138e2f42ca2469fa8
    @see Connector
    @see UdsConnector */
//------------------------------------------------------------------------------
class TcpConnector : public Connector
{
public:
    /// Shared pointer to a TcpConnector
    using Ptr = std::shared_ptr<TcpConnector>;

    /** Creates a new TcpConnector instance. */
    static Ptr create(
        AsioService& iosvc,             /**< Service used for asyncronous I/O. */
        const std::string& hostName,    /**< URL or IP of the router to connect to. */
        const std::string& serviceName, /**< Port number or service name. */
        CodecId codecId,                /**< The serializer to use. */
        RawsockMaxLength maxRxLength = RawsockMaxLength::kB_64
            /**< The maximum length of incoming and outgoing WAMP messages. */
    );

    /** Creates a new TcpConnector instance.
        This overload takes the port number as a 16-bit integer. */
    static Ptr create(
        AsioService& iosvc,          /**< Service used for asyncronous I/O. */
        const std::string& hostName, /**< URL or IP of the router to connect to. */
        unsigned short port,         /**< Port number. */
        CodecId codecId,             /**< The serializer to use. */
        RawsockMaxLength maxRxLength = RawsockMaxLength::kB_64
            /**< The maximum length of incoming and outgoing WAMP messages. */
    );

protected:
    virtual Connector::Ptr clone() const override;

    virtual void establish(Handler handler) override;

    virtual void cancel() override;

private:
    using Impl = internal::LegacyAsioEndpoint<internal::TcpOpener>;

    // Connection details to pass on to internal::TcpOpener.
    struct Info
    {
        AsioService& iosvc;
        std::string hostName;
        std::string serviceName;
        CodecId codecId;
        RawsockMaxLength maxRxLength;
    };

    TcpConnector(Info info);

    Ptr shared_from_this();

    std::unique_ptr<Impl> impl_;
    Info info_;
};

} // namespace legacy

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/legacytcpconnector.ipp"
#endif

#endif // CPPWAMP_LEGACYTCPCONNECTOR_HPP
