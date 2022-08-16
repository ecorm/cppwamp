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
#include "../erroror.hpp"
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
    // TODO: Merge with AsioEndpoint

public:
    using Endpoint = TEndpoint;
    using Establisher = typename Endpoint::Establisher;
    using Info = typename Endpoint::Establisher::Info;
    using Transport = typename Endpoint::Transport;
    using Ptr = std::shared_ptr<RawsockConnector>;

    using Handler = std::function<void (ErrorOr<Transporting::Ptr>)>;

    static Ptr create(IoStrand s, Info i, int codecId)
    {
        return Ptr(new RawsockConnector(std::move(s), std::move(i), codecId));
    }

    void establish(Handler&& handler)
    {
        struct Established
        {
            Ptr self;
            Handler handler;

            void operator()(ErrorOr<Transporting::Ptr> transport)
            {
                auto& me = *self;
                boost::asio::post(me.strand_,
                                  std::bind(std::move(handler),
                                            std::move(transport)));
                me.endpoint_.reset();
            }
        };

        CPPWAMP_LOGIC_CHECK(!endpoint_, "Connection already in progress");
        endpoint_.reset(new Endpoint( Establisher(strand_, info_),
                                      codecId_, info_.maxRxLength() ));
        endpoint_->establish(Established{this->shared_from_this(),
                                         std::move(handler)});
    }

    void cancel()
    {
        if (endpoint_)
            endpoint_->cancel();
    }

private:
    RawsockConnector(IoStrand s, Info i, int codecId)
        : strand_(std::move(s)),
          info_(std::move(i)),
          codecId_(codecId)
    {}

    boost::asio::strand<AnyIoExecutor> strand_;
    Info info_;
    int codecId_;
    std::unique_ptr<Endpoint> endpoint_;
};

} // namespace internal

} // namespace wamp

#endif // CPPWAMP_RAWSOCKCONNECTOR_HPP
