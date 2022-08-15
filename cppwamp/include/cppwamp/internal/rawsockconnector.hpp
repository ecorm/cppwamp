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
#include "../codec.hpp"
#include "../connector.hpp"
#include "../error.hpp"
#include "client.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TEndpoint>
class RawsockConnector : public Connector
{
public:
    using Endpoint = TEndpoint;
    using Info = typename Endpoint::Establisher::Info;
    using Transport = typename Endpoint::Transport;
    using Ptr = std::shared_ptr<RawsockConnector>;

    static Ptr create(const AnyIoExecutor& e, BufferCodecBuilder b, Info i)
    {
        using std::move;
        return Ptr(new RawsockConnector(IoStrand(e), move(b), move(i)));
    }

    IoStrand strand() const override {return strand_;}

protected:
    using Establisher = typename Endpoint::Establisher;

    virtual Connector::Ptr clone() const override
    {
        return Connector::Ptr(new RawsockConnector(strand_, codecBuilder_,
                                                   info_));
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
                if (!ec)
                {
                    assert(codecId == me.codecBuilder_.id());
                    client = internal::Client::create(me.codecBuilder_(),
                                                      std::move(trnsp));
                }
                boost::asio::post(me.strand_,
                                  std::bind(std::move(handler), ec, client));
                me.endpoint_.reset();
            }
        };

        CPPWAMP_LOGIC_CHECK(!endpoint_, "Connection already in progress");
        endpoint_.reset(new Endpoint( Establisher(strand_, info_),
                                      codecBuilder_.id(),
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
    RawsockConnector(IoStrand s, BufferCodecBuilder b, Info i)
        : strand_(std::move(s)),
          codecBuilder_(b),
          info_(std::move(i))
    {}

    boost::asio::strand<AnyIoExecutor> strand_;
    BufferCodecBuilder codecBuilder_;
    Info info_;
    std::unique_ptr<Endpoint> endpoint_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_RAWSOCKCONNECTOR_HPP
