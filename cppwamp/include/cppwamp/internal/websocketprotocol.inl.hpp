/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2023.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#include "../transports/websocketprotocol.hpp"
#include <array>
#include <boost/beast/websocket/option.hpp>
#include "../api.hpp"
#include "../version.hpp"

namespace wamp
{

//******************************************************************************
// WebsocketCloseCategory
//******************************************************************************

CPPWAMP_INLINE const char* WebsocketCloseCategory::name() const noexcept
{
    return "wamp::WebsocketCloseCategory";
}

CPPWAMP_INLINE std::string WebsocketCloseCategory::message(int ev) const
{
    static const std::array<const char*, 14> msg{
    {
/* normal         = 1000 */ "Websocket connection successfully fulfilled its purpose",
/* goingAway      = 1001 */ "Websocket peer is navigating away or going down",
/* protocolError  = 1002 */ "Websocket protocol error",
/* unknownData    = 1003 */ "Websocket peer cannot accept data type",
/*                  1004 */ "",
/*                  1005 */ "",
/*                  1006 */ "",
/* badPayload     = 1007 */ "Invalid websocket message data type",
/* policyError    = 1008 */ "Websocket peer received a message violating its policy",
/* tooBig         = 1009 */ "Websocket peer received a message too big to process",
/* needsExtension = 1010 */ "Websocket server lacks extension expected by client",
/* internalError  = 1011 */ "Websocket server encountered an unexpected condition",
/* serviceRestart = 1012 */ "Websocket server is restarting",
/* tryAgainLater  = 1013 */ "Websocket connection terminated due to temporary server condition"
    }};

    if (ev == 1)
        return "Websocket connection closed abnormally for unknown reason";

    if (ev < 1000 || ev > 1013)
        return {};
    return msg.at(ev - 1000);
}

CPPWAMP_INLINE bool WebsocketCloseCategory::equivalent(
    const std::error_code& code, int condition) const noexcept
{
    return (code.category() == websocketCloseCategory()) &&
           (code.value() == condition);
}

CPPWAMP_INLINE WebsocketCloseCategory::WebsocketCloseCategory() = default;

CPPWAMP_INLINE WebsocketCloseCategory& websocketCloseCategory()
{
    static WebsocketCloseCategory instance;
    return instance;
}

CPPWAMP_INLINE std::error_code make_error_code(WebsocketCloseErrc errc)
{
    return {static_cast<int>(errc), websocketCloseCategory()};
}

CPPWAMP_INLINE std::error_condition
make_error_condition(WebsocketCloseErrc errc)
{
    return {static_cast<int>(errc), websocketCloseCategory()};
}


//******************************************************************************
// WebsocketPermessageDeflate
//******************************************************************************

struct WebsocketPermessageDeflate::Defaults
{
    static const boost::beast::websocket::permessage_deflate& get()
    {
        static const boost::beast::websocket::permessage_deflate instance;
        return instance;
    }
};

CPPWAMP_INLINE
WebsocketPermessageDeflate::WebsocketPermessageDeflate(bool enabled)
    : threshold_(Defaults::get().msg_size_threshold),
      maxWindowBits_(Defaults::get().client_max_window_bits),
      compressionLevel_(Defaults::get().compLevel),
      memoryLevel_(Defaults::get().memLevel)
{}

CPPWAMP_INLINE WebsocketPermessageDeflate&
WebsocketPermessageDeflate::withMaxWindowBits(int bits)
{
    maxWindowBits_ = bits;
    return *this;
}

CPPWAMP_INLINE WebsocketPermessageDeflate&
WebsocketPermessageDeflate::withoutContextTakeover(bool without)
{
    noContextTakeover_ = without;
    return *this;
}

CPPWAMP_INLINE WebsocketPermessageDeflate&
WebsocketPermessageDeflate::withCompressionLevel(int level)
{
    compressionLevel_ = level;
    return *this;
}

CPPWAMP_INLINE WebsocketPermessageDeflate&
WebsocketPermessageDeflate::withMemoryLevel(int level)
{
    memoryLevel_ = level;
    return *this;
}

CPPWAMP_INLINE WebsocketPermessageDeflate&
WebsocketPermessageDeflate::withThreshold(std::size_t threshold)
{
    threshold_ = threshold;
    return *this;
}

CPPWAMP_INLINE bool WebsocketPermessageDeflate::enabled() const
{
    return enabled_;
}

CPPWAMP_INLINE int WebsocketPermessageDeflate::maxWindowBits() const
{
    return maxWindowBits_;
}

CPPWAMP_INLINE bool WebsocketPermessageDeflate::noContextTakeover() const
{
    return noContextTakeover_;
}

CPPWAMP_INLINE int WebsocketPermessageDeflate::compressionLevel() const
{
    return compressionLevel_;
}

CPPWAMP_INLINE int WebsocketPermessageDeflate::memoryLevel() const
{
    return memoryLevel_;
}

CPPWAMP_INLINE std::size_t WebsocketPermessageDeflate::threshold() const
{
    return threshold_;
}


//******************************************************************************
// WebsocketOptions
//******************************************************************************

CPPWAMP_INLINE WebsocketOptions& WebsocketOptions::withAgent(std::string agent)
{
    agent_ = std::move(agent);
    return *this;
}

/** @details
    Sets the [boost::beast::websocket::stream::write_buffer_bytes]
    (https://www.boost.org/doc/libs/release/libs/beast/doc/html/beast/ref/boost__beast__websocket__stream/write_buffer_bytes.html) option. */
CPPWAMP_INLINE WebsocketOptions&
WebsocketOptions::withWriteBufferSize(std::size_t bytes)
{
    writeBufferSize_ = bytes;
    return *this;
}

/** @details
    Sets the [boost::beast::websocket::stream::auto_fragment]
    (https://www.boost.org/doc/libs/release/libs/beast/doc/html/beast/ref/boost__beast__websocket__stream/auto_fragment.html) option. */
CPPWAMP_INLINE WebsocketOptions&
WebsocketOptions::withAutoFragment(bool enabled)
{
    autoFragment_ = enabled;
    return *this;
}

CPPWAMP_INLINE WebsocketOptions&
WebsocketOptions::withPermessageDeflate(WebsocketPermessageDeflate options)
{
    permessageDeflate_ = options;
    return *this;
}

CPPWAMP_INLINE const std::string& WebsocketOptions::agent() const
{
    return agent_;
}

CPPWAMP_INLINE std::size_t WebsocketOptions::writeBufferSize() const
{
    return writeBufferSize_;
}

CPPWAMP_INLINE bool WebsocketOptions::autoFragment() const
{
    return autoFragment_;
}

CPPWAMP_INLINE const WebsocketPermessageDeflate&
WebsocketOptions::permessageDeflate() const
{
    return permessageDeflate_;
}


//******************************************************************************
// WebsocketClientLimits
//******************************************************************************

CPPWAMP_INLINE WebsocketClientLimits&
WebsocketClientLimits::withHeaderSize(std::size_t n)
{
    headerSize_ = n;
    return *this;
}

CPPWAMP_INLINE std::size_t WebsocketClientLimits::headerSize() const
{
    return headerSize_;
}


//******************************************************************************
// WebsocketHost
//******************************************************************************

CPPWAMP_INLINE WebsocketHost::WebsocketHost(std::string address,
                                            std::string serviceName)
    : Base(std::move(address), std::move(serviceName))
{
    options_.withAgent(Version::clientAgentString());
}

CPPWAMP_INLINE WebsocketHost::WebsocketHost(std::string address, Port port)
    : WebsocketHost(std::move(address), std::to_string(port))
{
    options_.withAgent(Version::clientAgentString());
}

CPPWAMP_INLINE WebsocketHost& WebsocketHost::withTarget(std::string target)
{
    // TODO: Validate target path
    target_ = std::move(target);
    return *this;
}

CPPWAMP_INLINE WebsocketHost&
WebsocketHost::withOptions(WebsocketOptions options)
{
    options_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE const std::string& WebsocketHost::target() const
{
    return target_;
}

CPPWAMP_INLINE const WebsocketOptions& WebsocketHost::options() const
{
    return options_;
}


//******************************************************************************
// WebsocketServerLimits
//******************************************************************************

CPPWAMP_INLINE WebsocketServerLimits&
WebsocketServerLimits::withHeaderSize(std::size_t n)
{
    headerSize_ = n;
    return *this;
}

CPPWAMP_INLINE std::size_t WebsocketServerLimits::headerSize() const
{
    return headerSize_;
}


//******************************************************************************
// WebsocketEndpoint
//******************************************************************************

CPPWAMP_INLINE WebsocketEndpoint::WebsocketEndpoint(Port port)
    : Base("", port)
{
    options_.withAgent(Version::serverAgentString());
    mutableAcceptorOptions().withReuseAddress(true);
}

CPPWAMP_INLINE WebsocketEndpoint::WebsocketEndpoint(std::string address,
                                                    unsigned short port)
    : Base(std::move(address), port)
{
    options_.withAgent(Version::serverAgentString());
    mutableAcceptorOptions().withReuseAddress(true);
}

CPPWAMP_INLINE WebsocketEndpoint&
WebsocketEndpoint::withOptions(WebsocketOptions options)
{
    options_ = std::move(options);
    return *this;
}

CPPWAMP_INLINE const WebsocketOptions& WebsocketEndpoint::options() const
{
    return options_;
}

CPPWAMP_INLINE std::string WebsocketEndpoint::label() const
{
    auto portString = std::to_string(port());
    if (address().empty())
        return "Websocket Port " + portString;
    return "Websocket " + address() + ':' + portString;
}

CPPWAMP_INLINE void WebsocketEndpoint::initialize(internal::PassKey) {}

} // namespace wamp
