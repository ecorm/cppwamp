/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_INTERNAL_TRANSPORT_HPP
#define CPPWAMP_INTERNAL_TRANSPORT_HPP

#include <functional>
#include <memory>
#include <system_error>
#include "../asiodefs.hpp"
#include "../messagebuffer.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
// Abstract base class for transports.
//------------------------------------------------------------------------------
class TransportBase : public std::enable_shared_from_this<TransportBase>
{
public:
    using Ptr         = std::shared_ptr<TransportBase>;
    using RxHandler   = std::function<void (MessageBuffer)>;
    using FailHandler = std::function<void (std::error_code ec)>;
    using PingHandler = std::function<void (float)>;

    // Noncopyable
    TransportBase(const TransportBase&) = delete;
    TransportBase& operator=(const TransportBase&) = delete;

    virtual ~TransportBase() {}

    const IoStrand& strand() const {return strand_;};

    size_t maxSendLength() const {return maxTxLength_;}

    size_t maxReceiveLength() const {return maxRxLength_;}

    virtual bool isOpen() const = 0;

    virtual bool isStarted() const = 0;

    virtual void start(RxHandler rxHandler, FailHandler failHandler) = 0;

    virtual void send(MessageBuffer message) = 0;

    virtual void close() = 0;

    virtual void ping(MessageBuffer message, PingHandler handler) = 0;

protected:
    TransportBase(IoStrand s, size_t maxSendLength, size_t maxReceiveLength)
        : strand_(std::move(s)),
          maxTxLength_(maxSendLength),
          maxRxLength_(maxReceiveLength)
    {}

    template <typename F, typename... Ts>
    void post(F&& handler, Ts&&... args)
    {
        boost::asio::post(strand_, std::bind(std::forward<F>(handler),
                                             std::forward<Ts>(args)...));
    }

private:
    IoStrand strand_;
    size_t maxTxLength_;
    size_t maxRxLength_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_INTERNAL_TRANSPORT_HPP
