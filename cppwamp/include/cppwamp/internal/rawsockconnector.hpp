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
#include "../error.hpp"
#include "client.hpp"

namespace wamp
{

namespace internal
{

//------------------------------------------------------------------------------
template <typename TEndpoint>
class RawsockConnector
    : public std::enable_shared_from_this<RawsockConnector<TEndpoint>>
{
public:
    using Endpoint = TEndpoint;
    using Establisher = typename Endpoint::Establisher;
    using Info = typename Endpoint::Establisher::Info;
    using Transport = typename Endpoint::Transport;
    using Ptr = std::shared_ptr<RawsockConnector>;

    using Handler =
        std::function<void (std::error_code,
                            std::shared_ptr<internal::ClientInterface>)>;

    static Ptr create(IoStrand s, Info i, BufferCodecBuilder b)
    {
        using std::move;
        return Ptr(new RawsockConnector(move(s), move(i), move(b)));
    }

    const IoStrand& strand() const {return strand_;}

    const Info& info() const {return info_;}

    const BufferCodecBuilder& codecBuilder() const {return codecBuilder_;}

    void establish(Handler&& handler)
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
        endpoint_->establish(Established{this->shared_from_this(),
                                         std::move(handler)});
    }

    void cancel()
    {
        if (endpoint_)
            endpoint_->cancel();
    }

private:
    RawsockConnector(IoStrand s, Info i, BufferCodecBuilder b)
        : strand_(std::move(s)),
          info_(std::move(i)),
          codecBuilder_(std::move(b))
    {}

    boost::asio::strand<AnyIoExecutor> strand_;
    Info info_;
    BufferCodecBuilder codecBuilder_;
    std::unique_ptr<Endpoint> endpoint_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_RAWSOCKCONNECTOR_HPP
