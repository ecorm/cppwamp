/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TCPCONNECTOR_HPP
#define CPPWAMP_TCPCONNECTOR_HPP

//------------------------------------------------------------------------------
/** @file
    Contains the declaration of the TcpConnector class. */
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
    template <typename TEstablisher> class AsioConnector;
    class TcpOpener;
}

//------------------------------------------------------------------------------
/** Establishes a client connection over TCP raw socket.
    @see Connector
    @see UdsConnector
    @see legacy::TcpConnector
    @see legacy::UdsConnector */
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
            /**< The maximum length of incoming WAMP messages. */
    );

    /** Creates a new TcpConnector instance.
        This overload takes the port number as a 16-bit integer. */
    static Ptr create(
        AsioService& iosvc,          /**< Service used for asyncronous I/O. */
        const std::string& hostName, /**< URL or IP of the router to connect to. */
        unsigned short port,         /**< Port number. */
        CodecId codecId,             /**< The serializer to use. */
        RawsockMaxLength maxRxLength = RawsockMaxLength::kB_64
           /**< The maximum length of incoming WAMP messages. */
    );

protected:
    virtual Connector::Ptr clone() const override;

    virtual void establish(Handler handler) override;

    virtual void cancel() override;

private:
    using Impl = internal::AsioConnector<internal::TcpOpener>;

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

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/tcpconnector.ipp"
#endif

#endif // CPPWAMP_TCPCONNECTOR_HPP
