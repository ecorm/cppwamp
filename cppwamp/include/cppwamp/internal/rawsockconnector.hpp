/*------------------------------------------------------------------------------
              Copyright Butterfly Energy Systems 2014-2015, 2022.
           Distributed under the Boost Software License, Version 1.0.
              (See accompanying file LICENSE_1_0.txt or copy at
                    http://www.boost.org/LICENSE_1_0.txt)
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_RAWSOCKCONNECTOR_HPP
#define CPPWAMP_RAWSOCKCONNECTOR_HPP

#include <cassert>
#include <memory>
#include <string>
#include <utility>
#include <boost/asio/post.hpp>
#include "../asiodefs.hpp"
#include "../connector.hpp"
#include "../error.hpp"
#include "client.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TCodec, typename TEndpoint>
class RawsockConnector : public Connector
{
public:
    using Codec = TCodec;
    using Endpoint = TEndpoint;
    using Info = typename Endpoint::Establisher::Info;
    using Transport = typename Endpoint::Transport;
    using Ptr = std::shared_ptr<RawsockConnector>;

    static Ptr create(AnyExecutor exec, Info info)
    {
        return Ptr(new RawsockConnector(exec, std::move(info)));
    }

protected:
    using Establisher = typename Endpoint::Establisher;

    virtual Connector::Ptr clone() const override
    {
        return Connector::Ptr(new RawsockConnector(executor_, info_));
    }

    virtual void establish(Handler handler) override
    {
        CPPWAMP_LOGIC_CHECK(!endpoint_, "Connection already in progress");
        endpoint_.reset(new Endpoint( Establisher(executor_, info_),
                                      Codec::id(),
                                      info_.maxRxLength() ));

        auto self = shared_from_this();
        endpoint_->establish(
            [this, self, handler](std::error_code ec, int codecId,
                                  typename Transport::Ptr trnsp)
        {
            internal::ClientInterface::Ptr client;
            using ClientType = internal::Client<Codec, Transport>;
            if (!ec)
            {
                assert(codecId == Codec::id());
                client = ClientType::create(std::move(trnsp));
            }
            boost::asio::post(executor_, std::bind(handler, ec, client));
            endpoint_.reset();
        });
    }

    virtual void cancel() override
    {
        if (endpoint_)
            endpoint_->cancel();
    }

private:
    RawsockConnector(AnyExecutor exec, Info info)
        : executor_(exec),
          info_(std::move(info))
    {}

    AnyExecutor executor_;
    Info info_;
    std::unique_ptr<Endpoint> endpoint_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_RAWSOCKCONNECTOR_HPP
