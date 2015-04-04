/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_UDSCONNECTOR_HPP
#define CPPWAMP_UDSCONNECTOR_HPP

//------------------------------------------------------------------------------
/** @file
    Contains the declaration of the UdsConnector class. */
//------------------------------------------------------------------------------

#include <memory>
#include <string>
#include "asiodefs.hpp"
#include "client.hpp"
#include "codec.hpp"
#include "error.hpp"
#include "rawsockdefs.hpp"

namespace wamp
{

// Forward declarations
namespace internal
{
    template <typename TEstablisher> class AsioConnector;
    class UdsOpener;
}

//------------------------------------------------------------------------------
/** Establishes a client connection over a Unix domain raw socket.
    @see Connector
    @see TcpConnector
    @see legacy::TcpConnector
    @see legacy::UdsConnector */
//------------------------------------------------------------------------------
class UdsConnector : public Connector
{
public:
    /// Shared pointer to a UdsConnector
    using Ptr = std::shared_ptr<UdsConnector>;

    /** Creates a new UdsConnector instance. */
    static Ptr create(
        AsioService& iosvc,       /**< Service used for asyncronous I/O. */
        const std::string& path,  /**< Path name of the Unix domain socket. */
        CodecId codecId,          /**< The serializer to use. */
        RawsockMaxLength maxRxLength = RawsockMaxLength::kB_64
            /**< The maximum length of incoming WAMP messages. */
    );

protected:
    virtual Connector::Ptr clone() const override;

    virtual void establish(Handler handler) override;

    virtual void cancel() override;

private:
    using Impl = internal::AsioConnector<internal::UdsOpener>;

    // Connection details to pass on to internal::UdsOpener.
    struct Info
    {
        AsioService& iosvc;
        std::string path;
        CodecId codecId;
        RawsockMaxLength maxRxLength;
    };

    UdsConnector(Info info);

    Ptr shared_from_this();

    std::unique_ptr<Impl> impl_;
    Info info_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/udsconnector.ipp"
#endif

#endif // CPPWAMP_UDSCONNECTOR_HPP
