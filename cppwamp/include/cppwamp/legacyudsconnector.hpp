/*------------------------------------------------------------------------------
                Copyright Butterfly Energy Systems 2014-2015.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_LEGACYUDSCONNECTOR_HPP
#define CPPWAMP_LEGACYUDSCONNECTOR_HPP

//------------------------------------------------------------------------------
/** @file
    Contains the declaration of the legacy::UdsConnector class. */
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
    class UdsOpener;
}

namespace legacy
{

//------------------------------------------------------------------------------
/** Establishes a client connection, over a Unix domain raw socket, to
    non-conformant routers.
    @see Connector
    @see TcpConnector
    @see UdsConnector
    @see legacy::TcpConnector */
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
            /**< The maximum length of incoming  abd outgoing WAMP messages. */
    );

protected:
    virtual Connector::Ptr clone() const override;

    virtual void establish(Handler handler) override;

    virtual void cancel() override;

private:
    using Impl = internal::LegacyAsioEndpoint<internal::UdsOpener>;

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

} // namespace legacy

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "internal/legacyudsconnector.ipp"
#endif

#endif // CPPWAMP_LEGACYUDSCONNECTOR_HPP
