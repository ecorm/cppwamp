/*------------------------------------------------------------------------------
    Copyright Butterfly Energy Systems 2014-2015, 2022.
    Distributed under the Boost Software License, Version 1.0.
    http://www.boost.org/LICENSE_1_0.txt
------------------------------------------------------------------------------*/

#ifndef CPPWAMP_RAWSOCKCONNECTOR_HPP
#define CPPWAMP_RAWSOCKCONNECTOR_HPP

#include <cassert>
#include <memory>
#include <string>
#include <utility>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
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

    static Ptr create(const AnyIoExecutor& exec, Info info)
    {
        return Ptr(new RawsockConnector(IoStrand(exec), std::move(info)));
    }

    IoStrand strand() const override {return strand_;}

protected:
    using Establisher = typename Endpoint::Establisher;

    virtual Connector::Ptr clone() const override
    {
        return Connector::Ptr(new RawsockConnector(strand_, info_));
    }

    virtual void establish(Handler&& handler) override
    {
        struct Established
        {
            Ptr self;
            Handler handler;

            void operator()(std::error_code ec, int codecId,
                            typename Transport::Ptr trnsp)
            {
                auto& me = *self;
                internal::ClientInterface::Ptr client;
                using ClientType = internal::Client<Codec, Transport>;
                if (!ec)
                {
                    assert(codecId == Codec::id());
                    client = ClientType::create(std::move(trnsp));
                }
                boost::asio::post(me.strand_,
                                  std::bind(std::move(handler), ec, client));
                me.endpoint_.reset();
            }
        };

        CPPWAMP_LOGIC_CHECK(!endpoint_, "Connection already in progress");
        endpoint_.reset(new Endpoint( Establisher(strand_, info_),
                                      Codec::id(),
                                      info_.maxRxLength() ));
        auto self =
            std::static_pointer_cast<RawsockConnector>(shared_from_this());
        endpoint_->establish(Established{std::move(self), std::move(handler)});
    }

    virtual void cancel() override
    {
        if (endpoint_)
            endpoint_->cancel();
    }

private:
    RawsockConnector(IoStrand strand, Info info)
        : strand_(std::move(strand)),
          info_(std::move(info))
    {}

    boost::asio::strand<AnyIoExecutor> strand_;
    Info info_;
    std::unique_ptr<Endpoint> endpoint_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_RAWSOCKCONNECTOR_HPP
