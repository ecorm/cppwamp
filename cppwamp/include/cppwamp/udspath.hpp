/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_UDSPATH_HPP
#define CPPWAMP_UDSPATH_HPP

//------------------------------------------------------------------------------
/** @file
    @brief Contains facilities for specifying Unix domain socket
           transport parameters and options. */
//------------------------------------------------------------------------------

#include <string>
#include "api.hpp"
#include "connector.hpp"
#include "rawsockoptions.hpp"
#include "udsprotocol.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
/** Contains a Unix Domain Socket path, as well as other socket options.
    Meets the requirements of @ref TransportSettings.
    @see ConnectionWish */
//------------------------------------------------------------------------------
class CPPWAMP_API UdsPath
{
public:
    /// Transport protocol tag associated with these settings.
    using Protocol = Uds;

    /// The default maximum length permitted for incoming messages.
    static constexpr RawsockMaxLength defaultMaxRxLength =
        RawsockMaxLength::MB_16;

    /** Converting constructor taking a path name. */
    UdsPath(
        std::string pathName,        ///< Path name of the Unix domain socket.
        UdsOptions options = {},     ///< Socket options.
        RawsockMaxLength maxRxLength
            = defaultMaxRxLength,    ///< Maximum inbound message length.
        bool deletePath = true       ///< Delete existing path before listening.
    );

    /** Specifies the socket options to use. */
    UdsPath& withOptions(UdsOptions options);

    /** Specifies the maximum length permitted for incoming messages. */
    UdsPath& withMaxRxLength(RawsockMaxLength length);

    /** Enables/disables the deletion of existing file path before listening. */
    UdsPath& withDeletePath(bool enabled = true);

    /** Couples a serialization format with these transport settings to
        produce a ConnectionWish that can be passed to Session::connect. */
    template <typename TFormat>
    ConnectionWish withFormat(TFormat) const
    {
        return ConnectionWish{*this, TFormat{}};
    }

    /** Obtains the path name. */
    const std::string& pathName() const;

    /** Obtains the transport options. */
    const UdsOptions& options() const;

    /** Obtains the specified maximum incoming message length. */
    RawsockMaxLength maxRxLength() const;

    /** Returns true if path deletion before listening is enabled. */
    bool deletePathEnabled() const;

private:
    std::string pathName_;
    UdsOptions options_;
    RawsockMaxLength maxRxLength_;
    bool deletePathEnabled_;
};

} // namespace wamp

#ifndef CPPWAMP_COMPILED_LIB
#include "./internal/udspath.ipp"
#endif

#endif // CPPWAMP_UDSPATH_HPP
