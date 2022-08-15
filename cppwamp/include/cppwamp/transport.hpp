/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_TRANSPORT_HPP
#define CPPWAMP_TRANSPORT_HPP

#include <functional>
#include <memory>
#include <system_error>
#include "asiodefs.hpp"
#include "messagebuffer.hpp"

namespace wamp
{

//------------------------------------------------------------------------------
struct TransportLimits
{
    std::size_t maxTxLength;
    std::size_t maxRxLength;
};

//------------------------------------------------------------------------------
// Interface class for transports.
//------------------------------------------------------------------------------
class Transporting : public std::enable_shared_from_this<Transporting>
{
public:
    using Ptr         = std::shared_ptr<Transporting>;
    using RxHandler   = std::function<void (MessageBuffer)>;
    using FailHandler = std::function<void (std::error_code ec)>;
    using PingHandler = std::function<void (float)>;

    Transporting() = default;

    // Noncopyable
    Transporting(const Transporting&) = delete;
    Transporting& operator=(const Transporting&) = delete;

    virtual ~Transporting() {}

    virtual IoStrand strand() const = 0;

    virtual TransportLimits limits() const = 0;

    virtual bool isOpen() const = 0;

    virtual bool isStarted() const = 0;

    virtual void start(RxHandler rxHandler, FailHandler failHandler) = 0;

    virtual void send(MessageBuffer message) = 0;

    virtual void close() = 0;

    virtual void ping(MessageBuffer message, PingHandler handler) = 0;
};

} // namespace wamp

#endif // CPPWAMP_TRANSPORT_HPP
